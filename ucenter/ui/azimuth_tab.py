"""Вкладка Azimuth: собирает панели измерения A->B, карту, поиск и таблицу и связывает их.

Правила установки точек — в MeasurementPoints (core), каждая панель отвечает за своё;
вкладка только передаёт события между ними. Точка A по умолчанию — геопозиция
компьютера (службы геолокации ОС), а не GNSS-приёмника.
"""
from dataclasses import replace
from datetime import datetime

from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtGui import QKeySequence, QShortcut
from PyQt6.QtWidgets import QGroupBox, QHBoxLayout, QSplitter, QVBoxLayout, QWidget

from ucenter.core.baseline import Baseline
from ucenter.core.host_location import OK, HostLocation
from ucenter.core.places import Place
from ucenter.core.points import KEYS, LatLon, MeasurementPoints
from ucenter.ui.baseline_map_view import BaselineMapView
from ucenter.ui.imagery_picker import ImageryPicker
from ucenter.ui.measurement_panels import PointsEditor, ResultOverlay, ResultPanel
from ucenter.ui.place_search_bar import PlaceSearchBar
from ucenter.ui.saved_measurements import SavedMeasurementsPanel
from ucenter.ui.style import role_label

LEFT_WIDTH = 340


class AzimuthTab(QWidget):
    expand_changed = pyqtSignal(bool)  # карта во всё окно: владелец прячет свои панели

    def __init__(self, parent=None):
        super().__init__(parent)
        self.points = MeasurementPoints()
        self.location = HostLocation(self)
        self._loc_status = (OK, "")  # уровень и текст последнего статуса геопозиции

        self.points_editor = PointsEditor()
        self.result_panel = ResultPanel()
        self.map = BaselineMapView()
        self.overlay = ResultOverlay(self.map)
        self.search_bar = PlaceSearchBar(self.map.visible_bounds)
        self.imagery = ImageryPicker()
        self.saved = SavedMeasurementsPanel()
        self.map.set_source(self.imagery.source())
        self._build_layout()
        self._wire()
        self._refresh()

    def _build_layout(self):
        self._left = QWidget()
        left = QVBoxLayout(self._left)
        left.setContentsMargins(0, 0, 0, 0)
        left.addWidget(self.points_editor)
        left.addWidget(self.result_panel, 1)  # растягивается до таблицы сохранённых
        self._left.setFixedWidth(LEFT_WIDTH)

        map_box = QGroupBox("Map — click to place points")
        ml = QVBoxLayout(map_box)
        imagery = QWidget()  # подпись и список — по одной линии с полем поиска
        il = QHBoxLayout(imagery)
        il.setContentsMargins(0, 0, 0, 0)
        il.addWidget(role_label("Imagery", "caption"))
        il.addWidget(self.imagery)
        tools = QHBoxLayout()
        tools.addWidget(self.search_bar, 1)
        tools.addWidget(imagery, 0, Qt.AlignmentFlag.AlignTop)
        ml.addLayout(tools)
        ml.addWidget(self.map, 1)

        top = QWidget()
        tl = QHBoxLayout(top)
        tl.setContentsMargins(0, 0, 0, 0)
        tl.addWidget(self._left)
        tl.addWidget(map_box, 1)

        splitter = QSplitter(Qt.Orientation.Vertical)
        splitter.setChildrenCollapsible(False)
        splitter.addWidget(top)
        splitter.addWidget(self.saved)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 1)
        QVBoxLayout(self).addWidget(splitter)

        self._esc = QShortcut(QKeySequence(Qt.Key.Key_Escape), self, lambda: self.set_expanded(False))
        self._esc.setContext(Qt.ShortcutContext.WidgetWithChildrenShortcut)
        self._esc.setEnabled(False)

    def _wire(self):
        self.location.position.connect(self._on_device_position)
        self.location.status.connect(self._on_location_status)
        self.map.clicked.connect(self._on_map_click)
        self.map.point_moved.connect(self._on_marker_moved)
        self.map.place_moved.connect(self.search_bar.move_place)
        self.map.recentered.connect(self._attach_a)
        self.map.expand_toggled.connect(self._apply_expanded)
        self.map.location_retry_requested.connect(self.location.retry)
        self.points_editor.point_edited.connect(self._on_point_edited)
        self.result_panel.save_requested.connect(self._save)
        self.result_panel.clear_requested.connect(self._new_measurement)
        self.overlay.save_requested.connect(self._save)
        self.search_bar.place_changed.connect(self._show_place)
        self.search_bar.point_requested.connect(lambda key, lat, lon: self._place_point(key, (lat, lon)))
        self.imagery.source_changed.connect(self.map.set_source)
        self.saved.show_requested.connect(self._show_saved)

    # ---------- Точки ----------
    def _sync(self, keys=KEYS, from_device: bool = False):
        """Переносит точки из модели в поля ввода и на карту, пересчитывает результат."""
        for key in keys:
            self.points_editor.show_point(key, self.points[key], from_device)
            self.map.set_point(key, self.points[key])
        self._refresh()

    def _place_point(self, key: str, ll: LatLon | None):
        """Ручная установка точки (кнопки «→ A/B», ввод координат)."""
        self.points.set(key, ll)
        self.map.stop_following()  # карта не уезжает за позицией, пока ставятся точки
        self._sync((key,))

    def _on_map_click(self, lat: float, lon: float):
        if self.points.click((lat, lon)) is not None:
            self.map.stop_following()
            self._sync()

    def _on_marker_moved(self, key: str, lat: float, lon: float):
        self.points.set(key, (lat, lon))
        self._sync((key,))

    def _on_point_edited(self, key: str, ll: LatLon | None):
        self._place_point(key, ll)
        if ll is not None:
            self.map.center_on(*ll)

    def _attach_a(self):
        """«Прицел»: A снова следует за устройством; без позиции — объясняем почему."""
        if self.points.attach_a():
            self._sync(("A",), from_device=True)
        else:
            self._refresh()
            self.map.show_location_help(*self._loc_status)

    def _new_measurement(self):
        self.points.new_measurement()
        self._sync(from_device=True)

    def _refresh(self):
        result = self.points.result
        self.result_panel.set_result(result)
        self.overlay.set_result(result)
        self.points_editor.set_hint(self._hint())

    def _hint(self) -> str:
        p = self.points
        if p["A"] is None:
            hint = "Click the map to place point A (or enter coordinates)."
        elif p["B"] is None:
            hint = "Click the map to place point B."
        else:
            hint = "Drag a marker to adjust. Save to store the result and start a new measurement."
        if p["A"] is not None and not p.a_live and p.device is not None:
            hint += " ⌖ on the map returns A to this computer's location."
        return hint

    # ---------- Геопозиция компьютера ----------
    def showEvent(self, e):
        super().showEvent(e)
        self.location.start()  # запрос доступа — только когда вкладку открыли

    def _on_location_status(self, level: str, text: str):
        """Проблемы с геопозицией показываются на «прицеле» карты."""
        self._loc_status = (level, text)
        self.map.set_location_alert(None if level == OK else level, text)

    def _on_device_position(self, lat: float, lon: float, accuracy: float | None):
        acc = "" if accuracy is None else f" ±{accuracy:.0f} m"
        self.points_editor.set_device_location(f"Device location: {lat:.7f}, {lon:.7f}{acc}")
        self.map.set_accuracy(accuracy)
        self.map.set_position(lat, lon)
        if self.points.device_moved((lat, lon)):
            self._sync(("A",), from_device=True)
        else:
            self._refresh()  # подсказка зависит от того, известна ли позиция

    # ---------- Найденное место ----------
    def _show_place(self, place: Place | None):
        self.map.set_place(place)
        if place is not None:
            self.map.fit_bounds(*place.bbox)

    # ---------- Сохранённые измерения ----------
    def _save(self):
        result = self.points.result
        if result is None:
            return
        self.saved.add(replace(result, saved_at=datetime.now().strftime("%Y-%m-%d %H:%M:%S")))
        self._new_measurement()

    def _show_saved(self, m: Baseline):
        self.points.load((m.a_lat, m.a_lon), (m.b_lat, m.b_lon))
        self._sync()
        # Масштаб такой, чтобы отрезок A–B был виден целиком
        self.map.fit_bounds(min(m.a_lat, m.b_lat), min(m.a_lon, m.b_lon),
                            max(m.a_lat, m.b_lat), max(m.a_lon, m.b_lon))

    # ---------- Карта во всё окно ----------
    def set_expanded(self, on: bool):
        if self.map.is_expanded() != on:
            self.map.set_expanded(on)
            self._apply_expanded(on)

    def _apply_expanded(self, on: bool):
        self._left.setVisible(not on)
        self.overlay.setVisible(on)
        self._esc.setEnabled(on)
        self.expand_changed.emit(on)

    def hideEvent(self, e):
        super().hideEvent(e)
        if not e.spontaneous():  # ушли на другую вкладку (а не свернули окно) — обычный вид
            self.set_expanded(False)
