#include "core/receiver_config.h"

#include <QHash>

namespace receiver_config {

namespace {

// Все разделы конфигурации (ioPort … ftsConf) и все носители: BBR, Flash, EEPROM, SPI Flash
constexpr quint32 AllSections = 0x0000FFFF;
constexpr quint8 AllDevices = 0x17;

}  // namespace

const QList<std::pair<QString, int>> &rateOptions()
{
    static const QList<std::pair<QString, int>> options = {
        {QStringLiteral("1 Hz"), 1000}, {QStringLiteral("5 Hz"), 200}, {QStringLiteral("10 Hz"), 100}};
    return options;
}

const QList<std::pair<QString, quint32>> &constellationKeys()
{
    static const QList<std::pair<QString, quint32>> keys = {
        {QStringLiteral("GPS"), GpsEnaKey},
        {QStringLiteral("GLONASS"), GloEnaKey},
        {QStringLiteral("Galileo"), GalEnaKey},
        {QStringLiteral("BeiDou"), BdsEnaKey},
    };
    return keys;
}

const QList<std::pair<QString, quint32>> &outputKeys()
{
    static const QList<std::pair<QString, quint32>> keys = {
        {QStringLiteral("UBX-NAV-PVT"), NavPvtUsbKey},
        {QStringLiteral("UBX-NAV-SAT"), NavSatUsbKey},
        {QStringLiteral("UBX-NAV-DOP"), NavDopUsbKey},
        {QStringLiteral("NMEA"), UsbNmeaKey},
    };
    return keys;
}

const QList<quint32> &pollKeys()
{
    static const QList<quint32> keys = [] {
        QList<quint32> k{RateKey};
        for (const auto &[_, key] : constellationKeys())
            k << key;
        for (const auto &[_, key] : outputKeys())
            k << key;
        return k;
    }();
    return keys;
}

QString keyName(quint32 key)
{
    static const QHash<quint32, QString> names = {
        {RateKey, "CFG_RATE_MEAS"},
        {GpsEnaKey, "CFG_SIGNAL_GPS_ENA"},
        {GloEnaKey, "CFG_SIGNAL_GLO_ENA"},
        {GalEnaKey, "CFG_SIGNAL_GAL_ENA"},
        {BdsEnaKey, "CFG_SIGNAL_BDS_ENA"},
        {NavPvtUsbKey, "CFG_MSGOUT_UBX_NAV_PVT_USB"},
        {NavSatUsbKey, "CFG_MSGOUT_UBX_NAV_SAT_USB"},
        {NavDopUsbKey, "CFG_MSGOUT_UBX_NAV_DOP_USB"},
        {UsbNmeaKey, "CFG_USBOUTPROT_NMEA"},
    };
    return names.value(key, QStringLiteral("0x%1").arg(key, 8, 16, QChar(u'0')));
}

QString commandName(Command command)
{
    return command == Command::Valset ? QStringLiteral("CFG-VALSET") : QStringLiteral("CFG-CFG");
}

quint8 commandId(Command command)
{
    return command == Command::Valset ? ubx::IdCfgValset : ubx::IdCfgCfg;
}

QByteArray buildValset(const ubx::ConfigItems &items, bool flash)
{
    const quint8 layers = ubx::LayerRam | ubx::LayerBbr | (flash ? ubx::LayerFlash : 0);
    return ubx::buildValset(layers, items);
}

QByteArray buildValget(const QList<quint32> &keys)
{
    return ubx::buildValget(ubx::PollLayerRam, keys);
}

QByteArray buildFactoryReset()
{
    return ubx::buildCfgCfg(AllSections, 0, AllSections, AllDevices);
}

std::optional<std::pair<Command, bool>> commandAck(const GnssMessage &msg)
{
    const auto *ack = msg.get<ubx::Ack>();
    if (!ack || ack->cls != ubx::ClassCfg)
        return std::nullopt;
    for (Command c : {Command::Valset, Command::FactoryReset})
        if (ack->id == commandId(c))
            return std::pair{c, ack->ok};
    return std::nullopt;
}

ubx::ConfigValues valgetValues(const GnssMessage &msg)
{
    ubx::ConfigValues result;
    if (const auto *valget = msg.get<ubx::Valget>()) {
        for (quint32 key : pollKeys())
            if (valget->values.contains(key))
                result.insert(key, valget->values.value(key));
    }
    return result;
}

}  // namespace receiver_config
