#include "core/satellites.h"

#include "core/timeutil.h"

#include <algorithm>

namespace {

char talkerToSystem(const QString &talker)
{
    static const QHash<QString, char> map = {{"GP", 'G'}, {"GL", 'R'}, {"GA", 'E'}, {"GB", 'B'},
                                             {"BD", 'B'}, {"GQ", 'Q'}, {"GN", 'G'}};
    return map.value(talker, 'G');
}

// systemId из GSA (NMEA 4.10+); 0 — система, которую не показываем
char gsaSystem(int systemId)
{
    static const QHash<int, char> map = {{1, 'G'}, {2, 'R'}, {3, 'E'}, {4, 'B'}, {5, 'Q'}};
    return map.value(systemId, 0);
}

}  // namespace

const QList<std::pair<char, QString>> &systemNames()
{
    static const QList<std::pair<char, QString>> names = {
        {'G', "GPS (G)"},    {'S', "SBAS (S)"},    {'E', "Galileo (E)"},
        {'B', "BeiDou (B)"}, {'R', "GLONASS (R)"}, {'Q', "QZSS (Q)"},
    };
    return names;
}

char classify(char sys, int svid)
{
    if (sys == 'G' && ((33 <= svid && svid <= 64) || (120 <= svid && svid <= 158)))
        return 'S';
    return sys;
}

void SatelliteTracker::handle(const GnssMessage &msg, std::optional<double> now)
{
    const double t = now.value_or(timeutil::monotonic());
    if (const auto *sat = msg.get<ubx::NavSat>()) {
        // Полный список спутников эпохи: заменяет всё, что было (в том числе из NMEA)
        m_navSatSeen = t;
        m_sats.clear();
        m_signals.clear();
        m_used.clear();
        for (const ubx::NavSatSv &sv : sat->svs) {
            const SatKey key{sv.sys, sv.svid};
            m_sats.insert(key, {sv.sys, sv.svid, sv.elv, sv.az, sv.cno, t});
            if (sv.used)
                m_used.insert(key, t);
        }
        return;
    }
    if (m_navSatSeen && t - *m_navSatSeen < NavSatTimeout)
        return;  // NAV-SAT свежий — NMEA не нужен
    if (const auto *gsv = msg.get<nmea::Gsv>()) {
        // До 4 спутников в сообщении, по одному сообщению на сигнал
        const char base = talkerToSystem(msg.talker);
        const QString signal =
            gsv->signalId.isEmpty() ? QStringLiteral("0") : gsv->signalId;  // NMEA < 4.10 — один сигнал
        for (const nmea::GsvSatellite &s : gsv->sats) {
            const SatKey key{classify(base, s.svid), s.svid};
            auto &sigs = m_signals[key];
            sigs[signal] = {s.cno, t};
            // Лучший C/N0 среди сигналов текущей эпохи: падение уровня и потеря
            // слежения (пустой C/N0) не маскируются старыми значениями
            std::optional<int> best;
            for (const Signal &sig : std::as_const(sigs))
                if (sig.cno && *sig.cno && t - sig.seen < SignalWindow && (!best || *sig.cno > *best))
                    best = sig.cno;
            m_sats.insert(key, {key.first, key.second, s.elv, s.az, best, t});
        }
    } else if (const auto *gsa = msg.get<nmea::Gsa>()) {
        const char base = gsa->systemId ? gsaSystem(*gsa->systemId) : talkerToSystem(msg.talker);
        if (!base)
            return;
        for (int svid : gsa->svids)
            m_used.insert({classify(base, svid), svid}, t);
    }
}

SatelliteSnapshot SatelliteTracker::snapshot(std::optional<double> now)
{
    const double t = now.value_or(timeutil::monotonic());
    m_sats.removeIf([t](const auto &it) { return t - it.value().seen >= SatTtl; });
    m_signals.removeIf([this](const auto &it) { return !m_sats.contains(it.key()); });
    m_used.removeIf([t](const auto &it) { return t - it.value() >= SatTtl; });

    SatelliteSnapshot snap;
    snap.sats = m_sats.values();
    std::sort(snap.sats.begin(), snap.sats.end(),
              [](const Satellite &a, const Satellite &b) { return a.key() < b.key(); });
    for (auto it = m_used.cbegin(); it != m_used.cend(); ++it)
        snap.used.insert(it.key());
    return snap;
}

QHash<char, std::pair<int, int>> countBySystem(const QList<Satellite> &sats, const QSet<SatKey> &used)
{
    QHash<char, std::pair<int, int>> counts;
    for (const auto &[ch, _] : systemNames())
        counts.insert(ch, {0, 0});
    for (const Satellite &s : sats) {
        auto &c = counts[s.sys];
        c.first += used.contains(s.key());
        c.second += 1;
    }
    return counts;
}
