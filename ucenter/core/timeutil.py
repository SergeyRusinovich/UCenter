"""Форматирование времени и поправка часового пояса."""
import time

DAY = 86400
# От UTC-12 до UTC+14 с шагом 30 минут
UTC_OFFSETS = tuple(half_hours / 2 for half_hours in range(-24, 29))


def format_hms(seconds: int) -> str:
    s = int(seconds) % DAY
    return f"{s // 3600:02d}:{s // 60 % 60:02d}:{s % 60:02d}"


def format_duration(seconds: float) -> str:
    """Длительность сеанса: 'чч:мм:сс', с днями — 'Nd чч:мм:сс'."""
    days, rem = divmod(int(seconds), DAY)
    return f"{days}d {format_hms(rem)}" if days else format_hms(rem)


def offset_label(hours: float) -> str:
    sign = "+" if hours >= 0 else "-"
    total = int(abs(hours) * 60)
    return f"UTC{sign}{total // 60:02d}:{total % 60:02d}"


def system_utc_offset() -> float:
    """Смещение часового пояса системы, округлённое до ближайшего из UTC_OFFSETS."""
    local = round(time.localtime().tm_gmtoff / 1800) / 2
    return max(UTC_OFFSETS[0], min(UTC_OFFSETS[-1], local))


def to_local(utc_seconds: int, offset_hours: float) -> int:
    """Секунды от полуночи UTC -> секунды от полуночи по местному времени."""
    return (utc_seconds + int(offset_hours * 3600)) % DAY
