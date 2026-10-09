"""Точки A и B измерения и правила их установки (чистая логика, без Qt).

A по умолчанию следует за геопозицией устройства («живая» A). Любая ручная установка A
отвязывает её; attach_a() привязывает снова. Пока позиции устройства нет, A не ждёт её:
щелчок по карте ставит A вручную.
"""
from ucenter.core.baseline import Baseline, measure

LatLon = tuple[float, float]
KEYS = ("A", "B")


class MeasurementPoints:
    def __init__(self):
        self._points: dict[str, LatLon | None] = dict.fromkeys(KEYS)
        self.device: LatLon | None = None
        self.a_live = True

    def __getitem__(self, key: str) -> LatLon | None:
        return self._points[key]

    @property
    def result(self) -> Baseline | None:
        a, b = self._points["A"], self._points["B"]
        return measure(a, b) if a and b else None

    def set(self, key: str, ll: LatLon | None):
        """Ручная установка точки (щелчок, перетаскивание, ввод, найденное место)."""
        if key == "A":
            self.a_live = False
        self._points[key] = ll

    def click(self, ll: LatLon) -> str | None:
        """Щелчок по карте ставит первую свободную точку; возвращает её ключ (None — обе стоят)."""
        free = next((k for k in KEYS if self._points[k] is None), None)
        if free is not None:
            self.set(free, ll)
        return free

    def device_moved(self, ll: LatLon) -> bool:
        """Новая позиция устройства; True, если A сдвинулась вместе с ней."""
        self.device = ll
        if self.a_live:
            self._points["A"] = ll
        return self.a_live

    def attach_a(self) -> bool:
        """A снова следует за устройством; True, если позиция устройства известна."""
        self.a_live = True
        if self.device is not None:
            self._points["A"] = self.device
        return self.device is not None

    def new_measurement(self):
        """После сохранения или очистки: B снимается, «живая» A возвращается к устройству."""
        self._points["A"] = self.device if self.a_live else None
        self._points["B"] = None

    def load(self, a: LatLon, b: LatLon):
        """Показать сохранённое измерение (A отвязывается от устройства)."""
        self.set("A", a)
        self.set("B", b)
