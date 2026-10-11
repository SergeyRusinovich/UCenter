#include "core/navigation.h"

#include "core/timeutil.h"

#include <QHash>

namespace {

constexpr double KnotsToKmh = 1.852;
constexpr double MmsToKmh = 0.0036;

QString fixName(int fixType)
{
    static const QHash<int, QString> names = {{0, "No Fix"}, {1, "Dead Reckoning"}, {2, "2D"},
                                              {3, "3D"},     {4, "GNSS+DR"},        {5, "Time only"}};
    return names.value(fixType, QString::number(fixType));
}

}  // namespace

bool NavigationState::apply(const GnssMessage &msg, std::optional<double> now)
{
    const double t = now.value_or(timeutil::monotonic());
    const bool pvt = m_pvtSeen && t - *m_pvtSeen < PvtTimeout;
    NavData &d = m_data;

    if (const auto *p = msg.get<ubx::NavPvt>()) {
        m_pvtSeen = t;
        if (p->fixType >= 2 && p->gnssFixOk) {
            d.lat = p->lat;
            d.lon = p->lon;
            d.altM = p->hMsl / 1000.0;
            d.speedKmh = p->gSpeed * MmsToKmh;
            d.cog = p->headMot;
        }
        if (p->validTime)
            d.utcSeconds = p->hour * 3600 + p->min * 60 + p->second;
        QString fix = fixName(p->fixType);
        if ((p->fixType == 3 || p->fixType == 4) && p->diffSoln)
            fix = QStringLiteral("GNSS+DGNSS");
        d.fix = fix;
        d.pdop = p->pDop;
        d.numSv = p->numSv;
        if (p->headVehValid) {
            d.heading = p->headVeh;
            d.headingSupported = true;
        }
        return true;
    }
    if (const auto *dop = msg.get<ubx::NavDop>()) {
        d.hdop = dop->hDop;  // HDOP без NMEA: NAV-PVT его не содержит
        return true;
    }
    if (const auto *g = msg.get<nmea::Gga>()) {
        m_quality = g->quality;
        d.hdop = g->hdop;
        if (pvt)
            return true;
        if (g->quality && g->lat && g->lon) {
            d.lat = g->lat;
            d.lon = g->lon;
        }
        if (g->alt)
            d.altM = g->alt;
        if (g->utcSeconds)
            d.utcSeconds = g->utcSeconds;
        d.numSv = g->numSv;
        d.fix = nmeaFix();
        return true;
    }
    if (const auto *g = msg.get<nmea::Gsa>()) {
        m_navMode = g->navMode;
        if (pvt)
            return false;
        d.pdop = g->pdop;
        d.fix = nmeaFix();
        return true;
    }
    if (const auto *r = msg.get<nmea::Rmc>()) {
        if (pvt)
            return false;
        bool changed = false;
        if (r->utcSeconds) {
            d.utcSeconds = r->utcSeconds;
            changed = true;
        }
        if (r->active) {
            if (r->speedKnots) {
                d.speedKmh = *r->speedKnots * KnotsToKmh;
                changed = true;
            }
            if (r->cog) {
                d.cog = r->cog;
                changed = true;
            }
        }
        return changed;
    }
    return false;
}

QString NavigationState::nmeaFix() const
{
    if (m_quality == 0 || m_navMode == 1)
        return QStringLiteral("No Fix");
    if (m_quality == 2)
        return QStringLiteral("GNSS+DGNSS");
    return m_navMode == 2 ? QStringLiteral("2D") : QStringLiteral("3D");
}
