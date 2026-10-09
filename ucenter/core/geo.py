"""Проекция Web Mercator для тайловой карты (чистая математика)."""
import math

TILE_SIZE = 256
MAX_LAT = 85.0511


def ll_to_px(lat: float, lon: float, zoom: int) -> tuple[float, float]:
    """Географические координаты -> пиксели мира Web Mercator."""
    lat = max(-MAX_LAT, min(MAX_LAT, lat))
    scale = TILE_SIZE * (2 ** zoom)
    x = (lon + 180.0) / 360.0 * scale
    s = math.sin(math.radians(lat))
    y = (0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)) * scale
    return x, y


def px_to_ll(x: float, y: float, zoom: int) -> tuple[float, float]:
    scale = TILE_SIZE * (2 ** zoom)
    lon = x / scale * 360.0 - 180.0
    n = math.pi - 2 * math.pi * y / scale
    lat = math.degrees(math.atan(math.sinh(n)))
    return lat, lon
