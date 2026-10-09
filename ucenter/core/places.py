"""Результаты поиска мест (геокодер Nominatim, формат jsonv2): разбор в типизированные Place."""
from dataclasses import dataclass, field, replace

from ucenter.core.geo import inverse

Ring = tuple[tuple[float, float], ...]  # последовательность (широта, долгота)


@dataclass(frozen=True)
class Place:
    name: str                  # короткое имя («Красная площадь»)
    address: str               # полное название с адресом
    lat: float
    lon: float
    bbox: tuple[float, float, float, float]  # юг, запад, север, восток
    areas: tuple[Ring, ...] = field(default=())  # внешние контуры полигонов (подсветка заливкой)
    lines: tuple[Ring, ...] = field(default=())  # линии (улицы, реки) — подсветка обводкой


def _ring(coords) -> Ring:
    return tuple((float(lat), float(lon)) for lon, lat, *_ in coords)  # GeoJSON: [lon, lat]


def _geometry(geo: dict | None) -> tuple[tuple[Ring, ...], tuple[Ring, ...]]:
    """GeoJSON -> (полигоны, линии). Дыры полигонов не рисуем — берём внешний контур."""
    kind, coords = (geo or {}).get("type"), (geo or {}).get("coordinates")
    if kind == "Polygon":
        return (_ring(coords[0]),), ()
    if kind == "MultiPolygon":
        return tuple(_ring(poly[0]) for poly in coords), ()
    if kind == "LineString":
        return (), (_ring(coords),)
    if kind == "MultiLineString":
        return (), tuple(_ring(line) for line in coords)
    return (), ()  # Point: только флажок


def parse_nominatim(data: list[dict]) -> list[Place]:
    places = []
    for item in data:
        try:
            south, north, west, east = (float(v) for v in item["boundingbox"])
            areas, lines = _geometry(item.get("geojson"))
            address = item.get("display_name", "")
            places.append(Place(
                name=item.get("name") or address.split(",")[0],
                address=address,
                lat=float(item["lat"]), lon=float(item["lon"]),
                bbox=(south, west, north, east), areas=areas, lines=lines,
            ))
        except (KeyError, TypeError, ValueError, IndexError):
            continue  # неполная запись — пропускаем, остальные показываем
    return places


def merge_duplicates(places: list[Place]) -> list[Place]:
    """Склеивает записи одного объекта (улица в OSM — несколько кусков) в одну."""
    merged: dict[tuple, Place] = {}
    for pl in places:
        # Куски одной улицы различаются лишь числовыми частями адреса (индекс, номер квартала)
        key = (pl.name, tuple(part for part in pl.address.split(", ") if not part.isdigit()))
        first = merged.get(key)
        if first is None:
            merged[key] = pl
            continue
        s1, w1, n1, e1 = first.bbox
        s2, w2, n2, e2 = pl.bbox
        merged[key] = replace(first, bbox=(min(s1, s2), min(w1, w2), max(n1, n2), max(e1, e2)),
                              areas=first.areas + pl.areas, lines=first.lines + pl.lines)
    return list(merged.values())


def sort_by_distance(places: list[Place], center: tuple[float, float]) -> list[Place]:
    """Ближние к center — первыми (Nominatim ранжирует по известности, а на месте важнее близость)."""
    return sorted(places, key=lambda pl: inverse(*center, pl.lat, pl.lon)[0])
