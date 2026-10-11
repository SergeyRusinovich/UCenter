// Форматирование времени, поправка часового пояса и монотонные часы.
#pragma once

#include <QList>
#include <QString>

namespace timeutil {

inline constexpr int Day = 86400;

// От UTC-12 до UTC+14 с шагом 30 минут
const QList<double> &utcOffsets();

QString formatHms(qint64 seconds);
// Длительность сеанса: «чч:мм:сс», с днями — «Nd чч:мм:сс»
QString formatDuration(double seconds);
QString offsetLabel(double hours);
// Смещение часового пояса системы, округлённое до ближайшего из utcOffsets()
double systemUtcOffset();
// Секунды от полуночи UTC -> секунды от полуночи по местному времени
int toLocal(int utcSeconds, double offsetHours);

// Секунды от произвольной точки отсчёта, не зависят от перевода часов
double monotonic();

}  // namespace timeutil
