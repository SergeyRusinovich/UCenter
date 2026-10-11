#include "core/timeutil.h"

#include <QDateTime>
#include <QElapsedTimer>

#include <algorithm>
#include <cmath>

namespace timeutil {

namespace {

// Остаток по модулю, неотрицательный при отрицательном делимом
qint64 floorMod(qint64 a, qint64 b)
{
    const qint64 r = a % b;
    return r < 0 ? r + b : r;
}

}  // namespace

const QList<double> &utcOffsets()
{
    static const QList<double> offsets = [] {
        QList<double> list;
        for (int halfHours = -24; halfHours < 29; ++halfHours)
            list << halfHours / 2.0;
        return list;
    }();
    return offsets;
}

QString formatHms(qint64 seconds)
{
    const qint64 s = floorMod(seconds, Day);
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600, 2, 10, QChar(u'0'))
        .arg(s / 60 % 60, 2, 10, QChar(u'0'))
        .arg(s % 60, 2, 10, QChar(u'0'));
}

QString formatDuration(double seconds)
{
    const auto total = qint64(seconds);
    const qint64 days = total / Day, rem = total % Day;
    return days ? QStringLiteral("%1d %2").arg(days).arg(formatHms(rem)) : formatHms(rem);
}

QString offsetLabel(double hours)
{
    const QChar sign = hours >= 0 ? u'+' : u'-';
    const int total = int(std::abs(hours) * 60);
    return QStringLiteral("UTC%1%2:%3")
        .arg(sign)
        .arg(total / 60, 2, 10, QChar(u'0'))
        .arg(total % 60, 2, 10, QChar(u'0'));
}

double systemUtcOffset()
{
    const double local = std::round(QDateTime::currentDateTime().offsetFromUtc() / 1800.0) / 2;
    return std::clamp(local, utcOffsets().first(), utcOffsets().last());
}

int toLocal(int utcSeconds, double offsetHours)
{
    return int(floorMod(utcSeconds + qint64(offsetHours * 3600), Day));
}

double monotonic()
{
    static QElapsedTimer timer = [] {
        QElapsedTimer t;
        t.start();
        return t;
    }();
    return double(timer.nsecsElapsed()) / 1e9;
}

}  // namespace timeutil
