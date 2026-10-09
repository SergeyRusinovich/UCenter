"""Строка поиска мест: запрос по Enter, выбор из результатов, «→ A» / «→ B», строка состояния.

С картой не связана напрямую: видимую область получает через переданную функцию,
а о выбранном месте и запросе поставить точку сообщает сигналами.
"""
from collections.abc import Callable
from dataclasses import replace

from PyQt6.QtCore import Qt, pyqtSignal
from PyQt6.QtWidgets import QHBoxLayout, QLineEdit, QMenu, QPushButton, QVBoxLayout, QWidget

from ucenter.core.places import Place, sort_by_distance
from ucenter.core.points import KEYS
from ucenter.ui.geocoder import ATTRIBUTION, PlaceSearch
from ucenter.ui.style import role_label

Bounds = tuple[float, float, float, float]  # юг, запад, север, восток
MENU_TEXT_W = 520


class PlaceSearchBar(QWidget):
    place_changed = pyqtSignal(object)               # выбранное место (Place) или None — убрать
    point_requested = pyqtSignal(str, float, float)  # поставить точку на место флажка

    def __init__(self, view_bounds: Callable[[], Bounds], parent=None):
        super().__init__(parent)
        self._view_bounds = view_bounds
        self._place: Place | None = None
        self._search = PlaceSearch(self)
        self._search.results.connect(self._on_results)
        self._search.failed.connect(self._set_status)

        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        row = QHBoxLayout()
        self.edit = QLineEdit()
        self.edit.setPlaceholderText("Поиск места: адрес, улица, город, объект… (Enter)")
        self.edit.setClearButtonEnabled(True)
        self.edit.setToolTip(ATTRIBUTION)
        self.edit.returnPressed.connect(self._start_search)
        self.edit.textChanged.connect(lambda text: text or self._select(None))
        row.addWidget(self.edit, 1)
        self._point_buttons = []
        for key in KEYS:
            btn = QPushButton(f"→ {key}")
            btn.setToolTip(f"Поставить точку {key} на найденное место")
            btn.clicked.connect(lambda _, k=key: self._request_point(k))
            btn.setEnabled(False)
            row.addWidget(btn)
            self._point_buttons.append(btn)
        lay.addLayout(row)
        self.status = role_label("", "caption")  # найденное место или ошибка
        lay.addWidget(self.status)

    def move_place(self, lat: float, lon: float):
        """Флажок места перетащили на карте."""
        if self._place is not None:
            self._place = replace(self._place, lat=lat, lon=lon)
            self.status.setToolTip(f"{self._place.address}\n{lat:.7f}, {lon:.7f} (флажок сдвинут)")

    def _set_status(self, text: str):
        self.status.setText(text)

    def _start_search(self):
        if self.edit.text().strip():
            self._set_status("Поиск…")
            self._search.search(self.edit.text(), self._view_bounds())

    def _on_results(self, text: str, places: list[Place]):
        if text != self.edit.text().strip():
            return  # пока ждали ответ, запрос уже изменили
        if not places:
            self._set_status(f"Ничего не найдено: «{text}»")
            return
        if len(places) == 1:
            self._select(places[0])
            return
        south, west, north, east = self._view_bounds()
        places = sort_by_distance(places, ((south + north) / 2, (west + east) / 2))
        self._set_status(f"Найдено: {len(places)} — выберите место")
        menu = QMenu(self)
        for place in places:
            action = menu.addAction(self._menu_text(place))
            action.setToolTip(place.address)
            action.triggered.connect(lambda _, pl=place: self._select(pl))
        menu.setToolTipsVisible(True)
        menu.popup(self.edit.mapToGlobal(self.edit.rect().bottomLeft()))

    def _menu_text(self, place: Place) -> str:
        rest = place.address.removeprefix(place.name).lstrip(", ")
        text = f"{place.name} — {rest}" if rest else place.name
        return self.fontMetrics().elidedText(text, Qt.TextElideMode.ElideRight, MENU_TEXT_W)

    def _select(self, place: Place | None):
        self._place = place
        for btn in self._point_buttons:
            btn.setEnabled(place is not None)
        if place is None:
            self._set_status("")
        else:
            self._set_status(self.fontMetrics().elidedText(
                place.address, Qt.TextElideMode.ElideRight, max(120, self.status.width())))
            self.status.setToolTip(f"{place.address}\n{place.lat:.7f}, {place.lon:.7f}")
        self.place_changed.emit(place)

    def _request_point(self, key: str):
        if self._place is not None:
            self.point_requested.emit(key, self._place.lat, self._place.lon)
