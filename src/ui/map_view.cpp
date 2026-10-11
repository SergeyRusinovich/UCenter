#include "ui/map_view.h"

#include "ui/map_buttons.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

using geo::TileSize;

namespace {

int wrapTile(int x, int n)
{
    return ((x % n) + n) % n;
}

}  // namespace

MapView::MapView(QWidget *parent, bool needFix, const TileSource &source)
    : QWidget(parent)
    , m_needFix(needFix)
    , m_zoom(needFix ? FixZoom : MinZoom)
{
    setMinimumSize(320, 260);
    resetTiles(source);
    auto *zoomIn = makeButton(QStringLiteral("+"), [this] { setZoom(m_zoom + 1); });
    auto *zoomOut = makeButton(QStringLiteral("−"), [this] { setZoom(m_zoom - 1); });
    m_btnCenter = makeButton<CrosshairButton>(QString(), &MapView::recenter);
    m_btnCenter->setToolTip(tr("Center on the current position"));
    m_buttons = {zoomIn, zoomOut, m_btnCenter};
}

// ---------- Публичный API ----------
void MapView::setView(LatLon center, int zoom)
{
    m_center = {std::clamp(center.lat, -geo::MaxLat, geo::MaxLat), geo::wrap360(center.lon + 180.0) - 180.0};
    m_zoom = std::clamp(zoom, int(MinZoom), int(MaxZoom));
    update();
}

void MapView::setPosition(std::optional<double> lat, std::optional<double> lon)
{
    if (!lat || !lon)
        return;
    const LatLon pos{*lat, *lon};
    if (m_pos == pos)
        return;
    if (!m_pos && m_follow)
        m_zoom = std::max(m_zoom, int(FixZoom));
    m_pos = pos;
    if (m_follow)
        m_center = pos;
    update();
}

void MapView::setSource(const TileSource &source)
{
    if (source == m_tiles->source())
        return;
    m_tiles->deleteLater();  // вместе с ним прерываются его незавершённые запросы
    resetTiles(source);
    update();
}

void MapView::resetTiles(const TileSource &source)
{
    m_tiles = new TileLoader(source, this);
    connect(m_tiles, &TileLoader::tileLoaded, this, qOverload<>(&QWidget::update));
}

void MapView::centerOn(double lat, double lon)
{
    m_follow = false;
    m_center = {lat, lon};
    update();
}

void MapView::fitBounds(const Bounds &b, int maxZoom)
{
    m_follow = false;
    m_center = {(b.south + b.north) / 2, (b.west + b.east) / 2};
    const double w = width() * 0.8, h = height() * 0.8;
    int z = maxZoom;
    for (; z > MinZoom; --z) {
        const QPointF p0 = geo::llToPx(b.north, b.west, z);
        const QPointF p1 = geo::llToPx(b.south, b.east, z);
        if (p1.x() - p0.x() <= w && p1.y() - p0.y() <= h)
            break;
    }
    m_zoom = z;
    update();
}

Bounds MapView::visibleBounds() const
{
    const LatLon nw = toGeo(QPointF(0, 0));
    const LatLon se = toGeo(QPointF(width(), height()));
    return {se.lat, nw.lon, nw.lat, se.lon};
}

QPointF MapView::toScreen(double lat, double lon) const
{
    return geo::llToPx(lat, lon, m_zoom) - origin();
}

LatLon MapView::toGeo(const QPointF &pt) const
{
    const QPointF world = origin() + pt;
    const LatLon ll = geo::pxToLl(world.x(), world.y(), m_zoom);
    return {ll.lat, geo::wrap360(ll.lon + 180.0) - 180.0};
}

QPointF MapView::origin() const
{
    return geo::llToPx(m_center.lat, m_center.lon, m_zoom) - QPointF(width() / 2.0, height() / 2.0);
}

// ---------- Управление ----------
void MapView::setZoom(int z, std::optional<QPointF> anchor)
{
    z = std::clamp(z, int(MinZoom), int(MaxZoom));
    if (z == m_zoom)
        return;
    const QPointF middle(width() / 2.0, height() / 2.0);
    const QPointF at = anchor.value_or(middle);
    const LatLon under = toGeo(at);
    m_zoom = z;
    if ((at - middle).manhattanLength() > ClickSlop) {
        // Точка под курсором остаётся под курсором
        const QPointF world = geo::llToPx(under.lat, under.lon, z) - at + middle;
        m_center = geo::pxToLl(world.x(), world.y(), z);
        m_follow = false;
    }
    update();
}

void MapView::recenter()
{
    m_follow = true;
    if (m_pos)
        m_center = *m_pos;
    update();
    emit recentered();
}

void MapView::resizeEvent(QResizeEvent *)
{
    const int x = width() - 36;
    for (int i = 0; i < m_buttons.size(); ++i)
        m_buttons[i]->move(x, 8 + i * 32);
}

void MapView::wheelEvent(QWheelEvent *e)
{
    const QPoint d = e->angleDelta();
    if (std::abs(d.y()) <= std::abs(d.x())) {
        e->ignore();  // горизонтальная прокрутка масштаб не меняет
        return;
    }
    if ((m_wheelAccum > 0) != (d.y() > 0))
        m_wheelAccum = 0;  // сменилось направление
    m_wheelAccum += d.y();
    const int steps = m_wheelAccum / WheelStep;
    if (steps == 0)
        return;
    m_wheelAccum -= steps * WheelStep;
    setZoom(m_zoom + steps, e->position());
}

