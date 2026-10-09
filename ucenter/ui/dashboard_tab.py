"""Вкладка Dashboard: данные навигации, поправка UTC, компас и карта."""
from PyQt6.QtWidgets import (
    QWidget, QGridLayout, QHBoxLayout, QVBoxLayout, QGroupBox, QComboBox,
)

from ucenter.core.navigation import NavData
from ucenter.core.timeutil import UTC_OFFSETS, format_hms, offset_label, system_utc_offset, to_local
from ucenter.ui.compass_view import CompassView
from ucenter.ui.map_view import MapView
from ucenter.ui.style import role_label

EMPTY = "—"


def _fmt(value, spec: str) -> str:
    return EMPTY if value is None else format(value, spec)


# (подпись, форматирование поля NavData для показа)
FIELDS = (
    ("Latitude", lambda d: _fmt(d.lat, ".7f")),
    ("Longitude", lambda d: _fmt(d.lon, ".7f")),
    ("Altitude (m)", lambda d: _fmt(d.alt_m, ".1f")),
    ("UTC Time", lambda d: EMPTY if d.utc_seconds is None else format_hms(d.utc_seconds)),
    ("Fix Type", lambda d: d.fix or EMPTY),
    ("PDOP", lambda d: _fmt(d.pdop, ".2f")),
    ("HDOP", lambda d: _fmt(d.hdop, ".2f")),
    ("SVs used", lambda d: _fmt(d.num_sv, "d")),
    ("Speed (km/h)", lambda d: _fmt(d.speed_kmh, ".1f")),
    ("Course (COG)", lambda d: EMPTY if d.cog is None else f"{d.cog:.1f}°"),
)


class DashboardTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        root = QHBoxLayout(self)

        left = QVBoxLayout()
        left.addWidget(self._build_values())
        left.addWidget(self._build_time_correction())
        compass_box = QGroupBox("Compass")
        cl = QVBoxLayout(compass_box)
        self.compass = CompassView()
        cl.addWidget(self.compass)
        left.addWidget(compass_box, 1)
        left_w = QWidget()
        left_w.setLayout(left)
        left_w.setFixedWidth(320)

        map_box = QGroupBox("Map View")
        ml = QVBoxLayout(map_box)
        self.map = MapView()
        ml.addWidget(self.map)

        root.addWidget(left_w)
        root.addWidget(map_box, 1)
        self._utc_seconds: int | None = None

    def _build_values(self) -> QGroupBox:
        box = QGroupBox("Data View")
        grid = QGridLayout(box)
        self._labels = {}
        for i, (name, _) in enumerate(FIELDS):
            val = role_label(EMPTY, "value")
            grid.addWidget(role_label(name, "caption"), i, 0)
            grid.addWidget(val, i, 1)
            self._labels[name] = val
        grid.setColumnStretch(1, 1)
        return box

    def _build_time_correction(self) -> QGroupBox:
        box = QGroupBox("UTC correction")
        lay = QGridLayout(box)
        self.offset_combo = QComboBox()
        for hours in UTC_OFFSETS:
            self.offset_combo.addItem(offset_label(hours), hours)
        # По умолчанию — часовой пояс системы
        self.offset_combo.setCurrentIndex(self.offset_combo.findData(system_utc_offset()))
        self.offset_combo.currentIndexChanged.connect(self._update_local_time)
        self.local_label = role_label(EMPTY, "value")
        lay.addWidget(role_label("Offset", "caption"), 0, 0)
        lay.addWidget(self.offset_combo, 0, 1)
        lay.addWidget(role_label("Local time", "caption"), 1, 0)
        lay.addWidget(self.local_label, 1, 1)
        return box

    def _update_local_time(self):
        if self._utc_seconds is None:
            self.local_label.setText(EMPTY)
            return
        local = to_local(self._utc_seconds, self.offset_combo.currentData())
        self.local_label.setText(format_hms(local))

    def update_state(self, data: NavData):
        for name, fmt in FIELDS:
            self._labels[name].setText(fmt(data))
        self._utc_seconds = data.utc_seconds
        self._update_local_time()
        self.map.set_position(data.lat, data.lon)
        self.compass.set_state(data.heading_supported, data.heading, data.cog)
