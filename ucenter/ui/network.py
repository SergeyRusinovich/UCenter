"""Общее для сетевых запросов к картографическим сервисам."""
from PyQt6.QtCore import QUrl
from PyQt6.QtNetwork import QNetworkRequest

USER_AGENT = "UCenter/1.0 (desktop GNSS viewer)"  # требуется политиками OSM и Nominatim


def make_request(url: QUrl) -> QNetworkRequest:
    req = QNetworkRequest(url)
    req.setHeader(QNetworkRequest.KnownHeaders.UserAgentHeader, USER_AGENT)
    return req
