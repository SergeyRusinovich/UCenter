// NMEA 0183: проверка и разбор строки в GnssMessage с разобранными GGA/GSA/RMC/GSV.
//
// Если непустое поле известного предложения не разбирается, содержимое не заполняется
// (msg.data — monostate): такое сообщение показывается в консоли, но в расчётах не участвует.
#pragma once

#include "core/messages.h"

#include <optional>

namespace nmea {

// Строка «$…*hh\r\n» -> сообщение; nullopt — неверный формат или контрольная сумма
std::optional<GnssMessage> parse(const QByteArray &line);

}  // namespace nmea
