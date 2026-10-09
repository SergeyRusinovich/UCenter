"""Измерение базовой линии A->B: азимут от истинного севера, расстояние, смещения."""
import math
from dataclasses import dataclass

from ucenter.core.geo import inverse


@dataclass(frozen=True)
class Baseline:
    a_lat: float
    a_lon: float
    b_lat: float
    b_lon: float
    distance_m: float
    azimuth: float | None       # A->B, град. от истинного севера по часовой [0, 360)
    back_azimuth: float | None  # B->A
    saved_at: str = ""          # локальное время сохранения

    @property
    def deviation(self) -> float | None:
        """Отклонение A->B от истинного севера, град. (-180, 180]: «+» к востоку, «-» к западу."""
        if self.azimuth is None:
            return None
        return self.azimuth - 360 if self.azimuth > 180 else self.azimuth

    @property
    def d_north(self) -> float:
        """Смещение B относительно A на север, м (в локальной плоскости)."""
        return 0.0 if self.azimuth is None else self.distance_m * math.cos(math.radians(self.azimuth))

    @property
    def d_east(self) -> float:
        """Смещение B относительно A на восток, м."""
        return 0.0 if self.azimuth is None else self.distance_m * math.sin(math.radians(self.azimuth))


def measure(a: tuple[float, float], b: tuple[float, float]) -> Baseline:
    dist, fwd, back = inverse(*a, *b)
    return Baseline(*a, *b, distance_m=dist, azimuth=fwd, back_azimuth=back)


def format_azimuth(az: float | None) -> str:
    return "—" if az is None else f"{az:.2f}°"


def format_deviation(dev: float | None) -> str:
    if dev is None:
        return "—"
    side = "E" if dev > 0 else "W" if dev < 0 else "N"
    return f"{dev:+.2f}° ({side})"
