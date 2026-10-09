"""Поиск мест через Nominatim (OpenStreetMap) с ответами на русском.

Политика Nominatim: не чаще 1 запроса в секунду, свой User-Agent, без автодополнения —
поэтому поиск запускается только по Enter, а слишком частый запрос откладывается.
"""
import json
import time

from PyQt6.QtCore import QObject, QTimer, QUrl, QUrlQuery, pyqtSignal
from PyQt6.QtNetwork import QNetworkAccessManager, QNetworkReply

from ucenter.core.places import Place, merge_duplicates, parse_nominatim
from ucenter.ui.network import make_request

SEARCH_URL = "https://nominatim.openstreetmap.org/search"
LANGUAGE = "ru"
MAX_RESULTS = 8
MIN_INTERVAL = 1.0  # секунд между запросами
ATTRIBUTION = "Поиск: © OpenStreetMap contributors (Nominatim)"


class PlaceSearch(QObject):
    results = pyqtSignal(str, list)  # запрос, list[Place]
    failed = pyqtSignal(str)         # текст ошибки

    def __init__(self, parent=None):
        super().__init__(parent)
        self._nam = QNetworkAccessManager(self)
        self._reply: QNetworkReply | None = None
        self._last = 0.0
        self._timer = QTimer(self, singleShot=True)
        self._timer.timeout.connect(self._send)
        self._query: tuple[str, tuple[float, float, float, float] | None] | None = None

    def search(self, text: str, viewbox: tuple[float, float, float, float] | None = None):
        """viewbox (юг, запад, север, восток) — видимая область: близкие места выше в списке."""
        text = text.strip()
        if not text:
            return
        self._query = (text, viewbox)
        wait = MIN_INTERVAL - (time.monotonic() - self._last)
        self._timer.start(max(0, int(wait * 1000)))

    def _send(self):
        if self._query is None:
            return
        text, viewbox = self._query
        if self._reply is not None:  # новый запрос заменяет незавершённый
            self._reply.finished.disconnect()
            self._reply.abort()
            self._reply.deleteLater()
        q = QUrlQuery()
        for key, value in (("q", text), ("format", "jsonv2"), ("limit", str(MAX_RESULTS)),
                           ("polygon_geojson", "1"), ("polygon_threshold", "0.00005"),
                           ("accept-language", LANGUAGE)):
            q.addQueryItem(key, value)
        if viewbox is not None:
            south, west, north, east = viewbox
            q.addQueryItem("viewbox", f"{west},{north},{east},{south}")
        url = QUrl(SEARCH_URL)
        url.setQuery(q)
        req = make_request(url)
        req.setRawHeader(b"Accept-Language", LANGUAGE.encode())
        self._last = time.monotonic()
        self._reply = reply = self._nam.get(req)
        reply.finished.connect(lambda: self._on_reply(reply, text))

    def _on_reply(self, reply: QNetworkReply, text: str):
        if reply is self._reply:
            self._reply = None
        reply.deleteLater()
        if reply.error() != QNetworkReply.NetworkError.NoError:
            self.failed.emit(f"Ошибка поиска: {reply.errorString()}")
            return
        try:
            places: list[Place] = merge_duplicates(parse_nominatim(json.loads(bytes(reply.readAll()))))
        except (ValueError, TypeError):
            self.failed.emit("Ошибка поиска: неожиданный ответ сервера")
            return
        self.results.emit(text, places)
