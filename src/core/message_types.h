// Разобранное содержимое сообщений приёмника: NMEA GGA/GSA/RMC/GSV и UBX NAV-PVT/NAV-SAT/
// NAV-DOP/ACK/CFG-VALGET. Пустое поле — std::nullopt.
#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include <optional>
#include <variant>

namespace nmea {

struct Gga {
    std::optional<int> utcSeconds;  // секунд от полуночи UTC
    std::optional<double> utcTime;  // то же с долями секунды
    std::optional<double> lat;      // град., юг — отрицательная
    std::optional<double> lon;      // град., запад — отрицательная
    int quality = 0;
    std::optional<int> numSv;
    std::optional<double> hdop;
    std::optional<double> alt;  // над уровнем моря, м
};

struct Gsa {
    int navMode = 1;   // 1 — нет решения, 2 — 2D, 3 — 3D
    QList<int> svids;  // спутники в решении
    std::optional<double> pdop;
    std::optional<int> systemId;  // NMEA 4.10+
};

struct Rmc {
    std::optional<int> utcSeconds;
    std::optional<double> utcTime;
    bool active = false;  // статус «A» — данные достоверны
    std::optional<double> speedKnots;
    std::optional<double> cog;
};

struct GsvSatellite {
    int svid = 0;
    std::optional<int> elv;
    std::optional<int> az;
    std::optional<int> cno;
};

struct Gsv {
    QList<GsvSatellite> sats;  // до 4 в сообщении
    QString signalId;          // NMEA 4.10+; пусто — один сигнал
};

}  // namespace nmea

namespace ubx {

using ConfigValues = QMap<quint32, qint64>;

struct NavPvt {
    quint32 iTow = 0;  // мс недели GPS
    int year = 0, month = 0, day = 0, hour = 0, min = 0, second = 0;
    bool validDate = false, validTime = false, fullyResolved = false;
    int fixType = 0;
    bool gnssFixOk = false, diffSoln = false, headVehValid = false;
    int carrSoln = 0;
    int numSv = 0;
    double lon = 0, lat = 0;                          // град.
    qint32 height = 0, hMsl = 0;                      // мм
    quint32 hAcc = 0, vAcc = 0;                       // мм
    qint32 velN = 0, velE = 0, velD = 0, gSpeed = 0;  // мм/с
    double headMot = 0;                               // град.
    quint32 sAcc = 0;                                 // мм/с
    double headAcc = 0;                               // град.
    double pDop = 0;
    double headVeh = 0;  // град.
};

// Спутник из NAV-SAT. sys — буква системы, как в NMEA-модели: G, S, E, B, Q, R
struct NavSatSv {
    char sys = 'G';
    int svid = 0;
    std::optional<int> cno;  // нет — не отслеживается
    std::optional<int> elv;  // нет — положение неизвестно
    std::optional<int> az;
    bool used = false;  // участвует в решении
};

struct NavSat {
    quint32 iTow = 0;
    QList<NavSatSv> svs;
};

struct NavDop {
    quint32 iTow = 0;
    double gDop = 0, pDop = 0, tDop = 0, vDop = 0, hDop = 0, nDop = 0, eDop = 0;
};

struct Ack {
    bool ok = false;  // ACK-ACK; false — ACK-NAK
    quint8 cls = 0;   // подтверждаемое сообщение
    quint8 id = 0;
};

struct Valget {
    int layer = 0;
    ConfigValues values;
};

}  // namespace ubx

// Содержимое, разобранное один раз при приёме кадра; monostate — сообщение без разбора
using MessageData = std::variant<std::monostate, nmea::Gga, nmea::Gsa, nmea::Rmc, nmea::Gsv, ubx::NavPvt, ubx::NavSat,
                                 ubx::NavDop, ubx::Ack, ubx::Valget>;
