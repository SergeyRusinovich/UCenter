// UBX: кадры (сборка, контрольная сумма), имена сообщений, разбор NAV-PVT/NAV-SAT/NAV-DOP/
// ACK/CFG-VALGET и сборка CFG-VALSET/VALGET/CFG-CFG для приёмников M9.
#pragma once

#include "core/messages.h"

#include <QByteArray>
#include <QList>

#include <utility>

namespace ubx {

inline constexpr quint8 Sync1 = 0xB5;
inline constexpr quint8 Sync2 = 0x62;
inline constexpr int HeaderSize = 6;     // sync, класс, id, длина
inline constexpr int FrameOverhead = 8;  // заголовок + контрольная сумма

inline constexpr quint8 ClassNav = 0x01, ClassAck = 0x05, ClassCfg = 0x06;
inline constexpr quint8 IdNavDop = 0x04, IdNavPvt = 0x07, IdNavSat = 0x35;
inline constexpr quint8 IdAckAck = 0x01, IdAckNak = 0x00;
inline constexpr quint8 IdCfgCfg = 0x09, IdCfgValset = 0x8A, IdCfgValget = 0x8B;

// Слои конфигурации для VALSET
inline constexpr quint8 LayerRam = 0x01, LayerBbr = 0x02, LayerFlash = 0x04;
// Слой для VALGET
inline constexpr quint8 PollLayerRam = 0;

using ConfigItems = QList<std::pair<quint32, quint64>>;  // ключ, значение

bool isUbx(const QByteArray &raw);
// Контрольная сумма Флетчера по классу, id, длине и нагрузке
std::pair<quint8, quint8> checksum(const QByteArray &data);
QByteArray frame(quint8 cls, quint8 id, const QByteArray &payload);
// Проверенный кадр целиком -> сообщение с разобранным содержимым
// (контрольную сумму проверяет FrameReader)
GnssMessage message(const QByteArray &raw);

// «NAV-PVT»; неизвестные — «0x01-0x99»
QString messageName(quint8 cls, quint8 id);
QString className(quint8 cls);

// Размер значения ключа конфигурации в байтах (по битам 28–30 ключа); 0 — неизвестен
int keySize(quint32 key);
// Ошибка — пустой массив (ключ неизвестного размера)
QByteArray buildValset(quint8 layers, const ConfigItems &items);
QByteArray buildValget(quint8 layer, const QList<quint32> &keys);
// CFG-CFG: очистить, сохранить и загрузить разделы конфигурации на заданных носителях
QByteArray buildCfgCfg(quint32 clearMask, quint32 saveMask, quint32 loadMask, quint8 deviceMask);

// Текст для консоли в духе pyubx2: «<UBX(NAV-PVT, iTOW=…, …)>»
QString describe(const GnssMessage &msg);

}  // namespace ubx

Q_DECLARE_METATYPE(ubx::ConfigItems)
Q_DECLARE_METATYPE(ubx::ConfigValues)
