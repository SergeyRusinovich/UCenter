"""Вкладка спутников: небесная карта и диаграмма сигналов, у каждой свои фильтры."""
from PyQt6.QtCore import Qt
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QGroupBox, QSlider,
)

from ucenter.core.satellites import count_by_system
from ucenter.ui.filters import ConstellationFilters
from ucenter.ui.satellite_views import SkyView, SignalView
from ucenter.ui.style import role_label
from ucenter.ui.theme import NOT_USED_COLOR


class SatellitesTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.sats, self.used = [], frozenset()
        lay = QVBoxLayout(self)

        # --- Sky view + боковая панель фильтров ---
        sky_box = QGroupBox("Satellite Position View")
        sky_lay = QHBoxLayout(sky_box)
        self.sky = SkyView()
        self.sky_filters = ConstellationFilters(vertical=True, show_counts=True)
        self.sky_filters.changed.connect(self._redraw)
        side = QVBoxLayout()
        side.addWidget(self.sky_filters)
        side.addLayout(self._build_zoom())
        side.addStretch()
        side_w = QWidget()
        side_w.setLayout(side)
        side_w.setFixedWidth(220)
        sky_lay.addWidget(self.sky, 1)
        sky_lay.addWidget(side_w)

        # --- Signal view + строка фильтров под диаграммой ---
        sig_box = QGroupBox("Satellite Signal View")
        sig_lay = QVBoxLayout(sig_box)
        self.signal = SignalView()
        self.signal_filters = ConstellationFilters(vertical=False, show_counts=False)
        self.signal_filters.changed.connect(self._redraw)
        legend = role_label(f'<span style="color:{NOT_USED_COLOR.name()}">&#9633;</span> '
                            "Not used in navigation", "caption")
        sig_lay.addWidget(self.signal, 1)
        sig_lay.addWidget(self.signal_filters)
        sig_lay.addWidget(legend)

        lay.addWidget(sky_box, 3)
        lay.addWidget(sig_box, 2)

    def _build_zoom(self) -> QVBoxLayout:
        lay = QVBoxLayout()
        self.zoom_label = QLabel()
        lay.addWidget(role_label("Zoom in/out", "section"))
        self.zoom = QSlider(Qt.Orientation.Horizontal)  # размер маркеров на карте
        self.zoom.setRange(1, 14)
        self.zoom.setValue(8)
        self.zoom.valueChanged.connect(self._redraw)
        lay.addWidget(self.zoom)
        lay.addWidget(self.zoom_label)
        return lay

    def update_data(self, sats, used):
        self.sats, self.used = sats, used
        self.sky_filters.set_counts(count_by_system(sats, used))  # счётчики до фильтрации
        self._redraw()

    def _redraw(self):
        z = self.zoom.value()
        self.zoom_label.setText(f"{z}")
        self.sky.set_data([s for s in self.sats if self.sky_filters.accepts(s)],
                          self.used, 6 + z * 0.8)
        self.signal.set_data([s for s in self.sats if self.signal_filters.accepts(s)],
                             self.used)
