#include "ui/compass_view.h"

#include "core/geo.h"
#include "ui/painting.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

using painting::polar;

namespace {

const char *const UnsupportedText = QT_TRANSLATE_NOOP(
    "CompassView", "Compass is not supported by this device\n(no heading sensor data from the receiver)");

// Курс появился, пропал или повернулся на заметный угол (с учётом перехода через 360°)
bool visiblyChanged(std::optional<double> before, std::optional<double> after)
{
    if (before.has_value() != after.has_value())
        return true;
    if (!before)
        return false;
    const double d = std::fmod(std::abs(*after - *before), 360.0);
    return std::min(d, 360.0 - d) >= CompassView::VisibleDeg;
}

// Полуширина основания стрелки, повёрнутая вместе с ней
QPointF side(double deg, double halfWidth)
{
    return {std::cos(geo::radians(deg)) * halfWidth, std::sin(geo::radians(deg)) * halfWidth};
}

}  // namespace

CompassView::CompassView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
}

void CompassView::setState(bool supported, std::optional<double> heading, std::optional<double> cog)
{
    const bool changed = supported != m_supported || visiblyChanged(m_heading, heading) || visiblyChanged(m_cog, cog);
    if (!changed)
        return;  // показанные значения остаются: разница меньше VisibleDeg
    m_supported = supported;
    m_heading = heading;
    m_cog = cog;
    update();
}

void CompassView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(width() / 2.0, height() / 2.0);
    const double r = std::min(width(), height()) / 2.0 - 22;
    const QColor text = palette().text().color();
    QColor dial = text;
    dial.setAlpha(m_supported ? 255 : 90);

    p.setPen(QPen(dial, 2));
    p.drawEllipse(c, r, r);
    for (int deg = 0; deg < 360; deg += 30)
        p.drawLine(polar(c, r, deg), polar(c, r - (deg % 90 == 0 ? 10 : 5), deg));
    p.setPen(dial);
    for (const auto &[txt, deg] : {std::pair{"N", 0}, std::pair{"E", 90}, std::pair{"S", 180}, std::pair{"W", 270}}) {
        const QPointF pt = polar(c, r + 12, deg);
        p.drawText(QRectF(pt.x() - 8, pt.y() - 8, 16, 16), Qt::AlignCenter, QLatin1String(txt));
    }

    // Курс движения по земле — маркер на ободе, доступен и без датчика
    if (m_cog) {
        const QPointF tip = polar(c, r - 2, *m_cog), base = polar(c, r - 14, *m_cog);
        const QPointF s = side(*m_cog, 5);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#1e6fff"));
        p.drawPolygon(QPolygonF({tip, base + s, base - s}));
    }

    if (m_supported && m_heading) {
        const QPointF tip = polar(c, r - 18, *m_heading);
        const QPointF tail = polar(c, 14, *m_heading + 180);
        const QPointF s = side(*m_heading, 7);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#e02020"));
        p.drawPolygon(QPolygonF({tip, c + s, c - s}));
        p.setBrush(QColor("#888"));
        p.drawPolygon(QPolygonF({tail, c + s, c - s}));
        p.setPen(text);
        p.drawText(QRectF(c.x() - 40, c.y() + r * 0.35, 80, 18), Qt::AlignCenter,
                   QString::number(*m_heading, 'f', 0) + u'°');
    } else if (!m_supported) {
        p.setPen(text);
        p.drawText(QRectF(c.x() - r * 0.8, c.y() - r * 0.5, r * 1.6, r), Qt::AlignCenter | Qt::TextWordWrap,
                   tr(UnsupportedText));
    }
}
