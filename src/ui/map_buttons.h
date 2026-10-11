// Круглые кнопки карты со значками, нарисованными цветом текста текущей темы.
#pragma once

#include <QColor>
#include <QPushButton>

#include <optional>

class IconButton : public QPushButton
{
    Q_OBJECT

public:
    using QPushButton::QPushButton;

protected:
    void paintEvent(QPaintEvent *e) override;
    virtual void paintIcon(QPainter &p, const QPointF &c, const QColor &color) = 0;
};

// «Прицел»: вернуться к текущей позиции. Цветная точка в углу — проблема с позицией.
class CrosshairButton : public IconButton
{
    Q_OBJECT

public:
    using IconButton::IconButton;
    void setAlert(std::optional<QColor> color);

protected:
    void paintEvent(QPaintEvent *e) override;
    void paintIcon(QPainter &p, const QPointF &c, const QColor &color) override;

private:
    std::optional<QColor> m_alert;
};

// Развернуть/свернуть карту: уголки наружу или (когда нажата) внутрь
class ExpandButton : public IconButton
{
    Q_OBJECT

public:
    using IconButton::IconButton;

protected:
    void paintIcon(QPainter &p, const QPointF &c, const QColor &color) override;
};
