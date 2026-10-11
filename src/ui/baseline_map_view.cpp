#include "ui/baseline_map_view.h"

#include "ui/location_popover.h"
#include "ui/map_buttons.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace {

constexpr double MarkerR = 11;
constexpr double HitR = 15;                // px: радиус, в котором нажатие захватывает маркер
constexpr double EquatorM = 40075016.686;  // длина экватора WGS84, м
constexpr double FlagPole = 34;            // px: высота древка
constexpr int LabelMaxW = 220;
const QColor Highlight("#FFB300");
const QColor FlagColor("#E53935");
const QColor Ink("#202124");

QColor pointColor(PointKey key)
{
    return key == PointKey::A ? QColor("#1FA35A") : QColor("#1e6fff");
}

// Стрелка направления A->B у середины отрезка
void paintArrow(QPainter &p, const QPointF &pa, const QPointF &pb)
{
    const QPointF d = pb - pa;
    const double length = std::hypot(d.x(), d.y());
    if (length < 4 * MarkerR)
        return;
    const QPointF u = d / length;
    const QPointF tip = (pa + pb) / 2 + u * 7;
    const QPointF base = tip - u * 14;
    const QPointF side = QPointF(-u.y(), u.x()) * 6;
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(Ink);
    p.drawPolygon(QPolygonF({tip, base + side, base - side}));
}

}  // namespace

BaselineMapView::BaselineMapView(QWidget *parent)
    : MapView(parent, false, tiles::esriImagery())
{
    setMouseTracking(true);  // курсор-«рука» над тем, что можно перетащить
    m_btnCenter->setToolTip(locateTip());
    m_btnExpand = makeButton<ExpandButton>(QString(), [this](bool on) { emit expandToggled(on); });
    m_btnExpand->setCheckable(true);
    m_btnExpand->setToolTip(tr("Map over the whole window (Esc — back)"));
    m_buttons.append(m_btnExpand);
}

QString BaselineMapView::locateTip()
{
    return tr("My location: center the map and put point A here");
}

// ---------- Развёрнутый вид ----------
bool BaselineMapView::isExpanded() const
{
    return m_btnExpand->isChecked();
}

void BaselineMapView::setExpanded(bool on)
{
    m_btnExpand->setChecked(on);
}

void BaselineMapView::setPlace(const std::optional<Place> &place)
{
    m_place = place;
    update();
}

void BaselineMapView::setLocationAlert(LocationLevel level, const QString &text)
{
    std::optional<QColor> color;
    if (level == LocationLevel::Wait)
        color = QColor("#F5A623");
    else if (level == LocationLevel::Error)
        color = QColor("#D93F3F");
    m_btnCenter->setAlert(color);
    m_btnCenter->setToolTip(text.isEmpty() ? locateTip() : locateTip() + QStringLiteral("\n\n") + text);
}

void BaselineMapView::showLocationHelp(LocationLevel level, const QString &text)
{
    if (!m_popover) {
        m_popover = new LocationPopover(this);
        connect(m_popover, &LocationPopover::retryRequested, this, &BaselineMapView::locationRetryRequested);
    }
    m_popover->showAt(m_btnCenter, level, text);
}

void BaselineMapView::setPoint(PointKey key, std::optional<LatLon> ll)
{
    point(key) = ll;
    update();
}

std::optional<QPointF> BaselineMapView::anchor(Handle h) const
{
    if (h == Handle::Flag) {
        if (!m_place)
            return std::nullopt;
        return toScreen(m_place->lat, m_place->lon);
    }
    const auto ll = point(h == Handle::A ? PointKey::A : PointKey::B);
    if (!ll)
        return std::nullopt;
    return toScreen(*ll);
}

std::optional<BaselineMapView::Handle> BaselineMapView::hit(const QPointF &pos) const
{
    for (Handle h : {Handle::B, Handle::A}) {  // маркеры рисуются поверх флажка, B — поверх A
        const auto pt = anchor(h);
        if (pt && (*pt - pos).manhattanLength() <= HitR)
            return h;
    }
    if (const auto base = anchor(Handle::Flag)) {
        // древко и полотнище флажка
        if (QRectF(base->x() - 6, base->y() - FlagPole - 4, 32, FlagPole + 10).contains(pos))
            return Handle::Flag;
    }
    return std::nullopt;
}

// ---------- Перетаскивание маркеров и флажка ----------
void BaselineMapView::mousePressEvent(QMouseEvent *e)
{
    m_grab = e->button() == Qt::LeftButton ? hit(e->position()) : std::nullopt;
    if (!m_grab) {
        MapView::mousePressEvent(e);
        return;
    }
    const auto at = anchor(*m_grab);
    if (!at) {
        m_grab.reset();
        return;
    }
    m_grabOffset = *at - e->position();
    setCursor(Qt::ClosedHandCursor);
}

