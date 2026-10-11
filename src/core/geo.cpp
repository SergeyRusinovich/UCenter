#include "core/geo.h"

#include <algorithm>
#include <cmath>

namespace geo {

namespace {

constexpr double Wgs84A = 6378137.0;
constexpr double Wgs84F = 1 / 298.257223563;
constexpr double EarthRadius = 6371008.8;  // средний радиус Земли, м (для сферического запасного расчёта)

double bearingSphere(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = radians(lat1), p2 = radians(lat2), dl = radians(lon2 - lon1);
    const double y = std::sin(dl) * std::cos(p2);
    const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
    return wrap360(degrees(std::atan2(y, x)));
}

Inverse inverseSphere(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = radians(lat1), p2 = radians(lat2);
    const double h = std::pow(std::sin((p2 - p1) / 2), 2)
                     + std::cos(p1) * std::cos(p2) * std::pow(std::sin(radians(lon2 - lon1) / 2), 2);
    const double dist = 2 * EarthRadius * std::asin(std::min(1.0, std::sqrt(h)));
    return {dist, bearingSphere(lat1, lon1, lat2, lon2), bearingSphere(lat2, lon2, lat1, lon1)};
}

}  // namespace

double wrap360(double deg)
{
    const double r = std::fmod(deg, 360.0);
    return r < 0 ? r + 360.0 : r;
}

QPointF llToPx(double lat, double lon, int zoom)
{
    lat = std::clamp(lat, -MaxLat, MaxLat);
    const double scale = TileSize * std::ldexp(1.0, zoom);
    const double x = (lon + 180.0) / 360.0 * scale;
    const double s = std::sin(radians(lat));
    const double y = (0.5 - std::log((1 + s) / (1 - s)) / (4 * Pi)) * scale;
    return {x, y};
}

LatLon pxToLl(double x, double y, int zoom)
{
    const double scale = TileSize * std::ldexp(1.0, zoom);
    const double lon = x / scale * 360.0 - 180.0;
    const double n = Pi - 2 * Pi * y / scale;
    return {degrees(std::atan(std::sinh(n))), lon};
}

Inverse inverse(double lat1, double lon1, double lat2, double lon2)
{
    // Совпадающие точки — явно: с FMA-сжатием sinSigma ниже может выйти не ровно нулём
    if (lat1 == lat2 && wrap360(lon1) == wrap360(lon2))
        return {0.0, std::nullopt, std::nullopt};
    const double a = Wgs84A, f = Wgs84F;
    const double b = (1 - f) * a;
    const double L = radians(lon2 - lon1);
    const double u1 = std::atan((1 - f) * std::tan(radians(lat1)));
    const double u2 = std::atan((1 - f) * std::tan(radians(lat2)));
    const double sinU1 = std::sin(u1), cosU1 = std::cos(u1);
    const double sinU2 = std::sin(u2), cosU2 = std::cos(u2);

    double lam = L;
    double sinLam = 0, cosLam = 0, sinSigma = 0, cosSigma = 0, sigma = 0, cos2Alpha = 0, cos2Sm = 0;
    bool converged = false;
    for (int i = 0; i < 200; ++i) {
        sinLam = std::sin(lam);
        cosLam = std::cos(lam);
        sinSigma = std::hypot(cosU2 * sinLam, cosU1 * sinU2 - sinU1 * cosU2 * cosLam);
        if (sinSigma == 0)
            return {0.0, std::nullopt, std::nullopt};
        cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLam;
        sigma = std::atan2(sinSigma, cosSigma);
        const double sinAlpha = cosU1 * cosU2 * sinLam / sinSigma;
        cos2Alpha = 1 - sinAlpha * sinAlpha;
        cos2Sm = cos2Alpha != 0 ? cosSigma - 2 * sinU1 * sinU2 / cos2Alpha : 0.0;
        const double c = f / 16 * cos2Alpha * (4 + f * (4 - 3 * cos2Alpha));
        const double lamPrev = lam;
        lam =
            L + (1 - c) * f * sinAlpha * (sigma + c * sinSigma * (cos2Sm + c * cosSigma * (-1 + 2 * cos2Sm * cos2Sm)));
        if (std::abs(lam - lamPrev) < 1e-12) {
            converged = true;
            break;
        }
    }
    if (!converged)
        return inverseSphere(lat1, lon1, lat2, lon2);  // почти антиподы: Винсенти не сходится

    const double uSq = cos2Alpha * (a * a - b * b) / (b * b);
    const double bigA = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
    const double bigB = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
    const double dSigma =
        bigB * sinSigma
        * (cos2Sm
           + bigB / 4
                 * (cosSigma * (-1 + 2 * cos2Sm * cos2Sm)
                    - bigB / 6 * cos2Sm * (-3 + 4 * sinSigma * sinSigma) * (-3 + 4 * cos2Sm * cos2Sm)));
    const double dist = b * bigA * (sigma - dSigma);
    const double fwd = std::atan2(cosU2 * sinLam, cosU1 * sinU2 - sinU1 * cosU2 * cosLam);
    const double at2 = std::atan2(cosU1 * sinLam, -sinU1 * cosU2 + cosU1 * sinU2 * cosLam);
    return {dist, wrap360(degrees(fwd)), wrap360(degrees(at2) + 180)};
}

}  // namespace geo
