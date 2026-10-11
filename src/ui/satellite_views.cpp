#include "ui/satellite_views.h"

#include "ui/painting.h"
#include "ui/theme.h"

#include <QPainter>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace {

constexpr int LabelGap = 6;  // минимальный зазор между подписями оси X, px

}  // namespace

QSet<int> labelIndices(const QList<char> &systems, int every)
{
    QList<int> result;
    int start = 0;
    for (int i = 0; i < systems.size(); ++i) {
        if (i && systems[i] != systems[i - 1])
            start = i;
        if ((i - start) % every)
            continue;
        if (!result.isEmpty() && i - result.last() < every) {
            if (i != start)
                continue;
            result.removeLast();  // начало группы вытесняет соседнюю подпись
        }
        result.append(i);
    }
    return QSet<int>(result.cbegin(), result.cend());
}

SkyView::SkyView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(300, 300);
}

void SkyView::setData(const QList<Satellite> &sats, const QSet<SatKey> &used, double marker)
{
    m_sats = sats;
    m_used = used;
    m_marker = marker;
    update();
}

void SkyView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor text = palette().text().color();

    // Спутники без азимута/возвышения (нет эфемерид) перечисляем над диаграммой,
    // отводя под надпись отдельную полосу, чтобы она не налезала на круг
    QStringList unplaced;
    for (const Satellite &s : std::as_const(m_sats))
        if (!s.elv || !s.az)
            unplaced << s.label();
    double top = 0;
    if (!unplaced.isEmpty()) {
        QRectF note(6, 4, width() - 12, height() / 4.0);
        const QString msg = tr("No azimuth/elevation yet: ") + unplaced.join(QStringLiteral(", "));
        const int flags = Qt::AlignLeft | Qt::TextWordWrap;
        note.setHeight(std::min(note.height(), p.boundingRect(note, flags, msg).height()));
        p.setPen(text);
        p.drawText(note, flags, msg);
        top = note.bottom() + 4;
    }

    const double side = std::min(double(width()), height() - top) - 36;
    const QPointF c(width() / 2.0, top + (height() - top) / 2);
    const double r = std::max(side, 10.0) / 2;
    p.setPen(QPen(text, 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r, r);
    p.setPen(QPen(text, 1));
    p.drawEllipse(c, r * 2 / 3, r * 2 / 3);  // 30° возвышения
    p.drawEllipse(c, r / 3, r / 3);          // 60° возвышения
    p.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r));
    const struct {
        const char *txt;
        double dx, dy;
    } marks[] = {{"N", 0, -r - 10}, {"S", 0, r + 10}, {"E", r + 10, 0}, {"W", -r - 10, 0}};
    for (const auto &m : marks)
        p.drawText(QRectF(c.x() + m.dx - 8, c.y() + m.dy - 8, 16, 16), Qt::AlignCenter, QLatin1String(m.txt));

    const double mk = m_marker;
    for (const Satellite &s : std::as_const(m_sats)) {
        if (!s.elv || !s.az)
            continue;
        const QPointF pt = painting::polar(c, r * (90 - *s.elv) / 90, *s.az);
        if (!s.cno || !*s.cno)
            p.setPen(QPen(theme::lostColor(), 3));
        else if (!m_used.contains(s.key()))
            p.setPen(QPen(theme::notUsedColor(), 3));
        else
            p.setPen(Qt::NoPen);
        p.setBrush(theme::systemColor(s.sys));
        p.drawEllipse(pt, mk, mk);
        p.setPen(Qt::black);
        QFont font = p.font();
        font.setPointSizeF(std::max(6.0, mk * 0.65));
        p.setFont(font);
        p.drawText(QRectF(pt.x() - mk, pt.y() - mk, 2 * mk, 2 * mk), Qt::AlignCenter, s.label());
        p.setFont(this->font());
    }
}

SignalView::SignalView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
}

void SignalView::setData(const QList<Satellite> &sats, const QSet<SatKey> &used)
{
    m_sats = sats;
    m_used = used;
    update();
}

void SignalView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor text = palette().text().color();
    const double left = 64, bottom = 24, top = 8, right = 8;
    const QRectF plot(left, top, width() - left - right, height() - top - bottom);
    QColor grid = text;
    grid.setAlpha(50);
    for (int v = 0; v <= 50; v += 10) {  // сетка и подписи оси Y
        const double y = plot.bottom() - plot.height() * v / 50;
        p.setPen(QPen(grid, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(text);
        p.drawText(QRectF(0, y - 8, left - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1 dB-Hz").arg(v));
    }
    if (m_sats.isEmpty())
        return;
    const double step = plot.width() / double(m_sats.size());
    const double bw = std::max(4.0, std::min(28.0, step - 4));
    // Подписываем каждый N-й столбик, чтобы подписи не налезали друг на друга
    const QFontMetrics fm = p.fontMetrics();
    int labelW = 0;
    QList<char> systems;
    for (const Satellite &s : std::as_const(m_sats)) {
        labelW = std::max(labelW, fm.horizontalAdvance(s.label()));
        systems << s.sys;
    }
    labelW += LabelGap;
    const QSet<int> labeled = labelIndices(systems, std::max(1, int(std::ceil(labelW / step))));
    for (int i = 0; i < m_sats.size(); ++i) {
        const Satellite &s = m_sats[i];
        const int cno = s.cno.value_or(0);
        const double x = plot.left() + i * step + (step - bw) / 2;
        const double bh = plot.height() * std::min(cno, 50) / 50;
        if (cno && !m_used.contains(s.key()))
            p.setPen(QPen(theme::notUsedColor(), 2));
        else
            p.setPen(Qt::NoPen);
        p.setBrush(theme::systemColor(s.sys));
        p.drawRect(QRectF(x, plot.bottom() - bh, bw, bh));
        p.setPen(Qt::black);
        if (cno && bw >= 14)
            p.drawText(QRectF(x, plot.bottom() - 16, bw, 14), Qt::AlignCenter, QString::number(cno));
        if (labeled.contains(i)) {
            const double cx = x + bw / 2;
            p.setPen(QPen(text, 1));
            p.drawLine(QPointF(cx, plot.bottom()), QPointF(cx, plot.bottom() + 3));
            p.drawText(QRectF(cx - labelW / 2.0, plot.bottom() + 3, labelW, 14), Qt::AlignCenter, s.label());
        }
    }
}
