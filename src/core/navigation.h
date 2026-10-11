// Навигационное решение из NAV-PVT/NAV-DOP и NMEA GGA/GSA/RMC (чистая логика, без Qt-объектов).
#pragma once

#include "core/messages.h"

#include <QString>

#include <optional>

// Снимок навигационного решения; nullopt — значение ещё не получено
struct NavData {
    std::optional<double> lat;
    std::optional<double> lon;
    std::optional<double> altM;
    std::optional<int> utcSeconds;  // секунд от полуночи UTC
    std::optional<QString> fix;
    std::optional<double> pdop;
    std::optional<double> hdop;
    std::optional<int> numSv;
    std::optional<double> speedKmh;
    std::optional<double> cog;      // курс движения (по земле), град.
    std::optional<double> heading;  // курс корпуса из датчика приёмника, град.
    bool headingSupported = false;  // приёмник хоть раз отдал валидный headVeh
};

// Накапливает решение из сообщений приёмника.
//
// NAV-PVT — основной источник (точнее и с флагами валидности); NMEA используется,
// когда PVT не включён. HDOP — из GGA или NAV-DOP.
class NavigationState
{
public:
    static constexpr double PvtTimeout = 2.0;  // секунд: пока NAV-PVT приходит, NMEA не перезаписывает его данные

    const NavData &data() const { return m_data; }

    // Применяет сообщение; true, если решение могло измениться
    bool apply(const GnssMessage &msg, std::optional<double> now = std::nullopt);

private:
    QString nmeaFix() const;

    NavData m_data;
    int m_quality = 0;                // GGA quality
    int m_navMode = 1;                // GSA navMode
    std::optional<double> m_pvtSeen;  // монотонное время последнего NAV-PVT
};