void MapView::mousePressEvent(QMouseEvent *e)
{
    m_dragLast = m_pressAt = e->position();
}

void MapView::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_dragLast || !ready())
        return;
    if (m_pressAt) {
        if ((e->position() - *m_pressAt).manhattanLength() < ClickSlop)
            return;
        m_pressAt.reset();  // началось перетаскивание
    }
    const QPointF d = e->position() - *m_dragLast;
    m_dragLast = e->position();
    const QPointF c = geo::llToPx(m_center.lat, m_center.lon, m_zoom);
    m_center = geo::pxToLl(c.x() - d.x(), c.y() - d.y(), m_zoom);
    m_follow = false;
    update();
}

void MapView::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_pressAt && ready() && e->button() == Qt::LeftButton) {
        const LatLon ll = toGeo(e->position());
        emit clicked(ll.lat, ll.lon);
    }
    m_dragLast.reset();
    m_pressAt.reset();
}

// ---------- Отрисовка ----------
void MapView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().alternateBase());
    if (!ready()) {
        p.setPen(palette().placeholderText().color());
        p.drawText(rect(), Qt::AlignCenter, tr("Waiting for position fix…"));
        return;
    }
    paintTiles(p);
    if (m_pos) {
        p.setPen(QPen(Qt::white, 3));
        p.setBrush(QColor("#e02020"));
        p.drawEllipse(toScreen(*m_pos), 8, 8);
    }
    p.save();
    paintOverlay(p);
    p.restore();

    QFont f = p.font();
    f.setPointSize(8);
    p.setFont(f);
    const QString text = m_tiles->source().attribution;
    const int tw = p.fontMetrics().horizontalAdvance(text) + 8;
    const QRectF box(width() - tw, height() - 16, tw, 16);
    p.fillRect(box, QColor(255, 255, 255, 190));
    p.setPen(QColor("#333"));
    p.drawText(box, Qt::AlignCenter, text);
}

void MapView::paintTiles(QPainter &p)
{
    const int z = m_zoom;
    const QPointF o = origin();
    const int n = 1 << z;
    const int tx0 = int(std::floor(o.x() / TileSize)), tx1 = int(std::floor((o.x() + width()) / TileSize));
    const int ty0 = int(std::max(0.0, std::floor(o.y() / TileSize)));
    const int ty1 = int(std::min(double(n - 1), std::floor((o.y() + height()) / TileSize)));

    // Сначала тайлы у центра: их видно первыми, остальные догрузятся следом
    QList<QPoint> order;
    for (int tx = tx0; tx <= tx1; ++tx)
        for (int ty = ty0; ty <= ty1; ++ty)
            order << QPoint(tx, ty);
    const QPointF mid = (o + QPointF(width() / 2.0, height() / 2.0)) / TileSize - QPointF(0.5, 0.5);
    std::sort(order.begin(), order.end(), [mid](const QPoint &a, const QPoint &b) {
        return QPointF(QPointF(a) - mid).manhattanLength() < QPointF(QPointF(b) - mid).manhattanLength();
    });

    // На Retina — тайлы следующего уровня: четыре на место одного, в полном разрешении экрана
    const TileSource &src = m_tiles->source();
    const bool hiDpi = devicePixelRatioF() >= 1.5 && src.hiDpi && z + 1 <= src.maxZoom;
    QSet<QString> wanted;
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const QPoint &t : std::as_const(order)) {
        const QRectF target(t.x() * TileSize - o.x(), t.y() * TileSize - o.y(), TileSize, TileSize);
        const int x = wrapTile(t.x(), n), y = t.y();
        if (!hiDpi) {
            if (!drawTile(p, target, z, x, y, &wanted))
                drawFallback(p, target, z, x, y);
            continue;
        }
        // Под дочерними — сам тайл из кэша или растянутый предок
        if (const QPixmap whole = m_tiles->cached(z, x, y); !whole.isNull())
            p.drawPixmap(target, whole, QRectF(whole.rect()));
        else
            drawFallback(p, target, z, x, y);
        const QSizeF half(TileSize / 2.0, TileSize / 2.0);
        for (int dy = 0; dy < 2; ++dy)
            for (int dx = 0; dx < 2; ++dx)
                drawTile(p, QRectF(target.topLeft() + QPointF(dx * half.width(), dy * half.height()), half), z + 1,
                         2 * x + dx, 2 * y + dy, &wanted);
    }
    m_tiles->retain(wanted);
}

bool MapView::drawTile(QPainter &p, const QRectF &target, int z, int x, int y, QSet<QString> *wanted)
{
    wanted->insert(tiles::key(z, x, y));
    const QPixmap pm = m_tiles->tile(z, x, y);
    if (pm.isNull())
        return false;
    p.drawPixmap(target, pm, QRectF(pm.rect()));
    return true;
}

void MapView::drawFallback(QPainter &p, const QRectF &target, int z, int x, int y)
{
    // Ближайший предок из кэша (до трёх уровней вверх), растянутый на место тайла
    for (int up = 1; up <= 3 && z - up >= 0; ++up) {
        const QPixmap parent = m_tiles->cached(z - up, x >> up, y >> up);
        if (parent.isNull())
            continue;
        const int parts = 1 << up;
        const double side = double(parent.width()) / parts;
        const QRectF source((x % parts) * side, (y % parts) * side, side, side);
        p.drawPixmap(target, parent, source);
        return;
    }
}
