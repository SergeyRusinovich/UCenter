"""Вкладка конфигуратора: формирует запросы CFG-VALSET, отправку делает владелец."""
from PyQt6.QtCore import pyqtSignal
from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QGroupBox, QComboBox, QPushButton,
    QCheckBox, QMessageBox,
)

from ucenter.core import receiver_config as cfg


class ConfigTab(QWidget):
    valset_requested = pyqtSignal(list)  # список (ключ, значение)
    refresh_requested = pyqtSignal()     # перечитать текущие значения из приёмника

    def __init__(self, parent=None):
        super().__init__(parent)
        lay = QVBoxLayout(self)

        rate_box = QGroupBox("Navigation Rate (CFG-RATE-MEAS)")
        rl = QHBoxLayout(rate_box)
        self.rate_combo = QComboBox()
        for label, ms in cfg.RATE_OPTIONS:
            self.rate_combo.addItem(label, ms)
        apply_rate = QPushButton("Применить")
        apply_rate.clicked.connect(self._apply_rate)
        rl.addWidget(self.rate_combo)
        rl.addWidget(apply_rate)
        rl.addStretch()
        read_btn = QPushButton("Прочитать из приёмника")
        read_btn.clicked.connect(self.refresh_requested)
        rl.addWidget(read_btn)

        gnss_box = QGroupBox("Созвездия (CFG-SIGNAL-*-ENA)")
        gl = QVBoxLayout(gnss_box)
        self.gnss_cbs = {}
        for name, key in cfg.CONSTELLATION_KEYS:
            cb = QCheckBox(name)
            cb.setChecked(True)
            gl.addWidget(cb)
            self.gnss_cbs[key] = cb
        apply_gnss = QPushButton("Применить созвездия")
        apply_gnss.clicked.connect(self._apply_constellations)
        gl.addWidget(apply_gnss)

        msg_box = QGroupBox("Вывод сообщений")
        ml = QHBoxLayout(msg_box)
        pvt_btn = QPushButton("Включить UBX-NAV-PVT на USB")
        pvt_btn.clicked.connect(
            lambda: self.valset_requested.emit([(cfg.NAV_PVT_USB_KEY, 1)]))
        ml.addWidget(pvt_btn)
        ml.addStretch()

        self._buttons = [apply_rate, apply_gnss, pvt_btn, read_btn]
        for b in (rate_box, gnss_box, msg_box):
            lay.addWidget(b)
        lay.addStretch()

    def set_connected(self, on: bool):
        for b in self._buttons:
            b.setEnabled(on)

    def apply_values(self, values: dict[str, int]):
        """Показывает текущие значения из CFG-VALGET."""
        if cfg.RATE_KEY in values:
            idx = self.rate_combo.findData(values[cfg.RATE_KEY])
            if idx >= 0:
                self.rate_combo.setCurrentIndex(idx)
        for key, cb in self.gnss_cbs.items():
            if key in values:
                cb.setChecked(bool(values[key]))

    def _apply_rate(self):
        self.valset_requested.emit([(cfg.RATE_KEY, int(self.rate_combo.currentData()))])

    def _apply_constellations(self):
        if not any(cb.isChecked() for cb in self.gnss_cbs.values()):
            QMessageBox.warning(self, "Созвездия", "Должно быть включено хотя бы одно.")
            return
        self.valset_requested.emit([(k, int(cb.isChecked())) for k, cb in self.gnss_cbs.items()])
