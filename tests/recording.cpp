#include "recording.h"

#include "core/ubx.h"

#include <QDateTime>
#include <QList>
#include <QRandomGenerator>
#include <QString>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {

constexpr double StartLat = 55.75, StartLon = 37.62;
constexpr double CircleRadius = 50.0;  // м: пешеход ходит по кругу
constexpr double Speed = 1.5;          // м/с
constexpr double MetersPerDegree = 111320.0;
constexpr double HeightMsl = 150.0, GeoidSeparation = 14.0;  // м
constexpr int GpsLeapSeconds = 18;
constexpr double UsedElevation = 15.0;  // градусов: ниже спутник в решение не берём

struct System {
    quint8 gnssId;       // UBX
    const char *talker;  // NMEA GSV
    int systemId;        // NMEA GSA (4.10)
    int prnOffset;       // NMEA PRN = svid + prnOffset (ГЛОНАСС — 65–96)
    const char *signalId;
    QList<int> svids;
};

struct Sv {
    const System *sys;
    int svid;
    double elev0, az0, phase;
};

struct State {
    double elev, az;
    int cno;
    bool used;
};

const QList<System> &systems()
{
    static const QList<System> list = {
        {0, "GP", 1, 0, "1", {2, 5, 7, 9, 13, 15, 18, 20, 27, 30}},
        {6, "GL", 2, 64, "1", {1, 2, 8, 9, 15, 16, 17, 24}},
        {2, "GA", 3, 0, "7", {1, 3, 5, 8, 13, 15, 21, 26}},
        {3, "GB", 4, 0, "1", {6, 9, 14, 19, 22, 32}},
    };
    return list;
}

void put(QByteArray *p, qsizetype at, qint64 value, int size)
{
    for (int i = 0; i < size; ++i)
        (*p)[at + i] = char((quint64(value) >> (8 * i)) & 0xFF);
}

QByteArray sentence(const QString &body)
{
    const QByteArray text = body.toLatin1();
    quint8 sum = 0;
    for (char c : text)
        sum ^= quint8(c);
    return '$' + text + '*' + QByteArray::number(sum, 16).toUpper().rightJustified(2, '0') + "\r\n";
}

// «ddmm.mmmmm» и полушарие
QString coordinate(double deg, int degDigits, char positive, char negative)
{
    const double a = std::abs(deg);
    const int whole = int(a);
    return QStringLiteral("%1%2,%3")
        .arg(whole, degDigits, 10, QLatin1Char('0'))
        .arg((a - whole) * 60.0, 8, 'f', 5, QLatin1Char('0'))
        .arg(QLatin1Char(deg >= 0 ? positive : negative));
}

}  // namespace

