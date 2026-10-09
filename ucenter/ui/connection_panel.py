"""Панель подключения: выбор порта/скорости, кнопка, индикатор статуса и время сеанса."""
import time
from collections.abc import Callable

from PyQt6.QtCore import pyqtSignal, QTimer
from PyQt6.QtWidgets import (
    QGroupBox, QHBoxLayout, QComboBox, QPushButton, QLabel, QMessageBox,
)

from ucenter.core.timeutil import format_duration
from ucenter.ui.style import set_style_property

PORT_POLL_MS = 2000  # как часто искать новые порты, пока нет подключения


class ConnectionPanel(QGroupBox):
    connect_requested = pyqtSignal(str, int)  # порт, скорость
    disconnect_requested = pyqtSignal()

    def __init__(self, find_ports: Callable[[], list[str]], parent=None):
        """find_ports — источник списка портов (внедряется, чтобы панель не зависела от ОС)."""
        super().__init__("Подключение", parent)
        self._find_ports = find_ports
        self._connected = False
        lay = QHBoxLayout(self)
        self.port_combo = QComboBox()
        self.port_combo.setEditable(True)
        self.port_combo.setMinimumWidth(220)
        self.baud_combo = QComboBox()
        for b in (9600, 38400, 115200):
            self.baud_combo.addItem(str(b), b)
        self.baud_combo.setCurrentText("38400")
        refresh_btn = QPushButton("Обновить")
        refresh_btn.clicked.connect(self.refresh_ports)
        self.connect_btn = QPushButton()
        self.connect_btn.clicked.connect(self._on_button)
        self.status_led = QLabel()
        self.status_led.setObjectName("statusLed")
        self.status_label = QLabel()
        self.uptime_label = QLabel()
        self._uptime_start = 0.0
        self._uptime_timer = QTimer(self)
        self._uptime_timer.timeout.connect(self._update_uptime)
        for w in (QLabel("Порт:"), self.port_combo, QLabel("Скорость:"),
                  self.baud_combo, refresh_btn, self.connect_btn,
                  self.status_led, self.status_label, self.uptime_label):
            lay.addWidget(w)
        lay.addStretch()
        self._ports: list[str] = []
        self.refresh_ports()
        self.set_connected(False)
        # Приёмник могли переткнуть в другой разъём — подхватываем новый порт сами
        self._port_timer = QTimer(self)
        self._port_timer.timeout.connect(self._poll_ports)
        self._port_timer.start(PORT_POLL_MS)

    def refresh_ports(self):
        ports = self._find_ports()
        current = self.port_combo.currentText()
        self._ports = ports
        self.port_combo.clear()
        self.port_combo.addItems(ports)
        # Оставляем выбранный порт, только если он ещё существует, иначе — первый найденный
        if current in ports:
            self.port_combo.setCurrentText(current)

    def _poll_ports(self):
        if self._connected or self.port_combo.hasFocus():
            return  # не мешаем вводу вручную и не трогаем список во время сеанса
        if self._find_ports() != self._ports:
            self.refresh_ports()

    def set_busy(self):
        """Идёт открытие порта."""
        self.connect_btn.setEnabled(False)

    def _update_uptime(self):
        """Время с момента успешного подключения."""
        self.uptime_label.setText(format_duration(time.monotonic() - self._uptime_start))

    def set_connected(self, on: bool):
        self._connected = on
        set_style_property(self.status_led, "state", "on" if on else "off")
        self.status_label.setText("Подключено" if on else "Отключено")
        self.connect_btn.setText("Отключить" if on else "Подключить")
        set_style_property(self.connect_btn, "variant", "danger" if on else "primary")
        self.connect_btn.setEnabled(True)
        self.port_combo.setEnabled(not on)
        self.baud_combo.setEnabled(not on)
        # Таймер запускается при подключении и сбрасывается при отключении
        self.uptime_label.setVisible(on)
        if on:
            self._uptime_start = time.monotonic()
            self._update_uptime()
            self._uptime_timer.start(1000)
        else:
            self._uptime_timer.stop()

    def _on_button(self):
        if self._connected:
            self.disconnect_requested.emit()
            return
        port = self.port_combo.currentText().strip()
        if not port:
            QMessageBox.warning(self, "Порт", "Выберите порт.")
            return
        self.connect_requested.emit(port, int(self.baud_combo.currentData()))
