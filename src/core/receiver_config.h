// Ключи конфигурации u-blox M9 (Configuration Database), команды конфигурации, разбор ACK и VALGET.
#pragma once

#include "core/messages.h"
#include "core/ubx.h"

#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace receiver_config {

// Идентификаторы ключей из описания интерфейса M9
inline constexpr quint32 RateKey = 0x30210001;       // CFG-RATE-MEAS, мс (U2)
inline constexpr quint32 GpsEnaKey = 0x1031001F;     // CFG-SIGNAL-GPS_ENA (L)
inline constexpr quint32 GalEnaKey = 0x10310021;     // CFG-SIGNAL-GAL_ENA
inline constexpr quint32 BdsEnaKey = 0x10310022;     // CFG-SIGNAL-BDS_ENA
inline constexpr quint32 GloEnaKey = 0x10310025;     // CFG-SIGNAL-GLO_ENA
inline constexpr quint32 NavPvtUsbKey = 0x20910009;  // CFG-MSGOUT-UBX_NAV_PVT_USB (U1, раз в N эпох)
inline constexpr quint32 NavSatUsbKey = 0x20910018;  // CFG-MSGOUT-UBX_NAV_SAT_USB
inline constexpr quint32 NavDopUsbKey = 0x2091003B;  // CFG-MSGOUT-UBX_NAV_DOP_USB
inline constexpr quint32 UsbNmeaKey = 0x10780002;    // CFG-USBOUTPROT-NMEA (L)

// Команды, на которые приёмник отвечает ACK-ACK/ACK-NAK
enum class Command { Valset, FactoryReset };

// (подпись, период измерений в мс)
const QList<std::pair<QString, int>> &rateOptions();
// (подпись, ключ конфигурации)
const QList<std::pair<QString, quint32>> &constellationKeys();
// (подпись, ключ) — вывод сообщений на USB
const QList<std::pair<QString, quint32>> &outputKeys();
// Ключи, текущие значения которых читаются из приёмника для вкладки конфигурации
const QList<quint32> &pollKeys();

// «CFG_RATE_MEAS»; неизвестный — «0x30210001»
QString keyName(quint32 key);
QString commandName(Command command);
quint8 commandId(Command command);

// Сериализует VALSET в слои RAM и BBR, а при flash — и во Flash; пустой массив — ключ неизвестного размера
QByteArray buildValset(const ubx::ConfigItems &items, bool flash = false);
// Запрос CFG-VALGET текущих значений из слоя RAM
QByteArray buildValget(const QList<quint32> &keys);
// CFG-CFG: стереть сохранённую конфигурацию (BBR, Flash) и загрузить заводскую
QByteArray buildFactoryReset();

// Подтверждение команды конфигурации: (команда, ACK — true / NAK — false); nullopt — не оно
std::optional<std::pair<Command, bool>> commandAck(const GnssMessage &msg);
// Значения опрошенных ключей из ответа CFG-VALGET (пусто — не он)
ubx::ConfigValues valgetValues(const GnssMessage &msg);

}  // namespace receiver_config
