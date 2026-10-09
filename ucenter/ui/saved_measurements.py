"""Таблица сохранённых измерений: хранение между запусками, удаление, экспорт в CSV."""
import os

from PyQt6.QtCore import QStandardPaths, pyqtSignal
from PyQt6.QtWidgets import (
    QAbstractItemView, QFileDialog, QGroupBox, QHBoxLayout, QHeaderView, QMessageBox,
    QPushButton, QTableWidget, QTableWidgetItem, QVBoxLayout,
)

from ucenter.core.baseline import Baseline, format_azimuth, format_deviation
from ucenter.core.baseline_store import load_json, save_json, write_csv

STORE_FILE = "measurements.json"

# (заголовок столбца, текст ячейки)
TABLE_COLUMNS = (
    ("Saved", lambda m: m.saved_at),
    ("A (lat, lon)", lambda m: f"{m.a_lat:.7f}, {m.a_lon:.7f}"),
    ("B (lat, lon)", lambda m: f"{m.b_lat:.7f}, {m.b_lon:.7f}"),
    ("Azimuth", lambda m: format_azimuth(m.azimuth)),
    ("Deviation", lambda m: format_deviation(m.deviation)),
    ("Distance (m)", lambda m: f"{m.distance_m:.3f}"),
)


def default_store_path() -> str:
    base = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.AppDataLocation)
    return os.path.join(base, STORE_FILE)


class SavedMeasurementsPanel(QGroupBox):
    show_requested = pyqtSignal(object)  # Baseline: показать на карте

    def __init__(self, store_path: str | None = None, parent=None):
        super().__init__("Saved measurements", parent)
        self._store_path = store_path or default_store_path()
        self._items: list[Baseline] = []

        lay = QVBoxLayout(self)
        self.table = QTableWidget(0, len(TABLE_COLUMNS))
        self.table.setHorizontalHeaderLabels([name for name, _ in TABLE_COLUMNS])
        self.table.setEditTriggers(QAbstractItemView.EditTrigger.NoEditTriggers)
        self.table.setSelectionBehavior(QAbstractItemView.SelectionBehavior.SelectRows)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.ResizeToContents)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.table.setToolTip("Double-click a row to show it on the map")
        self.table.cellDoubleClicked.connect(lambda row, _: self._request_show(row))
        self.table.itemSelectionChanged.connect(self._update_buttons)
        lay.addWidget(self.table)

        buttons = QHBoxLayout()
        self._show = QPushButton("Show on map")
        self._show.clicked.connect(lambda: self._request_show(self.table.currentRow()))
        self._delete = QPushButton("Delete")
        self._delete.setProperty("variant", "danger")
        self._delete.clicked.connect(self._delete_selected)
        self._export = QPushButton("Export CSV…")
        self._export.clicked.connect(self._export_csv)
        buttons.addWidget(self._show)
        buttons.addWidget(self._delete)
        buttons.addStretch()
        buttons.addWidget(self._export)
        lay.addLayout(buttons)

        self._load()

    def add(self, item: Baseline):
        self._items.append(item)
        self._store()  # при ошибке запись остаётся в таблице до закрытия программы
        self._fill()
        self.table.scrollToBottom()

    def _request_show(self, row: int):
        if 0 <= row < len(self._items):
            self.show_requested.emit(self._items[row])

    def _delete_selected(self):
        for row in sorted({i.row() for i in self.table.selectedItems()}, reverse=True):
            del self._items[row]
        self._store()
        self._fill()

    def _fill(self):
        self.table.setRowCount(len(self._items))
        for row, m in enumerate(self._items):
            for col, (_, fmt) in enumerate(TABLE_COLUMNS):
                self.table.setItem(row, col, QTableWidgetItem(fmt(m)))
        self._update_buttons()

    def _update_buttons(self):
        selected = self.table.currentRow() >= 0 and bool(self.table.selectedItems())
        self._show.setEnabled(selected)
        self._delete.setEnabled(selected)
        self._export.setEnabled(bool(self._items))

    def _load(self):
        try:
            self._items = load_json(self._store_path)
        except (OSError, ValueError, TypeError) as exc:
            QMessageBox.warning(self, "Saved measurements", f"Cannot read {self._store_path}:\n{exc}")
        self._fill()

    def _store(self):
        try:
            save_json(self._store_path, self._items)
        except OSError as exc:
            QMessageBox.warning(self, "Saved measurements", f"Cannot write {self._store_path}:\n{exc}")

    def _export_csv(self):
        path, _ = QFileDialog.getSaveFileName(
            self, "Export measurements", "measurements.csv", "CSV files (*.csv)")
        if not path:
            return
        try:
            write_csv(path, self._items)
        except OSError as exc:
            QMessageBox.warning(self, "Export CSV", f"Cannot write {path}:\n{exc}")
