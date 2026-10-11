#include "ui/painting.h"

#include "core/geo.h"

#include <cmath>

namespace painting {

QPointF polar(const QPointF &center, double radius, double deg)
{
    const double a = geo::radians(deg);
    return {center.x() + radius * std::sin(a), center.y() - radius * std::cos(a)};
}

}  // namespace painting
