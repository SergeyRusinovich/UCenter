#include "ui/map_buttons.h"

#include <QPainter>

void IconButton::paintEvent(QPaintEvent *e)
{
    QPushButton::paintEvent(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    paintIcon(p, QPointF(width() / 2.0, height() / 2.0), palette().color(group, QPalette::ButtonText));
}

void CrosshairButton::setAlert(std::optional<QColor> color)
{
    m_alert = color;
    update();
}

void CrosshairButton::paintEvent(QPaintEvent *e)
{
    IconButton::paintEvent(e);
    if (!m_alert)
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(*m_alert);
    p.drawEllipse(QPointF(width() - 6, 6), 4.5, 4.5);
}

void CrosshairButton::paintIcon(QPainter &p, const QPointF &c, const QColor &color)
{
    p.setPen(QPen(color, 1.6));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, 5.5, 5.5);
    for (const QPointF d : {QPointF(0, -1), QPointF(0, 1), QPointF(-1, 0), QPointF(1, 0)})
        p.drawLine(c + d * 3, c + d * 9);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(c, 1.5, 1.5);
}

void ExpandButton::paintIcon(QPainter &p, const QPointF &c, const QColor &color)
{
    p.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const bool inward = isChecked();
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            const double off = inward ? 2 : 7;
            const QPointF corner = c + QPointF(sx * off, sy * off);
            const double arm = inward ? 4 : -4;  // усы уголка: наружу у «свернуть», к центру у «развернуть»
            p.drawLine(corner, corner + QPointF(sx * arm, 0));
            p.drawLine(corner, corner + QPointF(0, sy * arm));
        }
    }
}
