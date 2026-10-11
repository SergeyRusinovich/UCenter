#include "ui/value_label.h"

#include "core/format.h"

#include <QPainter>

ValueLabel::ValueLabel(QWidget *parent)
    : QWidget(parent)
    , m_text(fmt::Empty)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ValueLabel::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    update();  // без updateGeometry(): размер от текста не зависит
}

QSize ValueLabel::sizeHint() const
{
    // Ширина самого длинного обычного значения — координаты в градусах
    return {fontMetrics().horizontalAdvance(QStringLiteral("-000.0000000°")), fontMetrics().height()};
}

QSize ValueLabel::minimumSizeHint() const
{
    return {fontMetrics().horizontalAdvance(QStringLiteral("0000")), fontMetrics().height()};
}

void ValueLabel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(m_text, Qt::ElideRight, width()));
}