namespace recording {

QByteArray reference(int epochs)
{
    QRandomGenerator rng(20261011);  // одна и та же последовательность на любой платформе
    QList<Sv> svs;
    for (const System &sys : systems())
        for (int svid : sys.svids)
            svs.append({&sys, svid, 5.0 + rng.bounded(80.0), rng.bounded(360.0), rng.bounded(2 * std::numbers::pi)});

    const QDateTime start(QDate(2026, 10, 11), QTime(12, 0), QTimeZone::UTC);  // воскресенье: начало недели GPS
    const qint64 weekStartMs = QDateTime(start.date(), QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch();

    QByteArray out;
    out.reserve(qsizetype(epochs) * 3000);
    QList<State> states(svs.size());
    for (int n = 0; n < epochs; ++n) {
        const double t = double(n) / RateHz;
        const QDateTime now = start.addMSecs(qint64(n) * 1000 / RateHz);
        const quint32 iTow = quint32(now.toMSecsSinceEpoch() - weekStartMs + GpsLeapSeconds * 1000);

        // Положение: круг радиусом CircleRadius, движение против часовой стрелки
        const double angle = Speed * t / CircleRadius;
        const double north = CircleRadius * std::sin(angle), east = CircleRadius * (1 - std::cos(angle));
        const double lat = StartLat + north / MetersPerDegree;
        const double lon = StartLon + east / (MetersPerDegree * std::cos(StartLat * std::numbers::pi / 180));
        const double velN = Speed * std::cos(angle), velE = Speed * std::sin(angle);
        const double heading = std::fmod(std::atan2(velE, velN) * 180 / std::numbers::pi + 360.0, 360.0);

        int used = 0;
        for (qsizetype i = 0; i < svs.size(); ++i) {
            const Sv &sv = svs[i];
            State &s = states[i];
            s.elev = std::clamp(sv.elev0 + 5.0 * std::sin(t / 600.0 + sv.phase), 1.0, 89.0);
            s.az = std::fmod(sv.az0 + 0.008 * t, 360.0);
            s.cno = std::clamp(int(20 + s.elev / 3 + rng.bounded(-2, 3)), 0, 55);
            s.used = s.elev >= UsedElevation;
            used += s.used;
        }
        const double pdop = 1.2 + 8.0 / std::max(used, 4), hdop = pdop * 0.6, vdop = pdop * 0.8;

        // ---------- UBX ----------
        QByteArray pvt(92, '\0');
        put(&pvt, 0, iTow, 4);
        put(&pvt, 4, now.date().year(), 2);
        put(&pvt, 6, now.date().month(), 1);
        put(&pvt, 7, now.date().day(), 1);
        put(&pvt, 8, now.time().hour(), 1);
        put(&pvt, 9, now.time().minute(), 1);
        put(&pvt, 10, now.time().second(), 1);
        put(&pvt, 11, 0x07, 1);  // validDate, validTime, fullyResolved
        put(&pvt, 12, 20, 4);    // tAcc, нс
        put(&pvt, 16, qint64(now.time().msec()) * 1000000, 4);
        put(&pvt, 20, 3, 1);     // 3D
        put(&pvt, 21, 0x01, 1);  // gnssFixOK
        put(&pvt, 23, used, 1);
        put(&pvt, 24, std::llround(lon * 1e7), 4);
        put(&pvt, 28, std::llround(lat * 1e7), 4);
        put(&pvt, 32, std::llround((HeightMsl + GeoidSeparation) * 1000), 4);
        put(&pvt, 36, std::llround(HeightMsl * 1000), 4);
        put(&pvt, 40, 1500, 4);  // hAcc, мм
        put(&pvt, 44, 2500, 4);  // vAcc, мм
        put(&pvt, 48, std::llround(velN * 1000), 4);
        put(&pvt, 52, std::llround(velE * 1000), 4);
        put(&pvt, 60, std::llround(Speed * 1000), 4);
        put(&pvt, 64, std::llround(heading * 1e5), 4);
        put(&pvt, 68, 300, 4);  // sAcc, мм/с
        put(&pvt, 72, std::llround(2.0 * 1e5), 4);
        put(&pvt, 76, std::llround(pdop * 100), 2);
        out += ubx::frame(ubx::ClassNav, ubx::IdNavPvt, pvt);

        QByteArray dop(18, '\0');
        put(&dop, 0, iTow, 4);
        const double dops[] = {pdop * 1.1, pdop, pdop * 0.5, vdop, hdop, hdop * 0.7, hdop * 0.7};
        for (int i = 0; i < 7; ++i)
            put(&dop, 4 + 2 * i, std::llround(dops[i] * 100), 2);
        out += ubx::frame(ubx::ClassNav, ubx::IdNavDop, dop);

        QByteArray sat(8 + 12 * svs.size(), '\0');
        put(&sat, 0, iTow, 4);
        put(&sat, 4, 1, 1);  // version
        put(&sat, 5, svs.size(), 1);
        for (qsizetype i = 0; i < svs.size(); ++i) {
            const qsizetype at = 8 + 12 * i;
            const State &s = states[i];
            put(&sat, at, svs[i].sys->gnssId, 1);
            put(&sat, at + 1, svs[i].svid, 1);
            put(&sat, at + 2, s.cno, 1);
            put(&sat, at + 3, std::lround(s.elev), 1);
            put(&sat, at + 4, std::lround(s.az), 2);
            put(&sat, at + 8, s.used ? 0x0C : 0x04, 4);  // qualityInd 4, svUsed
        }
        out += ubx::frame(ubx::ClassNav, ubx::IdNavSat, sat);

        // ---------- NMEA ----------
        const QString time = now.toString(QStringLiteral("hhmmss.zzz")).chopped(1);  // hhmmss.ss
        const QString latText = coordinate(lat, 2, 'N', 'S'), lonText = coordinate(lon, 3, 'E', 'W');
        out += sentence(QStringLiteral("GNRMC,%1,A,%2,%3,%4,%5,%6,,,A,V")
                            .arg(time, latText, lonText)
                            .arg(Speed / 0.514444, 0, 'f', 3)
                            .arg(heading, 0, 'f', 2)
                            .arg(now.toString(QStringLiteral("ddMMyy"))));
        out += sentence(QStringLiteral("GNGGA,%1,%2,%3,1,%4,%5,%6,M,%7,M,,")
                            .arg(time, latText, lonText)
                            .arg(used, 2, 10, QLatin1Char('0'))
                            .arg(hdop, 0, 'f', 2)
                            .arg(HeightMsl, 0, 'f', 1)
                            .arg(GeoidSeparation, 0, 'f', 1));
        for (const System &sys : systems()) {
            QStringList prns;
            QList<qsizetype> inView;
            for (qsizetype i = 0; i < svs.size(); ++i) {
                if (svs[i].sys != &sys)
                    continue;
                inView.append(i);
                if (states[i].used && prns.size() < 12)
                    prns << QString::number(svs[i].svid + sys.prnOffset);
            }
            while (prns.size() < 12)
                prns << QString();
            out += sentence(QStringLiteral("GNGSA,A,3,%1,%2,%3,%4,%5")
                                .arg(prns.join(u','))
                                .arg(pdop, 0, 'f', 2)
                                .arg(hdop, 0, 'f', 2)
                                .arg(vdop, 0, 'f', 2)
                                .arg(sys.systemId));
            const qsizetype pages = (inView.size() + 3) / 4;
            for (qsizetype page = 0; page < pages; ++page) {
                QString body = QStringLiteral("%1GSV,%2,%3,%4")
                                   .arg(QLatin1String(sys.talker))
                                   .arg(pages)
                                   .arg(page + 1)
                                   .arg(inView.size(), 2, 10, QLatin1Char('0'));
                for (qsizetype k = page * 4; k < std::min(inView.size(), page * 4 + 4); ++k) {
                    const qsizetype i = inView[k];
                    body += QStringLiteral(",%1,%2,%3,%4")
                                .arg(svs[i].svid + sys.prnOffset, 2, 10, QLatin1Char('0'))
                                .arg(std::lround(states[i].elev), 2, 10, QLatin1Char('0'))
                                .arg(std::lround(states[i].az), 3, 10, QLatin1Char('0'))
                                .arg(states[i].cno, 2, 10, QLatin1Char('0'));
                }
                out += sentence(body + u',' + QLatin1String(sys.signalId));
            }
        }
    }
    return out;
}

}  // namespace recording
