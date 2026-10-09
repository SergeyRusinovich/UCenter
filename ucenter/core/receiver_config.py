"""Сборка UBX-CFG-VALSET для u-blox M9 (Configuration Database)."""
from pyubx2 import UBXMessage, SET_LAYER_RAM, SET_LAYER_BBR, TXN_NONE, POLL_LAYER_RAM

# (подпись, период измерений в мс)
RATE_OPTIONS = (("1 Hz", 1000), ("5 Hz", 200), ("10 Hz", 100))
RATE_KEY = "CFG_RATE_MEAS"

# (подпись, ключ конфигурации)
CONSTELLATION_KEYS = (
    ("GPS", "CFG_SIGNAL_GPS_ENA"),
    ("GLONASS", "CFG_SIGNAL_GLO_ENA"),
    ("Galileo", "CFG_SIGNAL_GAL_ENA"),
    ("BeiDou", "CFG_SIGNAL_BDS_ENA"),
)
NAV_PVT_USB_KEY = "CFG_MSGOUT_UBX_NAV_PVT_USB"
# Ключи, текущие значения которых читаются из приёмника для вкладки конфигурации
POLL_KEYS = [RATE_KEY] + [key for _, key in CONSTELLATION_KEYS]

CFG_CLASS, VALSET_ID = 0x06, 0x8A


def build_valset(cfg_data: list[tuple[str, int]]) -> bytes:
    """Сериализует VALSET в слои RAM и BBR."""
    msg = UBXMessage.config_set(SET_LAYER_RAM | SET_LAYER_BBR, TXN_NONE, cfg_data)
    return msg.serialize()


def build_valget(keys: list[str]) -> bytes:
    """Запрос CFG-VALGET текущих значений из слоя RAM."""
    return UBXMessage.config_poll(POLL_LAYER_RAM, 0, keys).serialize()


def valset_ack(msg) -> bool | None:
    """True/False для ACK-ACK/ACK-NAK на CFG-VALSET, None — для прочих сообщений."""
    ident = getattr(msg, "identity", "")
    if ident not in ("ACK-ACK", "ACK-NAK"):
        return None
    if getattr(msg, "clsID", None) != CFG_CLASS or getattr(msg, "msgID", None) != VALSET_ID:
        return None
    return ident == "ACK-ACK"


def valget_values(msg) -> dict[str, int]:
    """Значения опрошенных ключей из ответа CFG-VALGET."""
    if getattr(msg, "identity", "") != "CFG-VALGET":
        return {}
    return {k: int(getattr(msg, k)) for k in POLL_KEYS if hasattr(msg, k)}
