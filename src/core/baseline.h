// Измерение базовой линии A->B: азимут от истинного севера, расстояние, смещения.
#pragma once

#include "core/geo.h"

#include <QString>

#include <optional>

struct Baseline {
    double aLat = 0.0;
    double aLon = 0.0;
    double bLat = 0.0;
    double bLon = 0.0;
    double distanceM = 0.0;
    std::optional<double> azimuth;      // A->B, град. от истинного севера по часовой [0, 360)
    std::optional<double> backAzimuth;  // B->A
    QString savedAt;                    // локальное время сохранения

    // Отклонение A->B от истинного севера, град. (-180, 180]: «+» к востоку, «-» к западу
    std::optional<double> deviation() const;
    // Смещение B относительно A на север и на восток, м (в локальной плоскости)
    double dNorth() const;
    double dEast() const;

    LatLon a() const { return {aLat, aLon}; }
    LatLon b() const { return {bLat, bLon}; }

    friend bool operator==(const Baseline &, const Baseline &) = default;
};

Baseline measure(LatLon a, LatLon b);

QString formatAzimuth(std::optional<double> az);
QString formatDeviation(std::optional<double> dev);
