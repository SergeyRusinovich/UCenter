// Модель спутников по UBX-NAV-SAT или NMEA GSV/GSA (чистая логика, без Qt-объектов).
#pragma once

#include "core/messages.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <optional>
#include <utility>

using SatKey = std::pair<char, int>;  // система, номер

// Системы в порядке показа: (буква, подпись)
const QList<std::pair<char, QString>> &systemNames();

// SBAS приходит под talker GP с номерами 33-64 и 120-158
char classify(char sys, int svid);

struct Satellite {
    char sys = 'G';
    int svid = 0;
    std::optional<int> elv;
    std::optional<int> az;
    std::optional<int> cno;
    double seen = 0.0;  // монотонное время последнего обновления

    SatKey key() const { return {sys, svid}; }
    QString label() const { return QLatin1Char(sys) + QString::number(svid); }
};

struct SatelliteSnapshot {
    QList<Satellite> sats;  // по системе и номеру
    QSet<SatKey> used;      // в решении
};

// Собирает спутники из NAV-SAT (полный список эпохи с флагом «в решении»), а пока его нет —
// из GSV и признак участия в решении из GSA. NAV-SAT точнее: SBAS и номера спутников приходят
// явно, не по диапазонам PRN.
class SatelliteTracker
{
public:
    static constexpr double SatTtl = 5.0;         // секунд до удаления спутника, от которого нет данных
    static constexpr double SignalWindow = 1.5;   // сигналы одной эпохи приходят почти одновременно
    static constexpr double NavSatTimeout = 2.0;  // секунд: пока NAV-SAT приходит, GSV/GSA не используются

    void handle(const GnssMessage &msg, std::optional<double> now = std::nullopt);
    // Удаляет устаревшие записи; возвращает отсортированные спутники и ключи «в решении»
    SatelliteSnapshot snapshot(std::optional<double> now = std::nullopt);

private:
    struct Signal {
        std::optional<int> cno;
        double seen = 0.0;
    };

    QHash<SatKey, Satellite> m_sats;
    // последний C/N0 по каждому сигналу (частоте) спутника
    QHash<SatKey, QHash<QString, Signal>> m_signals;
    QHash<SatKey, double> m_used;
    std::optional<double> m_navSatSeen;  // монотонное время последнего NAV-SAT
};

// {система: (в решении, видно)} для каждой системы из systemNames()
QHash<char, std::pair<int, int>> countBySystem(const QList<Satellite> &sats, const QSet<SatKey> &used);
