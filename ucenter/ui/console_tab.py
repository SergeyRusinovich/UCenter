"""Консоль данных NMEA/UBX с Freeze и фильтрами."""
from collections import deque

from PyQt6.QtGui import QFontDatabase
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QCheckBox, QPushButton, QPlainTextEdit,
)

from ucenter.core.messages import is_ubx, message_text

MAX_LINES = 5000  # ограничиваем память


class ConsoleTab(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        lay = QVBoxLayout(self)
        row = QHBoxLayout()
        self.freeze_cb = QCheckBox("Freeze")
        self.nmea_only_cb = QCheckBox("Только NMEA")
        self.ubx_only_cb = QCheckBox("Только UBX")
        clear_btn = QPushButton("Очистить")
        # Фильтры взаимоисключающие
        self.nmea_only_cb.toggled.connect(
            lambda on: on and self.ubx_only_cb.setChecked(False))
        self.ubx_only_cb.toggled.connect(
            lambda on: on and self.nmea_only_cb.setChecked(False))
        for c in (self.freeze_cb, self.nmea_only_cb, self.ubx_only_cb):
            row.addWidget(c)
        row.addStretch()
        row.addWidget(clear_btn)
        self.console = QPlainTextEdit()
        self.console.setReadOnly(True)
        self.console.setMaximumBlockCount(MAX_LINES)
        # Строки копятся здесь и выводятся пачкой в flush(): одна вставка вместо сотен в секунду
        self._pending: deque[str] = deque(maxlen=MAX_LINES)
        self.console.setObjectName("console")
        # Системный моноширинный шрифт: Menlo/SF Mono на macOS, Consolas на Windows
        self.console.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        clear_btn.clicked.connect(self._clear)
        lay.addLayout(row)
        lay.addWidget(self.console)

    def append_message(self, parsed, raw: bytes):
        if self.freeze_cb.isChecked():
            return
        ubx = is_ubx(raw)
        if (self.nmea_only_cb.isChecked() and ubx) or (self.ubx_only_cb.isChecked() and not ubx):
            return
        self._pending.append(message_text(parsed, raw))

    def flush(self):
        """Выводит накопленные строки; пока вкладка скрыта, они только копятся."""
        if not self._pending or not self.isVisible():
            return
        self.console.appendPlainText("\n".join(self._pending))  # автоскролл вниз при добавлении
        self._pending.clear()

    def _clear(self):
        self._pending.clear()
        self.console.clear()
