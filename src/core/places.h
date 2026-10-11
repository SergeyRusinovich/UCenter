// Результаты поиска мест (геокодер Nominatim, формат jsonv2): разбор в типизированные Place.
#pragma once

#include "core/geo.h"

#include <QJsonArray>
#include <QList>
#include <QString>

using Ring = QList<LatLon>;  // последовательность точек контура или линии

struct Place {
    QString name;     // короткое имя («Красная площадь»)
    QString address;  // полное название с адресом
    double lat = 0.0;
    double lon = 0.0;
    Bounds bbox;
    QList<Ring> areas;  // внешние контуры полигонов (подсветка заливкой)
    QList<Ring> lines;  // линии (улицы, реки) — подсветка обводкой
};

namespace places {

QList<Place> parseNominatim(const QJsonArray &data);
// Склеивает записи одного объекта (улица в OSM — несколько кусков) в одну
QList<Place> mergeDuplicates(const QList<Place> &places);
// Ближние к center — первыми (Nominatim ранжирует по известности, а на месте важнее близость)
QList<Place> sortByDistance(QList<Place> places, LatLon center);

}  // namespace places
