#include "core/nmea.h"

#include <QHash>
#include <QStringList>

#include <type_traits>

namespace nmea {

namespace {

constexpr int MinAddress = 3;  // «P» + тип у фирменных, talker + тип у стандартных

using Fields = QList<QByteArray>;

// Поле: пустое -> nullopt (ok остаётся true), неразборчивое -> ok = false
template <class T>
std::optional<T> number(const Fields &fields, qsizetype i, bool *ok)
{
    if (i >= fields.size() || fields[i].isEmpty())
        return std::nullopt;
    bool good = false;
    T v{};
    if constexpr (std::is_same_v<T, int>)
        v = fields[i].toInt(&good);
    else
        v = fields[i].toDouble(&good);
    if (!good) {
        *ok = false;
        return std::nullopt;
    }
    return v;
}

QByteArray field(const Fields &fields, qsizetype i)
{
    return i < fields.size() ? fields[i] : QByteArray();
}

// «hhmmss.ss» -> секунды от полуночи с долями
std::optional<double> timeOfDay(const Fields &fields, qsizetype i, bool *ok)
{
    const QByteArray t = field(fields, i);
    if (t.isEmpty())
        return std::nullopt;
    bool h = false, m = false, s = false;
    const int hh = t.mid(0, 2).toInt(&h), mm = t.mid(2, 2).toInt(&m);
    const double ss = t.mid(4).toDouble(&s);
    if (t.size() < 6 || !h || !m || !s || hh > 23 || mm > 59 || ss < 0 || ss >= 61) {
        *ok = false;
        return std::nullopt;
    }
    return hh * 3600 + mm * 60 + ss;
}

std::optional<int> wholeSeconds(const std::optional<double> &t)
{
    return t ? std::optional(int(*t)) : std::nullopt;
}

// «ddmm.mmmm» + полушарие -> градусы со знаком
std::optional<double> coordinate(const Fields &fields, qsizetype i, int degDigits, bool *ok)
{
    const QByteArray v = field(fields, i), hemi = field(fields, i + 1);
    if (v.isEmpty())
        return std::nullopt;
    bool d = false, m = false;
    const double deg = v.left(degDigits).toDouble(&d);
    const double min = v.mid(degDigits).toDouble(&m);
    if (!d || !m || v.size() <= degDigits) {
        *ok = false;
        return std::nullopt;
    }
    const double value = deg + min / 60.0;
    return (hemi == "S" || hemi == "W") ? -value : value;
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

std::optional<Gga> decodeGga(const Fields &f)
{
    bool ok = true;
    Gga g;
    g.utcTime = timeOfDay(f, 0, &ok);
    g.utcSeconds = wholeSeconds(g.utcTime);
    g.lat = coordinate(f, 1, 2, &ok);
    g.lon = coordinate(f, 3, 3, &ok);
    g.quality = number<int>(f, 5, &ok).value_or(0);
    g.numSv = number<int>(f, 6, &ok);
    g.hdop = number<double>(f, 7, &ok);
    g.alt = number<double>(f, 8, &ok);
    return ok ? std::optional(g) : std::nullopt;
}

std::optional<Gsa> decodeGsa(const Fields &f)
{
    bool ok = true;
    Gsa g;
    g.navMode = number<int>(f, 1, &ok).value_or(1);
    for (int i = 2; i < 14; ++i)
        if (const auto sv = number<int>(f, i, &ok))
            g.svids << *sv;
    g.pdop = number<double>(f, 14, &ok);
    if (f.size() > 17) {
        bool sysOk = true;  // неразборчивый systemId — не повод отбросить сообщение
        g.systemId = number<int>(f, 17, &sysOk);
    }
    return ok ? std::optional(g) : std::nullopt;
}

std::optional<Rmc> decodeRmc(const Fields &f)
{
    bool ok = true;
    Rmc r;
    r.utcTime = timeOfDay(f, 0, &ok);
    r.utcSeconds = wholeSeconds(r.utcTime);
    r.active = field(f, 1) == "A";
    r.speedKnots = number<double>(f, 6, &ok);
    r.cog = number<double>(f, 7, &ok);
    return ok ? std::optional(r) : std::nullopt;
}

std::optional<Gsv> decodeGsv(const Fields &f)
{
    if (f.size() < 3)
        return std::nullopt;
    bool ok = true;
    Gsv g;
    // numMsg, msgNum, numSV, затем группы по 4 поля и, в NMEA 4.10+, signalID в конце
    const qsizetype groups = (f.size() - 3) / 4;
    if ((f.size() - 3) % 4 == 1)
        g.signalId = QString::fromLatin1(f.last());
    for (qsizetype n = 0; n < groups; ++n) {
        const qsizetype i = 3 + n * 4;
        const auto svid = number<int>(f, i, &ok);
        if (!svid)
            continue;
        g.sats.append({*svid, number<int>(f, i + 1, &ok), number<int>(f, i + 2, &ok), number<int>(f, i + 3, &ok)});
    }
    return ok ? std::optional(g) : std::nullopt;
}

template <class T>
void store(MessageData *data, const std::optional<T> &value)
{
    if (value)
        *data = *value;
}

}  // namespace

std::optional<GnssMessage> parse(const QByteArray &line)
{
    QByteArray text = line;
    while (text.endsWith('\n') || text.endsWith('\r'))
        text.chop(1);
    const qsizetype star = text.lastIndexOf('*');
    if (!text.startsWith('$') || star < 0 || star + 3 != text.size())
        return std::nullopt;
    const int hi = hexDigit(text[star + 1]), lo = hexDigit(text[star + 2]);
    if (hi < 0 || lo < 0)
        return std::nullopt;
    quint8 sum = 0;
    for (qsizetype i = 1; i < star; ++i) {
        const auto c = quint8(text[i]);
        if (c < 0x20 || c > 0x7e)
            return std::nullopt;
        sum ^= c;
    }
    if (sum != quint8(hi * 16 + lo))
        return std::nullopt;

    QList<QByteArray> parts = text.mid(1, star - 1).split(',');
    const QByteArray address = parts.takeFirst();
    if (address.size() < MinAddress)
        return std::nullopt;
    GnssMessage msg;
    msg.protocol = GnssMessage::Protocol::Nmea;
    msg.raw = line;
    if (address.startsWith('P')) {
        msg.talker = QStringLiteral("P");
        msg.sentence = QString::fromLatin1(address.mid(1));
        // У фирменных u-blox тип уточняет первое поле: $PUBX,00 -> PUBX00
        msg.identity = QString::fromLatin1(address + (parts.isEmpty() ? QByteArray() : parts.first()));
    } else {
        msg.talker = QString::fromLatin1(address.left(2));
        msg.sentence = QString::fromLatin1(address.mid(2));
        msg.identity = QString::fromLatin1(address);
        if (msg.sentence == u"GGA")
            store(&msg.data, decodeGga(parts));
        else if (msg.sentence == u"GSA")
            store(&msg.data, decodeGsa(parts));
        else if (msg.sentence == u"RMC")
            store(&msg.data, decodeRmc(parts));
        else if (msg.sentence == u"GSV")
            store(&msg.data, decodeGsv(parts));
    }
    msg.fields = parts;
    return msg;
}

QString fieldName(const GnssMessage &msg, qsizetype index)
{
    static const QHash<QString, QStringList> names = {
        {QStringLiteral("GGA"), QStringLiteral("time lat NS lon EW quality numSV HDOP alt altUnit sep sepUnit "
                                               "diffAge diffStation")
                                    .split(u' ')},
        {QStringLiteral("RMC"),
         QStringLiteral("time status lat NS lon EW spd cog date mv mvEW posMode navStatus").split(u' ')},
        {QStringLiteral("GSA"), QStringLiteral("opMode navMode sv1 sv2 sv3 sv4 sv5 sv6 sv7 sv8 sv9 sv10 sv11 sv12 "
                                               "PDOP HDOP VDOP systemId")
                                    .split(u' ')},
    };
    if (msg.sentence == u"GSV") {
        // numMsg, msgNum, numSV, группы по 4 поля и, в NMEA 4.10+, signalId в конце
        static const QStringList head = {QStringLiteral("numMsg"), QStringLiteral("msgNum"), QStringLiteral("numSV")};
        static const QStringList group = {QStringLiteral("svid"), QStringLiteral("elv"), QStringLiteral("az"),
                                          QStringLiteral("cno")};
        if (index < head.size())
            return head[index];
        const qsizetype rest = msg.fields.size() - head.size();
        if (rest % 4 == 1 && index == msg.fields.size() - 1)
            return QStringLiteral("signalId");
        const qsizetype n = index - head.size();
        return group[n % 4] + QString::number(n / 4 + 1);
    }
    const QStringList list = names.value(msg.sentence);
    return index < list.size() ? list[index] : QString::number(index + 1);
}

}  // namespace nmea
