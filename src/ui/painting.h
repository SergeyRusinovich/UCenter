// Общие помощники отрисовки.
#pragma once

#include <QPointF>

namespace painting {

// Точка на расстоянии radius от center по азимуту deg (0° — вверх, по часовой)
QPointF polar(const QPointF &center, double radius, double deg);

}  // namespace painting
