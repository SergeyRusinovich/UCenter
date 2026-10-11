#include "core/baseline.h"

#include "core/format.h"

#include <cmath>

std::optional<double> Baseline::deviation() const
{
    if (!azimuth)
        return std::nullopt;
    return *azimuth > 180 ? *azimuth - 360 : *azimuth;
}

double Baseline::dNorth() const
{
    return azimuth ? distanceM * std::cos(geo::radians(*azimuth)) : 0.0;
}

double Baseline::dEast() const
{
    return azimuth ? distanceM * std::sin(geo::radians(*azimuth)) : 0.0;
}

Baseline measure(LatLon a, LatLon b)
{
    const geo::Inverse inv = geo::inverse(a, b);
    return {a.lat, a.lon, b.lat, b.lon, inv.distance, inv.azimuth, inv.backAzimuth, {}};
}

QString formatAzimuth(std::optional<double> az)
{
    return az ? fmt::fixed(*az, 2) + u'°' : fmt::Empty;
}

QString formatDeviation(std::optional<double> dev)
{
    if (!dev)
        return fmt::Empty;
    const char *side = *dev > 0 ? "E" : *dev < 0 ? "W" : "N";
    return QStringLiteral("%1° (%2)").arg(fmt::signedFixed(*dev, 2), QLatin1String(side));
}
