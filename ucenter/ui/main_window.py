"""Главное окно: собирает модели, сеанс приёмника и вкладки и связывает их сигналами."""
from PyQt6.QtCore import QTimer
from PyQt6.QtWidgets import QMainWindow, QWidget, QTabWidget, QVBoxLayout, QStatusBar, QMessageBox

from ucenter.core.navigation import NavigationState
from ucenter.core.ports import find_ports
from ucenter.core.satellites import SatelliteTracker
from ucenter.core.session import ReceiverSession
from ucenter.ui.config_tab import ConfigTab
from ucenter.ui.connection_panel import ConnectionPanel
from ucenter.ui.console_tab import ConsoleTab
from ucenter.ui.dashboard_tab import DashboardTab
from ucenter.ui.satellites_tab import SatellitesTab

UI_INTERVAL_MS = 100   # не чаще 10 перерисовок Dashboard/консоли в секунду
SAT_INTERVAL_MS = 1000
STATUS_MS = 5000


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("UCenter — u-blox M9")
        self.resize(1000, 750)
        self.session = ReceiverSession(self)
        self.nav = NavigationState()
        self.tracker = SatelliteTracker()
        self._nav_dirty = False
        self._error_shown = False

        self.connection = ConnectionPanel(find_ports)
        self.console_tab = ConsoleTab()
        self.dashboard_tab = DashboardTab()
        self.satellites_tab = SatellitesTab()
        self.config_tab = ConfigTab()
        self._build_layout()
        self._wire()

        # Сообщения только обновляют модели, а перерисовка идёт по таймерам
        self._start_timer(UI_INTERVAL_MS, self._refresh_ui)
        self._start_timer(SAT_INTERVAL_MS, self._refresh_satellites)

    def _build_layout(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.addWidget(self.connection)
        self.tabs = QTabWidget()
        for widget, title in ((self.console_tab, "Console"),
                              (self.dashboard_tab, "Dashboard"),
                              (self.satellites_tab, "Satellites"),
                              (self.config_tab, "Configuration")):
            self.tabs.addTab(widget, title)
        root.addWidget(self.tabs)
        self.setStatusBar(QStatusBar())

    def _wire(self):
        s = self.session
        self.connection.connect_requested.connect(self._connect)
        self.connection.disconnect_requested.connect(s.close)
        self.config_tab.valset_requested.connect(s.apply_config)
        self.config_tab.refresh_requested.connect(s.poll_config)
        s.connected_changed.connect(self.connection.set_connected)
        s.connected_changed.connect(self.config_tab.set_connected)
        s.message.connect(self._on_message)
        s.config_values.connect(self.config_tab.apply_values)
        s.status.connect(lambda text: self.statusBar().showMessage(text, STATUS_MS))
        s.error.connect(self._on_error)
        self.config_tab.set_connected(False)

    def _start_timer(self, interval_ms: int, slot):
        timer = QTimer(self)
        timer.timeout.connect(slot)
        timer.start(interval_ms)

    def _connect(self, port: str, baud: int):
        self.connection.set_busy()
        self.session.open(port, baud)

    def _on_error(self, text: str):
        self.statusBar().showMessage(text, 2 * STATUS_MS)
        if self._error_shown:  # не плодим окна при серии ошибок
            return
        self._error_shown = True
        try:
            QMessageBox.critical(self, "Ошибка порта", text)
        finally:
            self._error_shown = False

    # ---------- Данные ----------
    def _on_message(self, parsed, raw: bytes):
        self.console_tab.append_message(parsed, raw)
        if self.nav.apply(parsed):
            self._nav_dirty = True
        self.tracker.handle(parsed)

    def _refresh_ui(self):
        if self._nav_dirty:
            self._nav_dirty = False
            self.dashboard_tab.update_state(self.nav.data)
        self.console_tab.flush()

    def _refresh_satellites(self):
        self.satellites_tab.update_data(*self.tracker.snapshot())

    def closeEvent(self, event):
        self.session.close()
        super().closeEvent(event)
