#include "core/baseline_store.h"

#include "core/format.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <functional>
#include <utility>

namespace baseline_store {

namespace {

QString optionalFixed(const std::optional<double> &v, int prec)
{
    return v ? fmt::fixed(*v, prec) : QString();
}

using Column = std::pair<const char *, std::function<QString(const Baseline &)>>;

const QList<Column> &csvColumns()
{
    static const QList<Column> columns = {
        {"Saved", [](const Baseline &m) { return m.savedAt; }},
        {"A lat", [](const Baseline &m) { return fmt::fixed(m.aLat, 8); }},
        {"A lon", [](const Baseline &m) { return fmt::fixed(m.aLon, 8); }},
        {"B lat", [](const Baseline &m) { return fmt::fixed(m.bLat, 8); }},
        {"B lon", [](const Baseline &m) { return fmt::fixed(m.bLon, 8); }},
        {"Azimuth A->B (deg)", [](const Baseline &m) { return optionalFixed(m.azimuth, 4); }},
        {"Back azimuth B->A (deg)", [](const Baseline &m) { return optionalFixed(m.backAzimuth, 4); }},
        {"Deviation from true N (deg)",
         [](const Baseline &m) { return m.deviation() ? fmt::signedFixed(*m.deviation(), 4) : QString(); }},
        {"Distance (m)", [](const Baseline &m) { return fmt::fixed(m.distanceM, 3); }},
        {"dNorth (m)", [](const Baseline &m) { return fmt::fixed(m.dNorth(), 3); }},
        {"dEast (m)", [](const Baseline &m) { return fmt::fixed(m.dEast(), 3); }},
    };
    return columns;
}

// Поле CSV в кавычках, если в нём есть разделитель, кавычка или перевод строки
QString csvField(const QString &s)
{
    if (!s.contains(u',') && !s.contains(u'"') && !s.contains(u'\n') && !s.contains(u'\r'))
        return s;
    return u'"' + QString(s).replace(QStringLiteral("\""), QStringLiteral("\"\"")) + u'"';
}

QJsonValue optionalJson(const std::optional<double> &v)
{
    return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}

bool readNumber(const QJsonObject &o, const char *key, double *out, QString *error)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble()) {
        *error = QStringLiteral("field '%1' is missing or not a number").arg(QLatin1String(key));
        return false;
    }
    *out = v.toDouble();
    return true;
}

bool readOptional(const QJsonObject &o, const char *key, std::optional<double> *out, QString *error)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isNull() || v.isUndefined()) {
        out->reset();
        return true;
    }
    double d = 0;
    if (!readNumber(o, key, &d, error))
        return false;
    *out = d;
    return true;
}

}  // namespace

QString writeCsv(const QString &path, const QList<Baseline> &items)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return file.errorString();
    QStringList header;
    for (const auto &[name, _] : csvColumns())
        header << QLatin1String(name);
    QString text = header.join(u',') + QStringLiteral("\r\n");
    for (const Baseline &m : items) {
        QStringList row;
        for (const auto &[_, format] : csvColumns())
            row << csvField(format(m));
        text += row.join(u',') + QStringLiteral("\r\n");
    }
    if (file.write(text.toUtf8()) < 0)
        return file.errorString();
    return {};
}

QString saveJson(const QString &path, const QList<Baseline> &items)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QStringLiteral("cannot create directory %1").arg(QFileInfo(path).absolutePath());
    QJsonArray array;
    for (const Baseline &m : items) {
        array.append(QJsonObject{
            {"a_lat", m.aLat},
            {"a_lon", m.aLon},
            {"b_lat", m.bLat},
            {"b_lon", m.bLon},
            {"distance_m", m.distanceM},
            {"azimuth", optionalJson(m.azimuth)},
            {"back_azimuth", optionalJson(m.backAzimuth)},
            {"saved_at", m.savedAt},
        });
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return file.errorString();
    const QJsonObject root{
        {"format", QStringLiteral("ucenter-measurements")},
        {"version", FormatVersion},
        {"measurements", array},
    };
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return file.errorString();
    return {};
}

QString loadJson(const QString &path, QList<Baseline> *items)
{
    items->clear();
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return file.errorString();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return parseError.errorString();
    QJsonArray array;
    if (doc.isArray()) {
        array = doc.array();  // версия 1: список замеров без обёртки
    } else if (doc.isObject()) {
        const QJsonObject root = doc.object();
        const int version = root.value(QLatin1String("version")).toInt(0);
        if (version > FormatVersion)
            return QStringLiteral(
                       "the file was written by a newer version of UCenter (format %1, this version reads up "
                       "to %2)")
                .arg(version)
                .arg(FormatVersion);
        if (version < 2 || !root.value(QLatin1String("measurements")).isArray())
            return QStringLiteral("a list of measurements is expected");
        array = root.value(QLatin1String("measurements")).toArray();
    } else {
        return QStringLiteral("a list of measurements is expected");
    }
    QList<Baseline> result;
    QString error;
    for (const auto &v : std::as_const(array)) {
        if (!v.isObject())
            return QStringLiteral("a measurement must be an object");
        const QJsonObject o = v.toObject();
        Baseline m;
        if (!readNumber(o, "a_lat", &m.aLat, &error) || !readNumber(o, "a_lon", &m.aLon, &error)
            || !readNumber(o, "b_lat", &m.bLat, &error) || !readNumber(o, "b_lon", &m.bLon, &error)
            || !readNumber(o, "distance_m", &m.distanceM, &error) || !readOptional(o, "azimuth", &m.azimuth, &error)
            || !readOptional(o, "back_azimuth", &m.backAzimuth, &error))
            return error;
        m.savedAt = o.value(QLatin1String("saved_at")).toString();
        result.append(m);
    }
    *items = result;
    return {};
}

QString setAside(const QString &path, QString *error)
{
    QString target = path + QStringLiteral(".unreadable-")
                     + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QFile file(path);
    if (!file.rename(target)) {
        *error = file.errorString();
        return {};
    }
    return target;
}

QString moveFromLegacy(const QString &legacyPath, const QString &path)
{
    if (!QFile::exists(legacyPath) || QFile::exists(path))
        return {};
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir))
        return QStringLiteral("cannot create directory %1").arg(dir);
    QFile file(legacyPath);
    if (!file.rename(path))
        return file.errorString();
    return {};
}

}  // namespace baseline_store
