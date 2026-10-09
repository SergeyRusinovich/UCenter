"""Модель спутников по NMEA GSV/GSA (чистая логика, без Qt)."""
import time
from dataclasses import dataclass

SYSTEM_NAMES = {
    "G": "GPS (G)",
    "S": "SBAS (S)",
    "E": "Galileo (E)",
    "B": "BeiDou (B)",
    "R": "GLONASS (R)",
    "Q": "QZSS (Q)",
}
TALKER_TO_SYS = {"GP": "G", "GL": "R", "GA": "E", "GB": "B", "BD": "B", "GQ": "Q", "GN": "G"}
GSA_SYSID_TO_SYS = {1: "G", 2: "R", 3: "E", 4: "B", 5: "Q"}
SAT_TTL = 5.0  # секунд до удаления спутника, от которого нет данных
SIGNAL_WINDOW = 1.5  # сигналы одной эпохи приходят почти одновременно


def classify(sys_char: str, svid: int) -> str:
    """SBAS приходит под talker GP с номерами 33-64 и 120-158."""
    if sys_char == "G" and (33 <= svid <= 64 or 120 <= svid <= 158):
        return "S"
    return sys_char


@dataclass
class Satellite:
    sys: str
    svid: int
    elv: int | None
    az: int | None
    cno: int | None
    seen: float  # monotonic-время последнего обновления

    @property
    def key(self) -> tuple[str, int]:
        return (self.sys, self.svid)

    @property
    def label(self) -> str:
        return f"{self.sys}{self.svid}"


class SatelliteTracker:
    """Собирает спутники из GSV и признак участия в решении из GSA."""

    def __init__(self):
        self._sats: dict[tuple[str, int], Satellite] = {}
        # последний C/N0 по каждому сигналу (частоте) спутника: {signalID: (cno, время)}
        self._signals: dict[tuple[str, int], dict[str, tuple[int | None, float]]] = {}
        self._used: dict[tuple[str, int], float] = {}

    def handle(self, msg, now: float | None = None):
        ident = getattr(msg, "identity", "")
        now = time.monotonic() if now is None else now
        try:
            if ident.endswith("GSV"):
                self._on_gsv(msg, now)
            elif ident.endswith("GSA"):
                self._on_gsa(msg, now)
        except (ValueError, TypeError):
            pass  # неполное сообщение

    def snapshot(self, now: float | None = None):
        """Удаляет устаревшие записи; возвращает (отсортированные спутники, ключи 'в решении')."""
        now = time.monotonic() if now is None else now
        self._sats = {k: s for k, s in self._sats.items() if now - s.seen < SAT_TTL}
        self._signals = {k: v for k, v in self._signals.items() if k in self._sats}
        self._used = {k: t for k, t in self._used.items() if now - t < SAT_TTL}
        sats = sorted(self._sats.values(), key=lambda s: (s.sys, s.svid))
        return sats, frozenset(self._used)

    def _on_gsv(self, msg, now):
        """До 4 спутников в сообщении, по одному сообщению на сигнал."""
        base = TALKER_TO_SYS.get(getattr(msg, "talker", "GN"), "G")
        signal = str(getattr(msg, "signalID", "") or "0")  # NMEA < 4.10 — один сигнал
        for i in range(1, 5):
            svid = getattr(msg, f"svid_{i:02d}", "")
            if svid == "":
                continue

            def num(name):
                v = getattr(msg, f"{name}_{i:02d}", "")
                return None if v == "" else int(v)

            sys_char, svid = classify(base, int(svid)), int(svid)
            signals = self._signals.setdefault((sys_char, svid), {})
            signals[signal] = (num("cno"), now)
            # Лучший C/N0 среди сигналов текущей эпохи: падение уровня и потеря
            # слежения (пустой C/N0) не маскируются старыми значениями
            cno = max((c for c, t in signals.values() if c and now - t < SIGNAL_WINDOW),
                      default=None)
            sat = Satellite(sys_char, svid, num("elv"), num("az"), cno, now)
            self._sats[sat.key] = sat

    def _on_gsa(self, msg, now):
        try:
            base = GSA_SYSID_TO_SYS.get(int(msg.systemId))
        except (AttributeError, ValueError, TypeError):
            base = TALKER_TO_SYS.get(getattr(msg, "talker", "GN"), "G")
        if base is None:
            return
        for i in range(1, 13):
            svid = getattr(msg, f"svid_{i:02d}", "")
            if svid != "":
                self._used[(classify(base, int(svid)), int(svid))] = now


def count_by_system(sats, used) -> dict[str, tuple[int, int]]:
    """{система: (в решении, видно)} для каждой системы из SYSTEM_NAMES."""
    counts = {ch: [0, 0] for ch in SYSTEM_NAMES}
    for s in sats:
        c = counts.setdefault(s.sys, [0, 0])
        c[0] += s.key in used
        c[1] += 1
    return {ch: (u, v) for ch, (u, v) in counts.items()}
