"""Геопозиция компьютера, на котором запущена программа (службы геолокации ОС, QtPositioning).

macOS выдаёт геопозицию только приложению с NSLocationUsageDescription в Info.plist,
поэтому там программу нужно запускать из UCenter.app (tools/make_macos_app.sh).
"""
import os
import sys

from PyQt6.QtCore import QCoreApplication, QLocationPermission, QObject, Qt, pyqtSignal
from PyQt6.QtPositioning import QGeoPositionInfo, QGeoPositionInfoSource

UPDATE_MS = 1000
OK, WAIT, ERROR = "ok", "wait", "error"
IN_APP_BUNDLE = os.environ.get("UCENTER_APP") == "1"  # выставляет лаунчер UCenter.app

# Короткие тексты: показываются в подсказке и карточке у «прицела»
if sys.platform == "darwin" and not IN_APP_BUNDLE:
    DENIED_TEXT = "Start UCenter.app to use location."  # python main.py доступа не получит
else:
    DENIED_TEXT = "Allow UCenter in Location Services."


class HostLocation(QObject):
    position = pyqtSignal(float, float, object)  # широта, долгота, точность (м) или None
    status = pyqtSignal(str, str)                # уровень: OK / WAIT / ERROR; текст для пользователя

    def __init__(self, parent=None):
        super().__init__(parent)
        self._source: QGeoPositionInfoSource | None = None
        self._started = False

    def start(self):
        """Запрашивает разрешение и запускает обновления (повторные вызовы игнорируются)."""
        if self._started:
            return
        self._started = True
        app = QCoreApplication.instance()
        perm = QLocationPermission()
        perm.setAccuracy(QLocationPermission.Accuracy.Precise)
        if app.checkPermission(perm) == Qt.PermissionStatus.Undetermined:
            self.status.emit(WAIT, "Requesting access…")
            app.requestPermission(perm, lambda p: self._on_permission(p.status()))
        else:
            self._on_permission(app.checkPermission(perm))

    def retry(self):
        """Повторная попытка — например, после того как доступ разрешили в настройках."""
        if self._source is not None:
            self._source.stopUpdates()
            self._source.deleteLater()
            self._source = None
        self._started = False
        self.start()

    def _on_permission(self, status: Qt.PermissionStatus):
        if status != Qt.PermissionStatus.Granted:
            self.status.emit(ERROR, DENIED_TEXT)
            return
        self._source = QGeoPositionInfoSource.createDefaultSource(self)
        if self._source is None:
            self.status.emit(ERROR, "No location provider on this computer.")
            return
        self._source.setUpdateInterval(UPDATE_MS)
        self._source.positionUpdated.connect(self._on_update)
        self._source.errorOccurred.connect(self._on_error)
        self._source.startUpdates()
        self.status.emit(WAIT, "Waiting for location…")

    def _on_update(self, info: QGeoPositionInfo):
        c = info.coordinate()
        if not c.isValid():
            return
        attr = QGeoPositionInfo.Attribute.HorizontalAccuracy
        acc = info.attribute(attr) if info.hasAttribute(attr) else None
        self.position.emit(c.latitude(), c.longitude(), acc)
        self.status.emit(OK, "")

    def _on_error(self, error: QGeoPositionInfoSource.Error):
        known = {
            QGeoPositionInfoSource.Error.AccessError: (ERROR, DENIED_TEXT),
            QGeoPositionInfoSource.Error.ClosedError: (ERROR, "Location Services are off."),
            QGeoPositionInfoSource.Error.UpdateTimeoutError: (WAIT, "No location yet…"),
        }
        self.status.emit(*known.get(error, (ERROR, f"Location error: {error.name}")))
