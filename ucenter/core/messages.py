"""Классификация и текстовое представление входящих сообщений."""

UBX_SYNC = b"\xb5\x62"


def is_ubx(raw: bytes) -> bool:
    return raw[:2] == UBX_SYNC


def message_text(parsed, raw: bytes) -> str:
    """UBX — в разобранном виде, NMEA — исходной строкой."""
    return str(parsed) if is_ubx(raw) else raw.decode("ascii", "replace").strip()
