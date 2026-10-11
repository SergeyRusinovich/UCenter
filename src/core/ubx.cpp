#include "core/ubx.h"

#include "core/receiver_config.h"

#include <QHash>
#include <QStringList>
#include <QtEndian>

#include <optional>

namespace ubx {

namespace {

const QHash<quint8, QString> &classNames()
{
    static const QHash<quint8, QString> names = {
        {0x01, "NAV"}, {0x02, "RXM"}, {0x04, "INF"}, {0x05, "ACK"}, {0x06, "CFG"}, {0x09, "UPD"},
        {0x0A, "MON"}, {0x0D, "TIM"}, {0x10, "ESF"}, {0x13, "MGA"}, {0x21, "LOG"}, {0x27, "SEC"},
    };
    return names;
}

// Сообщения, которые выдают или принимают приёмники M9
const QHash<quint16, QString> &messageNames()
{
    static const QHash<quint16, QString> names = {
        {0x0101, "POSECEF"}, {0x0102, "POSLLH"},   {0x0103, "STATUS"},  {0x0104, "DOP"},       {0x0107, "PVT"},
        {0x0109, "ODO"},     {0x0111, "VELECEF"},  {0x0112, "VELNED"},  {0x0113, "HPPOSECEF"}, {0x0114, "HPPOSLLH"},
        {0x0120, "TIMEGPS"}, {0x0121, "TIMEUTC"},  {0x0122, "CLOCK"},   {0x0123, "TIMEGLO"},   {0x0124, "TIMEBDS"},
        {0x0125, "TIMEGAL"}, {0x0126, "TIMELS"},   {0x0132, "SBAS"},    {0x0134, "ORB"},       {0x0135, "SAT"},
        {0x0136, "COV"},     {0x0139, "GEOFENCE"}, {0x0142, "SLAS"},    {0x0143, "SIG"},       {0x0160, "AOPSTATUS"},
        {0x0161, "EOE"},     {0x0213, "SFRBX"},    {0x0214, "MEASX"},   {0x0215, "RAWX"},      {0x0241, "PMREQ"},
        {0x0259, "RLM"},     {0x0400, "ERROR"},    {0x0401, "WARNING"}, {0x0402, "NOTICE"},    {0x0403, "TEST"},
        {0x0404, "DEBUG"},   {0x0500, "NAK"},      {0x0501, "ACK"},     {0x0600, "PRT"},       {0x0601, "MSG"},
        {0x0604, "RST"},     {0x0608, "RATE"},     {0x0609, "CFG"},     {0x068A, "VALSET"},    {0x068B, "VALGET"},
        {0x068C, "VALDEL"},  {0x0A02, "IO"},       {0x0A04, "VER"},     {0x0A06, "MSGPP"},     {0x0A07, "RXBUF"},
        {0x0A08, "TXBUF"},   {0x0A09, "HW"},       {0x0A0B, "HW2"},     {0x0A28, "GNSS"},      {0x0A31, "SPAN"},
        {0x0A36, "COMMS"},   {0x0A37, "HW3"},      {0x0A38, "RF"},      {0x0D01, "TP"},        {0x0D03, "TM2"},
        {0x0D06, "VRFY"},    {0x2703, "UNIQID"},
    };
    return names;
}

quint8 u1(const QByteArray &p, qsizetype at)
{
    return quint8(p[at]);
}
quint16 u2(const QByteArray &p, qsizetype at)
{
    return qFromLittleEndian<quint16>(p.constData() + at);
}
qint16 i2(const QByteArray &p, qsizetype at)
{
    return qFromLittleEndian<qint16>(p.constData() + at);
}
quint32 u4(const QByteArray &p, qsizetype at)
{
    return qFromLittleEndian<quint32>(p.constData() + at);
}
qint32 i4(const QByteArray &p, qsizetype at)
{
    return qFromLittleEndian<qint32>(p.constData() + at);
}
quint64 u8(const QByteArray &p, qsizetype at)
{
    return qFromLittleEndian<quint64>(p.constData() + at);
}

void appendLe(QByteArray *out, quint64 value, int size)
{
    for (int i = 0; i < size; ++i)
        out->append(char((value >> (8 * i)) & 0xFF));
}

// gnssId из NAV-SAT -> буква системы; 0 — система, которую не показываем
char gnssSystem(quint8 gnssId)
{
    switch (gnssId) {
    case 0: return 'G';
    case 1: return 'S';
    case 2: return 'E';
    case 3: return 'B';
    case 5: return 'Q';
    case 6: return 'R';
    default: return 0;
    }
}

std::optional<NavPvt> decodeNavPvt(const QByteArray &p)
{
    if (p.size() < 92)
        return std::nullopt;
    NavPvt v;
    v.iTow = u4(p, 0);
    v.year = u2(p, 4);
    v.month = u1(p, 6);
    v.day = u1(p, 7);
    v.hour = u1(p, 8);
    v.min = u1(p, 9);
    v.second = u1(p, 10);
    const quint8 valid = u1(p, 11);
    v.validDate = valid & 0x01;
    v.validTime = valid & 0x02;
    v.fullyResolved = valid & 0x04;
    v.fixType = u1(p, 20);
    const quint8 flags = u1(p, 21);
    v.gnssFixOk = flags & 0x01;
    v.diffSoln = flags & 0x02;
    v.headVehValid = flags & 0x20;
    v.carrSoln = flags >> 6;
    v.numSv = u1(p, 23);
    v.lon = i4(p, 24) * 1e-7;
    v.lat = i4(p, 28) * 1e-7;
    v.height = i4(p, 32);
    v.hMsl = i4(p, 36);
    v.hAcc = u4(p, 40);
    v.vAcc = u4(p, 44);
    v.velN = i4(p, 48);
    v.velE = i4(p, 52);
    v.velD = i4(p, 56);
    v.gSpeed = i4(p, 60);
    v.headMot = i4(p, 64) * 1e-5;
    v.sAcc = u4(p, 68);
    v.headAcc = u4(p, 72) * 1e-5;
    v.pDop = u2(p, 76) * 0.01;
    v.headVeh = i4(p, 84) * 1e-5;
    return v;
}

std::optional<NavSat> decodeNavSat(const QByteArray &p)
{
    if (p.size() < 8)
        return std::nullopt;
    const int count = u1(p, 5);
    if (p.size() < 8 + 12 * count)
        return std::nullopt;
    NavSat v;
    v.iTow = u4(p, 0);
    for (int n = 0; n < count; ++n) {
        const qsizetype at = 8 + 12 * qsizetype(n);
        const char sys = gnssSystem(u1(p, at));
        if (!sys)
            continue;
        NavSatSv sv;
        sv.sys = sys;
        sv.svid = u1(p, at + 1);
        if (const int cno = u1(p, at + 2))
            sv.cno = cno;
        int elev = u1(p, at + 3);  // I1: байт со знаком
        if (elev > 127)
            elev -= 256;
        if (elev >= -90 && elev <= 90) {  // вне диапазона — положение неизвестно
            sv.elv = elev;
            sv.az = i2(p, at + 4);
        }
        sv.used = u4(p, at + 8) & 0x08;
        v.svs.append(sv);
    }
    return v;
}

std::optional<NavDop> decodeNavDop(const QByteArray &p)
{
    if (p.size() < 18)
        return std::nullopt;
    NavDop v;
    v.iTow = u4(p, 0);
    v.gDop = u2(p, 4) * 0.01;
    v.pDop = u2(p, 6) * 0.01;
    v.tDop = u2(p, 8) * 0.01;
    v.vDop = u2(p, 10) * 0.01;
    v.hDop = u2(p, 12) * 0.01;
    v.nDop = u2(p, 14) * 0.01;
    v.eDop = u2(p, 16) * 0.01;
    return v;
}

std::optional<Valget> decodeValget(const QByteArray &p)
{
    if (p.size() < 4)
        return std::nullopt;
    Valget v;
    v.layer = u1(p, 1);
    qsizetype at = 4;
    while (at + 4 <= p.size()) {
        const quint32 key = u4(p, at);
        const int size = keySize(key);
        at += 4;
        if (size == 0 || at + size > p.size())
            return std::nullopt;  // без размера дальше не разобрать
        switch (size) {
        case 1: v.values.insert(key, u1(p, at)); break;
        case 2: v.values.insert(key, u2(p, at)); break;
        case 4: v.values.insert(key, u4(p, at)); break;
        default: v.values.insert(key, qint64(u8(p, at))); break;
        }
        at += size;
    }
    return v;
}

template <class T>
void store(MessageData *data, const std::optional<T> &value)
{
    if (value)
        *data = *value;
}

QString number(double v)
{
    return QString::number(v, 'g', 10);
}

QString describeNavPvt(const NavPvt &p)
{
    const QList<std::pair<const char *, QString>> fields = {
        {"iTOW", QString::number(p.iTow)},
        {"year", QString::number(p.year)},
        {"month", QString::number(p.month)},
        {"day", QString::number(p.day)},
        {"hour", QString::number(p.hour)},
        {"min", QString::number(p.min)},
        {"second", QString::number(p.second)},
        {"validDate", QString::number(p.validDate)},
        {"validTime", QString::number(p.validTime)},
        {"fullyResolved", QString::number(p.fullyResolved)},
        {"fixType", QString::number(p.fixType)},
        {"gnssFixOk", QString::number(p.gnssFixOk)},
        {"diffSoln", QString::number(p.diffSoln)},
        {"headVehValid", QString::number(p.headVehValid)},
        {"carrSoln", QString::number(p.carrSoln)},
        {"numSV", QString::number(p.numSv)},
        {"lon", number(p.lon)},
        {"lat", number(p.lat)},
        {"height", QString::number(p.height)},
        {"hMSL", QString::number(p.hMsl)},
        {"hAcc", QString::number(p.hAcc)},
        {"vAcc", QString::number(p.vAcc)},
        {"velN", QString::number(p.velN)},
        {"velE", QString::number(p.velE)},
        {"velD", QString::number(p.velD)},
        {"gSpeed", QString::number(p.gSpeed)},
        {"headMot", number(p.headMot)},
        {"sAcc", QString::number(p.sAcc)},
        {"headAcc", number(p.headAcc)},
        {"pDOP", number(p.pDop)},
        {"headVeh", number(p.headVeh)},
    };
    QStringList parts{QStringLiteral("NAV-PVT")};
    for (const auto &[name, value] : fields)
        parts << QLatin1String(name) + u'=' + value;
    return parts.join(QStringLiteral(", "));
}

QString describeNavSat(const NavSat &s)
{
    QStringList parts{QStringLiteral("NAV-SAT"), QStringLiteral("iTOW=%1").arg(s.iTow),
                      QStringLiteral("numSvs=%1").arg(s.svs.size())};
    for (const NavSatSv &sv : s.svs) {
        parts << QStringLiteral("%1%2(cno=%3 elev=%4 azim=%5%6)")
                     .arg(QLatin1Char(sv.sys))
                     .arg(sv.svid)
                     .arg(sv.cno.value_or(0))
                     .arg(sv.elv ? QString::number(*sv.elv) : QStringLiteral("?"))
                     .arg(sv.az ? QString::number(*sv.az) : QStringLiteral("?"))
                     .arg(sv.used ? QStringLiteral(" used") : QString());
    }
    return parts.join(QStringLiteral(", "));
}

QString describeNavDop(const NavDop &d)
{
    return QStringLiteral("NAV-DOP, iTOW=%1, gDOP=%2, pDOP=%3, tDOP=%4, vDOP=%5, hDOP=%6, nDOP=%7, eDOP=%8")
        .arg(d.iTow)
        .arg(number(d.gDop), number(d.pDop), number(d.tDop), number(d.vDop), number(d.hDop), number(d.nDop),
             number(d.eDop));
}

QString describeValget(const GnssMessage &msg, const Valget &v)
{
    const QByteArray p = msg.payload();
    QStringList parts{QStringLiteral("CFG-VALGET"), QStringLiteral("version=%1").arg(u1(p, 0)),
                      QStringLiteral("layer=%1").arg(v.layer), QStringLiteral("position=%1").arg(u2(p, 2))};
    for (auto it = v.values.cbegin(); it != v.values.cend(); ++it)
        parts << QStringLiteral("%1=%2").arg(receiver_config::keyName(it.key())).arg(it.value());
    return parts.join(QStringLiteral(", "));
}

}  // namespace

bool isUbx(const QByteArray &raw)
{
    return raw.size() >= 2 && quint8(raw[0]) == Sync1 && quint8(raw[1]) == Sync2;
}

std::pair<quint8, quint8> checksum(const QByteArray &data)
{
    quint8 a = 0, b = 0;
    for (char c : data) {
        a += quint8(c);
        b += a;
    }
    return {a, b};
}

QByteArray frame(quint8 cls, quint8 id, const QByteArray &payload)
{
    QByteArray body;
    body.append(char(cls));
    body.append(char(id));
    appendLe(&body, quint64(payload.size()), 2);
    body.append(payload);
    const auto [a, b] = checksum(body);
    QByteArray out;
    out.append(char(Sync1));
    out.append(char(Sync2));
    out.append(body);
    out.append(char(a));
    out.append(char(b));
    return out;
}

GnssMessage message(const QByteArray &raw)
{
    GnssMessage msg;
    msg.protocol = GnssMessage::Protocol::Ubx;
    msg.raw = raw;
    msg.cls = quint8(raw[2]);
    msg.id = quint8(raw[3]);
    msg.identity = messageName(msg.cls, msg.id);
    const QByteArray p = msg.payload();
    if (msg.cls == ClassNav && msg.id == IdNavPvt)
        store(&msg.data, decodeNavPvt(p));
    else if (msg.cls == ClassNav && msg.id == IdNavSat)
        store(&msg.data, decodeNavSat(p));
    else if (msg.cls == ClassNav && msg.id == IdNavDop)
        store(&msg.data, decodeNavDop(p));
    else if (msg.cls == ClassAck && (msg.id == IdAckAck || msg.id == IdAckNak) && p.size() >= 2)
        msg.data = Ack{msg.id == IdAckAck, u1(p, 0), u1(p, 1)};
    else if (msg.cls == ClassCfg && msg.id == IdCfgValget)
        store(&msg.data, decodeValget(p));
    return msg;
}

QString className(quint8 cls)
{
    return classNames().value(cls, QStringLiteral("0x%1").arg(cls, 2, 16, QChar(u'0')));
}

QString messageName(quint8 cls, quint8 id)
{
    const QString name = messageNames().value(quint16(cls << 8 | id));
    if (!name.isEmpty())
        return className(cls) + u'-' + name;
    return QStringLiteral("0x%1-0x%2").arg(cls, 2, 16, QChar(u'0')).arg(id, 2, 16, QChar(u'0'));
}

int keySize(quint32 key)
{
    switch ((key >> 28) & 0x7) {
    case 1:  // L: один бит, передаётся байтом
    case 2: return 1;
    case 3: return 2;
    case 4: return 4;
    case 5: return 8;
    default: return 0;
    }
}

QByteArray buildValset(quint8 layers, const ConfigItems &items)
{
    QByteArray payload;
    payload.append(char(0));  // version 0: без транзакций
    payload.append(char(layers));
    payload.append(2, char(0));  // reserved
    for (const auto &[key, value] : items) {
        const int size = keySize(key);
        if (size == 0)
            return {};
        appendLe(&payload, key, 4);
        appendLe(&payload, value, size);
    }
    return frame(ClassCfg, IdCfgValset, payload);
}

QByteArray buildValget(quint8 layer, const QList<quint32> &keys)
{
    QByteArray payload;
    payload.append(char(0));  // version 0: запрос
    payload.append(char(layer));
    payload.append(2, char(0));  // position
    for (quint32 key : keys)
        appendLe(&payload, key, 4);
    return frame(ClassCfg, IdCfgValget, payload);
}

QByteArray buildCfgCfg(quint32 clearMask, quint32 saveMask, quint32 loadMask, quint8 deviceMask)
{
    QByteArray payload;
    appendLe(&payload, clearMask, 4);
    appendLe(&payload, saveMask, 4);
    appendLe(&payload, loadMask, 4);
    payload.append(char(deviceMask));
    return frame(ClassCfg, IdCfgCfg, payload);
}

QString describe(const GnssMessage &msg)
{
    QString body;
    if (const auto *pvt = msg.get<NavPvt>()) {
        body = describeNavPvt(*pvt);
    } else if (const auto *sat = msg.get<NavSat>()) {
        body = describeNavSat(*sat);
    } else if (const auto *dop = msg.get<NavDop>()) {
        body = describeNavDop(*dop);
    } else if (const auto *ack = msg.get<Ack>()) {
        body = QStringLiteral("%1, clsID=%2, msgID=%3")
                   .arg(msg.identity, className(ack->cls), messageName(ack->cls, ack->id));
    } else if (const auto *valget = msg.get<Valget>()) {
        body = describeValget(msg, *valget);
    } else if (msg.cls == 0x04) {  // INF-*: текстовое сообщение приёмника
        body = QStringLiteral("%1, msgContent=%2").arg(msg.identity, QString::fromLatin1(msg.payload()));
    } else {
        body = QStringLiteral("%1, length=%2").arg(msg.identity).arg(msg.raw.size() - FrameOverhead);
    }
    return QStringLiteral("<UBX(%1)>").arg(body);
}

}  // namespace ubx
