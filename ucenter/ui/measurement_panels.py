"""Панели измерения A->B: ввод координат точек, результат и плашка результата поверх карты."""
from PyQt6.QtCore import QLocale, pyqtSignal
from PyQt6.QtGui import QDoubleValidator
from PyQt6.QtWidgets import QFrame, QGridLayout, QGroupBox, QHBoxLayout, QLineEdit, QPushButton

from ucenter.core.baseline import Baseline, format_azimuth, format_deviation
from ucenter.core.points import KEYS, LatLon
from ucenter.ui.style import role_label

EMPTY = "—"

# (подпись, форматирование результата для показа)
RESULTS = (
    ("Azimuth A→B", lambda m: format_azimuth(m.azimuth)),
    ("Deviation from true N", lambda m: format_deviation(m.deviation)),
    ("Back azimuth B→A", lambda m: format_azimuth(m.back_azimuth)),
    ("Distance (m)", lambda m: f"{m.distance_m:.3f}"),
    ("ΔNorth (m)", lambda m: f"{m.d_north:+.3f}"),
    ("ΔEast (m)", lambda m: f"{m.d_east:+.3f}"),
)


def _coord_edit(limit: float, placeholder: str) -> QLineEdit:
    edit = QLineEdit()
    validator = QDoubleValidator(-limit, limit, 8, edit)
    validator.setNotation(QDoubleValidator.Notation.StandardNotation)
    validator.setLocale(QLocale.c())  # десятичная точка независимо от локали ОС
    edit.setValidator(validator)
    edit.setPlaceholderText(placeholder)
    return edit


def _primary_button(text: str) -> QPushButton:
    btn = QPushButton(text)
    btn.setProperty("variant", "primary")
    return btn


class PointsEditor(QGroupBox):
    """Координаты A и B, позиция устройства и подсказка, что делать дальше."""

    point_edited = pyqtSignal(str, object)  # ключ, (широта, долгота) или None — ввод пользователя

    def __init__(self, parent=None):
        super().__init__("Points", parent)
        grid = QGridLayout(self)
        grid.addWidget(role_label("Latitude", "caption"), 0, 1)
        grid.addWidget(role_label("Longitude", "caption"), 0, 2)
        self._edits: dict[str, tuple[QLineEdit, QLineEdit]] = {}
        for row, key in enumerate(KEYS, 1):
            lat, lon = _coord_edit(90, "lat"), _coord_edit(180, "lon")
            for edit in (lat, lon):
                edit.editingFinished.connect(lambda k=key: self._on_edit(k))
            grid.addWidget(role_label(key, "section"), row, 0)
            grid.addWidget(lat, row, 1)
            grid.addWidget(lon, row, 2)
            self._edits[key] = (lat, lon)
        grid.setColumnStretch(1, 1)
        grid.setColumnStretch(2, 1)
        self._device = role_label("", "caption")  # координаты устройства, когда они есть
        self._device.hide()
        grid.addWidget(self._device, len(KEYS) + 1, 0, 1, 3)
        self._hint = role_label("", "caption")
        self._hint.setWordWrap(True)
        grid.addWidget(self._hint, len(KEYS) + 2, 0, 1, 3)

    def show_point(self, key: str, ll: LatLon | None, from_device: bool = False):
        """from_device — обновление по позиции устройства: не затирает поле, в котором печатают."""
        for edit, i in zip(self._edits[key], (0, 1)):
            if not (from_device and edit.hasFocus()):
                edit.setText("" if ll is None else f"{ll[i]:.8f}")

    def set_device_location(self, text: str):
        self._device.setText(text)
        self._device.setVisible(bool(text))

    def set_hint(self, text: str):
        self._hint.setText(text)

    def _on_edit(self, key: str):
        lat_e, lon_e = self._edits[key]
        if not (lat_e.isModified() or lon_e.isModified()):
            return  # потеря фокуса без правки
        lat_t, lon_t = lat_e.text().strip(), lon_e.text().strip()
        if not lat_t and not lon_t:
            self.point_edited.emit(key, None)
        elif lat_t and lon_t:
            self.point_edited.emit(key, (float(lat_t), float(lon_t)))


class ResultPanel(QGroupBox):
    """Результат измерения; Save и Clear прижаты к низу блока."""

    save_requested = pyqtSignal()
    clear_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__("Result (WGS84, true north)", parent)
        grid = QGridLayout(self)
        self._labels = {}
        for i, (name, _) in enumerate(RESULTS):
            val = role_label(EMPTY, "value")
            grid.addWidget(role_label(name, "caption"), i, 0)
            grid.addWidget(val, i, 1)
            self._labels[name] = val
        grid.setColumnStretch(1, 1)

        buttons = QHBoxLayout()
        self._save = _primary_button("Save")
        self._save.setToolTip("Add to saved measurements and start a new one")
        self._save.clicked.connect(self.save_requested)
        clear = QPushButton("Clear")
        clear.clicked.connect(self.clear_requested)
        buttons.addWidget(self._save)
        buttons.addWidget(clear)
        grid.setRowStretch(len(RESULTS), 1)  # свободное место — между значениями и кнопками
        grid.addLayout(buttons, len(RESULTS) + 1, 0, 1, 2)

    def set_result(self, result: Baseline | None):
        for name, fmt in RESULTS:
            self._labels[name].setText(EMPTY if result is None else fmt(result))
        self._save.setEnabled(result is not None)


class ResultOverlay(QFrame):
    """Плашка поверх карты в развёрнутом виде: результат и Save вместо скрытых панелей."""

    save_requested = pyqtSignal()

    def __init__(self, parent):
        super().__init__(parent)
        self.setObjectName("mapOverlay")
        lay = QHBoxLayout(self)
        lay.setContentsMargins(10, 6, 6, 6)
        self._label = role_label("", "value")
        self._save = _primary_button("Save")
        self._save.clicked.connect(self.save_requested)
        lay.addWidget(self._label)
        lay.addWidget(self._save)
        self.move(8, 8)
        self.hide()

    def set_result(self, r: Baseline | None):
        if r is None:
            self._label.setText("Place points A and B")
        else:
            self._label.setText(f"A→B {format_azimuth(r.azimuth)}   {format_deviation(r.deviation)}"
                                f"   {r.distance_m:.1f} m")
        self._save.setEnabled(r is not None)
        self.adjustSize()