void BaselineMapView::mouseMoveEvent(QMouseEvent *e)
{
    if (!m_grab) {
        if (e->buttons() == Qt::NoButton)
            setCursor(hit(e->position()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        MapView::mouseMoveEvent(e);
        return;
    }
    const LatLon ll = toGeo(e->position() + m_grabOffset);
    if (*m_grab == Handle::Flag) {
        if (!m_place)  // место убрали, пока его тащили
            return;
        m_place->lat = ll.lat;
        m_place->lon = ll.lon;
        emit placeMoved(ll.lat, ll.lon);
    } else {
        const PointKey key = *m_grab == Handle::A ? PointKey::A : PointKey::B;
        point(key) = ll;
        emit pointMoved(key, ll.lat, ll.lon);
    }
    update();
}

void BaselineMapView::mouseReleaseEvent(QMouseEvent *e)
{
    if (!m_grab) {
        MapView::mouseReleaseEvent(e);
        return;
    }
    m_grab.reset();
    setCursor(Qt::OpenHandCursor);
}

// ---------- Отрисовка ----------
void BaselineMapView::paintOverlay(QPainter &p)
{
    if (m_pos && m_accuracy && *m_accuracy > 0) {
        const double mPerPx = EquatorM * std::cos(geo::radians(m_pos->lat)) / (geo::TileSize * std::ldexp(1.0, zoom()));
        const double r = *m_accuracy / mPerPx;
        if (r > 10) {
            p.setPen(QPen(QColor(255, 255, 255, 200), 1.5));
            p.setBrush(QColor(30, 111, 255, 45));
            p.drawEllipse(toScreen(*m_pos), r, r);
        }
    }
    if (m_place)
        paintPlaceArea(p, *m_place);
    const auto a = point(PointKey::A), b = point(PointKey::B);
    if (a && b) {
        const QPointF pa = toScreen(*a), pb = toScreen(*b);
        for (const auto &[color, width] : {std::pair{QColor(Qt::white), 5}, std::pair{Ink, 2}}) {
            p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(pa, pb);
        }
        paintArrow(p, pa, pb);
    }
    if (m_place)
        paintFlag(p, *m_place);
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    for (PointKey key : PointKeys) {
        const auto ll = point(key);
        if (!ll)
            continue;
        const QPointF pt = toScreen(*ll);
        p.setPen(QPen(Qt::white, 2.5));
        p.setBrush(pointColor(key));
        p.drawEllipse(pt, MarkerR, MarkerR);
        p.setPen(Qt::white);
        p.drawText(QRectF(pt.x() - MarkerR, pt.y() - MarkerR, 2 * MarkerR, 2 * MarkerR), Qt::AlignCenter,
                   pointName(key));
    }
}

void BaselineMapView::paintPlaceArea(QPainter &p, const Place &place)
{
    QColor fill = Highlight;
    fill.setAlpha(70);
    for (const auto &[rings, closed] : {std::pair{&place.areas, true}, std::pair{&place.lines, false}}) {
        for (const Ring &ring : *rings) {
            QPolygonF poly;
            for (const LatLon &ll : ring)
                poly << toScreen(ll);
            QPainterPath path;
            path.addPolygon(poly);
            if (closed)
                path.closeSubpath();
            p.setBrush(closed ? QBrush(fill) : QBrush(Qt::NoBrush));
            p.setPen(QPen(QColor(255, 255, 255, 220), 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(path);
            p.setPen(QPen(Highlight, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(path);
        }
    }
}

// Флажок: древко стоит основанием на месте, рядом — название
void BaselineMapView::paintFlag(QPainter &p, const Place &place)
{
    const QPointF base = toScreen(place.lat, place.lon);
    const QPointF top = base - QPointF(0, FlagPole);
    p.setPen(QPen(Qt::white, 2));
    p.setBrush(Highlight);
    p.drawEllipse(base, 4, 4);
    for (const auto &[color, width] : {std::pair{QColor(Qt::white), 4}, std::pair{Ink, 2}}) {
        p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(base, top);
    }
    const QPolygonF cloth({top, top + QPointF(22, 7), top + QPointF(0, 14)});
    p.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
    p.setBrush(FlagColor);
    p.drawPolygon(cloth);

    const QFontMetrics fm = p.fontMetrics();
    const QString text = fm.elidedText(place.name, Qt::ElideRight, LabelMaxW);
    const QRectF box(top.x() + 26, top.y() - 2, fm.horizontalAdvance(text) + 12, fm.height() + 4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 255, 255, 230));
    p.drawRoundedRect(box, 4, 4);
    p.setPen(Ink);
    p.drawText(box, Qt::AlignCenter, text);
}
