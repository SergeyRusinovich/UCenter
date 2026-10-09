"""Загрузка тайлов карты: сеть, кэш в памяти и на диске, повторы с паузой."""
import os
import time
from collections import OrderedDict
from dataclasses import dataclass

from PyQt6.QtCore import QObject, QStandardPaths, QUrl, pyqtSignal
from PyQt6.QtGui import QPixmap
from PyQt6.QtNetwork import (
    QNetworkAccessManager, QNetworkDiskCache, QNetworkRequest, QNetworkReply,
)

from ucenter.ui.network import make_request

MEMORY_TILES = 300
MAX_PENDING = 64
DISK_CACHE_BYTES = 200 * 1024 * 1024
RETRY_MIN, RETRY_MAX = 10.0, 300.0  # секунд до повтора неудачного тайла (растёт вдвое)


@dataclass(frozen=True)
class TileSource:
    url: str          # шаблон с {z}, {x}, {y}
    attribution: str
    cache_dir: str    # подкаталог дискового кэша


OSM = TileSource("https://tile.openstreetmap.org/{z}/{x}/{y}.png",
                 "© OpenStreetMap contributors", "osm-tiles")
ESRI_IMAGERY = TileSource(
    "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
    "Imagery © Esri, Maxar, Earthstar Geographics", "esri-imagery")
# Запасной вариант там, где Esri недоступен. Прямой доступ к тайлам Google не разрешён
# условиями Google Maps Platform: только для личного/внутреннего использования.
GOOGLE_IMAGERY = TileSource("https://mt1.google.com/vt/lyrs=s&x={x}&y={y}&z={z}",
                            "Imagery © Google", "google-imagery")


class TileLoader(QObject):
    """Отдаёт тайл из кэша или запрашивает его; tile_loaded — когда пришёл новый."""

    tile_loaded = pyqtSignal()

    def __init__(self, source: TileSource = OSM, parent=None):
        super().__init__(parent)
        self.source = source
        self._cache: OrderedDict[str, QPixmap] = OrderedDict()
        self._pending: set[str] = set()
        self._failed: dict[str, tuple[float, float]] = {}  # тайл -> (время повтора, пауза)

        self._nam = QNetworkAccessManager(self)
        # Дисковый кэш: тайлы переживают перезапуск, как требует политика OSM
        disk = QNetworkDiskCache(self)
        base = QStandardPaths.writableLocation(QStandardPaths.StandardLocation.CacheLocation)
        disk.setCacheDirectory(os.path.join(base, source.cache_dir))
        disk.setMaximumCacheSize(DISK_CACHE_BYTES)
        self._nam.setCache(disk)
        self._nam.setRedirectPolicy(QNetworkRequest.RedirectPolicy.NoLessSafeRedirectPolicy)
        self._nam.finished.connect(self._on_reply)

    def tile(self, z: int, x: int, y: int) -> QPixmap | None:
        key = f"{z}/{x}/{y}"
        pm = self._cache.get(key)
        if pm is not None:
            self._cache.move_to_end(key)
            return pm
        failed = self._failed.get(key)
        if failed and time.monotonic() < failed[0]:
            return None  # ждём паузу, а не долбим сервер на каждой перерисовке
        if key not in self._pending and len(self._pending) < MAX_PENDING:
            self._request(key)
        return None

    def _request(self, key: str):
        self._pending.add(key)
        z, x, y = key.split("/")
        req = make_request(QUrl(self.source.url.format(z=z, x=x, y=y)))
        req.setAttribute(QNetworkRequest.Attribute.CacheLoadControlAttribute,
                         QNetworkRequest.CacheLoadControl.PreferCache)
        req.setAttribute(QNetworkRequest.Attribute.User, key)
        self._nam.get(req)

    def _on_reply(self, reply: QNetworkReply):
        key = reply.request().attribute(QNetworkRequest.Attribute.User)
        self._pending.discard(key)
        pm = QPixmap()
        if reply.error() == QNetworkReply.NetworkError.NoError and pm.loadFromData(reply.readAll()):
            self._failed.pop(key, None)
            self._cache[key] = pm
            while len(self._cache) > MEMORY_TILES:
                self._cache.popitem(last=False)
            self.tile_loaded.emit()
        else:
            delay = min(RETRY_MAX, self._failed[key][1] * 2) if key in self._failed else RETRY_MIN
            self._failed[key] = (time.monotonic() + delay, delay)
        reply.deleteLater()
