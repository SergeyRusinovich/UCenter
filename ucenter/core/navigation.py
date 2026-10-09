"""Навигационное решение из NAV-PVT и NMEA GGA/GSA/RMC (чистая логика, без Qt)."""
import time
from dataclasses import dataclass, replace

FIX_NAMES = {0: "No Fix", 1: "Dead Reckoning", 2: "2D", 3: "3D",
             4: "GNSS+DR", 5: "Time only"}
KNOTS_TO_KMH = 1.852
MMS_TO_KMH = 0.0036
PVT_TIMEOUT = 2.0  # секунд: пока NAV-PVT приходит, NMEA не перезаписывает его данные


@dataclass(frozen=True)
class NavData:
    """Снимок навигационного решения; None — значение ещё не получено."""
    lat: float | None = None
    lon: float | None = None
    alt_m: float | None = None
    utc_seconds: int | None = None   # секунд от полуночи UTC
    fix: str | None = None
    pdop: float | None = None
    hdop: float | None = None
    num_sv: int | None = None
    speed_kmh: float | None = None
    cog: float | None = None          # курс движения (по земле), град.
    heading: float | None = None      # курс корпуса из датчика приёмника, град.
    heading_supported: bool = False   # приёмник хоть раз отдал валидный headVeh


def _num(value, kind=float):
    return None if value in ("", None) else kind(value)


def _seconds(t) -> int | None:
    """datetime.time из NMEA -> секунды от полуночи (None, если времени нет)."""
    return t.hour * 3600 + t.minute * 60 + t.second if hasattr(t, "hour") else None


class NavigationState:
    """Накапливает решение из сообщений приёмника.

    NAV-PVT — основной источник (точнее и с флагами валидности); NMEA используется,
    когда PVT не включён. HDOP есть только в GGA.
    """

    def __init__(self):
        self._data = NavData()
        self._quality = 0   # GGA quality
        self._nav_mode = 1  # GSA navMode
        self._pvt_seen: float | None = None  # monotonic-время последнего NAV-PVT

    @property
    def data(self) -> NavData:
        return self._data

    def apply(self, msg, now: float | None = None) -> bool:
        """Применяет сообщение; True, если решение могло измениться."""
        ident = getattr(msg, "identity", "")
        now = time.monotonic() if now is None else now
        pvt = self._pvt_seen is not None and now - self._pvt_seen < PVT_TIMEOUT
        try:
            if ident == "NAV-PVT":
                self._pvt_seen = now
                upd = self._from_pvt(msg)
            elif ident.endswith("GGA"):
                upd = self._from_gga(msg, pvt)
            elif ident.endswith("GSA"):
                self._nav_mode = int(msg.navMode or 1)
                upd = {} if pvt else {"pdop": _num(msg.PDOP), "fix": self._nmea_fix()}
            elif ident.endswith("RMC"):
                upd = {} if pvt else self._from_rmc(msg)
            else:
                return False
        except (AttributeError, ValueError, TypeError):
            return False  # неполное сообщение (например, пустые поля без фикса)
        if not upd:
            return False
        self._data = replace(self._data, **upd)
        return True

    def _from_pvt(self, msg) -> dict:
        upd = {}
        if msg.fixType >= 2 and msg.gnssFixOk:
            upd.update(lat=msg.lat, lon=msg.lon, alt_m=msg.hMSL / 1000,
                       speed_kmh=msg.gSpeed * MMS_TO_KMH, cog=float(msg.headMot))
        if msg.validTime:
            upd["utc_seconds"] = msg.hour * 3600 + msg.min * 60 + msg.second
        fix = FIX_NAMES.get(msg.fixType, str(msg.fixType))
        if msg.fixType in (3, 4) and msg.diffSoln:
            fix = "GNSS+DGNSS"
        upd.update(fix=fix, pdop=float(msg.pDOP), num_sv=int(msg.numSV))
        if msg.headVehValid:
            upd.update(heading=float(msg.headVeh), heading_supported=True)
        return upd

    def _from_gga(self, msg, pvt: bool) -> dict:
        self._quality = int(msg.quality or 0)
        upd = {"hdop": _num(msg.HDOP)}
        if pvt:
            return upd
        if msg.quality and msg.lat != "" and msg.lon != "":
            upd.update(lat=float(msg.lat), lon=float(msg.lon))
        if msg.alt != "":
            upd["alt_m"] = float(msg.alt)
        if (t := _seconds(msg.time)) is not None:
            upd["utc_seconds"] = t
        upd.update(num_sv=_num(msg.numSV, int), fix=self._nmea_fix())
        return upd

    def _from_rmc(self, msg) -> dict:
        upd = {}
        if (t := _seconds(msg.time)) is not None:
            upd["utc_seconds"] = t
        if msg.status == "A":
            if msg.spd != "":
                upd["speed_kmh"] = float(msg.spd) * KNOTS_TO_KMH
            if msg.cog != "":
                upd["cog"] = float(msg.cog)
        return upd

    def _nmea_fix(self) -> str:
        if self._quality == 0 or self._nav_mode == 1:
            return "No Fix"
        if self._quality == 2:
            return "GNSS+DGNSS"
        return {2: "2D", 3: "3D"}.get(self._nav_mode, "3D")
