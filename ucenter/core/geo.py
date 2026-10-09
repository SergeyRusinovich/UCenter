"""Проекция Web Mercator для тайловой карты и геодезия WGS84 (чистая математика)."""
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


# ---------- Геодезия на эллипсоиде WGS84 ----------
WGS84_A = 6378137.0
WGS84_F = 1 / 298.257223563
EARTH_RADIUS = 6371008.8  # средний радиус Земли, м (для сферического запасного расчёта)


def inverse(lat1: float, lon1: float, lat2: float, lon2: float
            ) -> tuple[float, float | None, float | None]:
    """Обратная геодезическая задача (формулы Винсенти, WGS84).

    Возвращает (расстояние в метрах, азимут 1->2, азимут 2->1). Азимуты — градусы
    от истинного севера по часовой стрелке [0, 360); для совпадающих точек — None.
    """
    a, f = WGS84_A, WGS84_F
    b = (1 - f) * a
    L = math.radians(lon2 - lon1)
    u1 = math.atan((1 - f) * math.tan(math.radians(lat1)))
    u2 = math.atan((1 - f) * math.tan(math.radians(lat2)))
    sin_u1, cos_u1 = math.sin(u1), math.cos(u1)
    sin_u2, cos_u2 = math.sin(u2), math.cos(u2)

    lam = L
    for _ in range(200):
        sin_lam, cos_lam = math.sin(lam), math.cos(lam)
        sin_sigma = math.hypot(cos_u2 * sin_lam, cos_u1 * sin_u2 - sin_u1 * cos_u2 * cos_lam)
        if sin_sigma == 0:
            return 0.0, None, None
        cos_sigma = sin_u1 * sin_u2 + cos_u1 * cos_u2 * cos_lam
        sigma = math.atan2(sin_sigma, cos_sigma)
        sin_alpha = cos_u1 * cos_u2 * sin_lam / sin_sigma
        cos2_alpha = 1 - sin_alpha ** 2
        cos_2sm = cos_sigma - 2 * sin_u1 * sin_u2 / cos2_alpha if cos2_alpha else 0.0
        c = f / 16 * cos2_alpha * (4 + f * (4 - 3 * cos2_alpha))
        lam_prev = lam
        lam = L + (1 - c) * f * sin_alpha * (
            sigma + c * sin_sigma * (cos_2sm + c * cos_sigma * (-1 + 2 * cos_2sm ** 2)))
        if abs(lam - lam_prev) < 1e-12:
            break
    else:
        return _inverse_sphere(lat1, lon1, lat2, lon2)  # почти антиподы: Винсенти не сходится

    u_sq = cos2_alpha * (a * a - b * b) / (b * b)
    big_a = 1 + u_sq / 16384 * (4096 + u_sq * (-768 + u_sq * (320 - 175 * u_sq)))
    big_b = u_sq / 1024 * (256 + u_sq * (-128 + u_sq * (74 - 47 * u_sq)))
    d_sigma = big_b * sin_sigma * (cos_2sm + big_b / 4 * (
        cos_sigma * (-1 + 2 * cos_2sm ** 2)
        - big_b / 6 * cos_2sm * (-3 + 4 * sin_sigma ** 2) * (-3 + 4 * cos_2sm ** 2)))
    dist = b * big_a * (sigma - d_sigma)
    fwd = math.atan2(cos_u2 * sin_lam, cos_u1 * sin_u2 - sin_u1 * cos_u2 * cos_lam)
    at_2 = math.atan2(cos_u1 * sin_lam, -sin_u1 * cos_u2 + cos_u1 * sin_u2 * cos_lam)
    return dist, math.degrees(fwd) % 360, (math.degrees(at_2) + 180) % 360


def _bearing_sphere(lat1, lon1, lat2, lon2) -> float:
    p1, p2, dl = math.radians(lat1), math.radians(lat2), math.radians(lon2 - lon1)
    y = math.sin(dl) * math.cos(p2)
    x = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return math.degrees(math.atan2(y, x)) % 360


def _inverse_sphere(lat1, lon1, lat2, lon2) -> tuple[float, float, float]:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    h = (math.sin((p2 - p1) / 2) ** 2
         + math.cos(p1) * math.cos(p2) * math.sin(math.radians(lon2 - lon1) / 2) ** 2)
    dist = 2 * EARTH_RADIUS * math.asin(min(1.0, math.sqrt(h)))
    return dist, _bearing_sphere(lat1, lon1, lat2, lon2), _bearing_sphere(lat2, lon2, lat1, lon1)
