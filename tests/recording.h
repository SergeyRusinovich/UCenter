// Эталонная запись для замеров (дорожная карта, фаза 1): синтетический поток M9 на 10 Гц со всеми
// сообщениями, которые разбирает приложение, — NAV-PVT, NAV-DOP, NAV-SAT и NMEA RMC, GGA, GSA, GSV,
// каждое в каждой эпохе (худший случай нагрузки). Детерминированная: одни и те же байты на любой
// платформе, поэтому в репозитории хранится генератор, а не файл.
#pragma once

#include <QByteArray>

namespace recording {

inline constexpr int RateHz = 10;
inline constexpr int ReferenceEpochs = 10 * 60 * RateHz;  // 10 минут

// Сырые байты, как их записывает приложение (.ubx)
QByteArray reference(int epochs = ReferenceEpochs);

}  // namespace recording
