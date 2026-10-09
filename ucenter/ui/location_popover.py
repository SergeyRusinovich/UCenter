"""Всплывающая карточка у «прицела»: почему нет геопозиции компьютера и что сделать."""
import sys

from PyQt6.QtCore import QPoint, Qt, QUrl, pyqtSignal
from PyQt6.QtGui import QDesktopServices
from PyQt6.QtWidgets import QFrame, QHBoxLayout, QPushButton, QVBoxLayout, QWidget

from ucenter.ui.style import role_label

# Страница настроек геолокации ОС (нет для Linux — кнопка не показывается)
SETTINGS_URLS = {
    "darwin": "x-apple.systempreferences:com.apple.preference.security?Privacy_LocationServices",
    "win32": "ms-settings:privacy-location",
}
TITLES = {"error": "No location", "wait": "Locating…"}
WIDTH = 240


class LocationPopover(QWidget):
    retry_requested = pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent, Qt.WindowType.Popup | Qt.WindowType.FramelessWindowHint)
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)  # скруглённые углы карточки
        card = QFrame(self)
        card.setObjectName("popover")
        QVBoxLayout(self).addWidget(card)
        self.layout().setContentsMargins(0, 0, 0, 0)

        lay = QVBoxLayout(card)
        lay.setContentsMargins(12, 10, 12, 10)
        lay.setSpacing(6)
        self.title = role_label("", "section")
        self.text = role_label("", "caption")
        self.text.setWordWrap(True)
        self.text.setFixedWidth(WIDTH - 24)
        lay.addWidget(self.title)
        lay.addWidget(self.text)

        buttons = QHBoxLayout()
        url = SETTINGS_URLS.get(sys.platform)
        if url:
            settings = QPushButton("Settings")
            settings.setToolTip("Open the location settings of the OS")
            settings.setProperty("variant", "primary")
            settings.clicked.connect(lambda: (QDesktopServices.openUrl(QUrl(url)), self.close()))
            buttons.addWidget(settings)
        retry = QPushButton("Retry")
        retry.clicked.connect(lambda: (self.retry_requested.emit(), self.close()))
        buttons.addWidget(retry)
        buttons.addStretch()
        lay.addLayout(buttons)

    def show_at(self, anchor: QWidget, level: str, text: str):
        """Показывает карточку под anchor, выровненную по его правому краю."""
        self.title.setText(TITLES.get(level, TITLES["wait"]))
        self.text.setText(text or "Waiting for location…")
        self.adjustSize()
        corner = anchor.mapToGlobal(QPoint(anchor.width(), anchor.height() + 6))
        self.move(corner.x() - self.width(), corner.y())
        self.show()
