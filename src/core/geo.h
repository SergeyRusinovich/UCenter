// Проекция Web Mercator для тайловой карты и геодезия WGS84 (чистая математика).
#pragma once

#include <QPointF>

#include <optional>

struct LatLon {
    double lat = 0.0;
    double lon = 0.0;
    friend bool operator==(const LatLon &, const LatLon &) = default;
};

// Прямоугольник на карте: юг, запад, север, восток
struct Bounds {
    double south = 0.0;
    double west = 0.0;
    double north = 0.0;
    double east = 0.0;
    friend bool operator==(const Bounds &, const Bounds &) = default;
};

namespace geo {

inline constexpr int TileSize = 256;
inline constexpr double MaxLat = 85.0511;
inline constexpr double Pi = 3.14159265358979323846;

inline double radians(double deg)
{
    return deg * Pi / 180.0;
}
inline double degrees(double rad)
{
    return rad * 180.0 / Pi;
}
// Остаток по модулю 360 в [0, 360) (std::fmod сохраняет знак делимого)
double wrap360(double deg);

// Географические координаты -> пиксели мира Web Mercator
QPointF llToPx(double lat, double lon, int zoom);
LatLon pxToLl(double x, double y, int zoom);

struct Inverse {
    double distance = 0.0;              // м
    std::optional<double> azimuth;      // 1->2, град. от истинного севера по часовой [0, 360)
    std::optional<double> backAzimuth;  // 2->1
};

// Обратная геодезическая задача (формулы Винсенти, WGS84). Для совпадающих точек азимутов нет.
Inverse inverse(double lat1, double lon1, double lat2, double lon2);
inline Inverse inverse(LatLon a, LatLon b)
{
    return inverse(a.lat, a.lon, b.lat, b.lon);
}

}  // namespace geo
