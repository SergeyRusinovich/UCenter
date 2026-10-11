#include "core/places.h"

#include <QHash>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>

namespace places {

namespace {

// Число из JSON: Nominatim отдаёт координаты строками, GeoJSON — числами
bool toDouble(const QJsonValue &v, double *out)
{
    if (v.isDouble()) {
        *out = v.toDouble();
        return true;
    }
    bool ok = false;
    if (v.isString())
        *out = v.toString().toDouble(&ok);
    return ok;
}

// GeoJSON: [lon, lat] -> LatLon
bool toRing(const QJsonValue &coords, Ring *ring)
{
    if (!coords.isArray())
        return false;
    for (const auto &pt : coords.toArray()) {
        const QJsonArray xy = pt.toArray();
        LatLon ll;
        if (xy.size() < 2 || !toDouble(xy[0], &ll.lon) || !toDouble(xy[1], &ll.lat))
            return false;
        ring->append(ll);
    }
    return true;
}

// GeoJSON -> полигоны и линии. Дыры полигонов не рисуем — берём внешний контур.
// Point или нет геометрии: только флажок.
bool readGeometry(const QJsonValue &geo, QList<Ring> *areas, QList<Ring> *lines)
{
    const QJsonObject obj = geo.toObject();
    const QString kind = obj.value(QLatin1String("type")).toString();
    const QJsonValue coords = obj.value(QLatin1String("coordinates"));
    Ring ring;
    if (kind == u"Polygon") {
        if (!toRing(coords.toArray().at(0), &ring))
            return false;
        areas->append(ring);
    } else if (kind == u"MultiPolygon") {
        if (!coords.isArray())
            return false;
        for (const auto &poly : coords.toArray()) {
            ring.clear();
            if (!toRing(poly.toArray().at(0), &ring))
                return false;
            areas->append(ring);
        }
    } else if (kind == u"LineString") {
        if (!toRing(coords, &ring))
            return false;
        lines->append(ring);
    } else if (kind == u"MultiLineString") {
        if (!coords.isArray())
            return false;
        for (const auto &line : coords.toArray()) {
            ring.clear();
            if (!toRing(line, &ring))
                return false;
            lines->append(ring);
        }
    }
    return true;
}

bool isDigits(const QString &s)
{
    return !s.isEmpty() && std::all_of(s.begin(), s.end(), [](QChar c) { return c.isDigit(); });
}

}  // namespace

QList<Place> parseNominatim(const QJsonArray &data)
{
    QList<Place> result;
    for (const auto &v : data) {
        // Неполная запись — пропускаем, остальные показываем
        const QJsonObject item = v.toObject();
        const QJsonArray bb = item.value(QLatin1String("boundingbox")).toArray();
        Place pl;
        double south = 0, north = 0, west = 0, east = 0;
        if (bb.size() != 4 || !toDouble(bb[0], &south) || !toDouble(bb[1], &north) || !toDouble(bb[2], &west)
            || !toDouble(bb[3], &east))
            continue;
        pl.bbox = {south, west, north, east};
        if (!toDouble(item.value(QLatin1String("lat")), &pl.lat)
            || !toDouble(item.value(QLatin1String("lon")), &pl.lon))
            continue;
        if (!readGeometry(item.value(QLatin1String("geojson")), &pl.areas, &pl.lines))
            continue;
        pl.address = item.value(QLatin1String("display_name")).toString();
        pl.name = item.value(QLatin1String("name")).toString();
        if (pl.name.isEmpty())
            pl.name = pl.address.section(u',', 0, 0);
        result.append(pl);
    }
    return result;
}

QList<Place> mergeDuplicates(const QList<Place> &places)
{
    QList<Place> merged;
    QHash<QString, qsizetype> index;  // ключ объекта -> позиция в merged
    for (const Place &pl : places) {
        // Куски одной улицы различаются лишь числовыми частями адреса (индекс, номер квартала)
        QStringList parts;
        for (const QString &part : pl.address.split(QStringLiteral(", ")))
            if (!isDigits(part))
                parts << part;
        const QString key = pl.name + u'\x1f' + parts.join(u'\x1f');
        const auto it = index.constFind(key);
        if (it == index.cend()) {
            index.insert(key, merged.size());
            merged.append(pl);
            continue;
        }
        Place &first = merged[*it];
        first.bbox = {std::min(first.bbox.south, pl.bbox.south), std::min(first.bbox.west, pl.bbox.west),
                      std::max(first.bbox.north, pl.bbox.north), std::max(first.bbox.east, pl.bbox.east)};
        first.areas += pl.areas;
        first.lines += pl.lines;
    }
    return merged;
}

QList<Place> sortByDistance(QList<Place> places, LatLon center)
{
    std::stable_sort(places.begin(), places.end(), [center](const Place &a, const Place &b) {
        return geo::inverse(center, {a.lat, a.lon}).distance < geo::inverse(center, {b.lat, b.lon}).distance;
    });
    return places;
}

}  // namespace places
