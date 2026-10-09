"""Карта на тайлах OpenStreetMap (только PyQt6, без WebEngine): отрисовка и управление."""
import math

from PyQt6.QtCore import Qt, QPointF, QRectF
from PyQt6.QtGui import QPainter, QColor, QPen
from PyQt6.QtWidgets import QWidget, QPushButton

from ucenter.core.geo import TILE_SIZE as TILE, ll_to_px, px_to_ll
from ucenter.ui.tile_loader import TileLoader

MIN_ZOOM, MAX_ZOOM = 3, 18
ATTRIBUTION = "© OpenStreetMap contributors"


class MapView(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(320, 260)
        self.zoom = 16
        self.follow = True
        self._pos: tuple[float, float] | None = None
        self._center = (0.0, 0.0)
        self._drag_last = None
        self._tiles = TileLoader(self)
        self._tiles.tile_loaded.connect(self.update)

        self._btn_in = self._make_button("+", lambda: self._set_zoom(self.zoom + 1))
        self._btn_out = self._make_button("−", lambda: self._set_zoom(self.zoom - 1))
        self._btn_center = self._make_button("◎", self._recenter)

    def _make_button(self, text, slot) -> QPushButton:
        b = QPushButton(text, self)
        b.setObjectName("mapButton")
        b.setFixedSize(28, 28)
        b.clicked.connect(slot)
        return b

    # ---------- Публичный API ----------
    def set_position(self, lat: float | None, lon: float | None):
        if lat is None or lon is None or self._pos == (lat, lon):
            return
        self._pos = (lat, lon)
        if self.follow:
            self._center = self._pos
        self.update()

    # ---------- Управление ----------
    def _set_zoom(self, z: int):
        self.zoom = max(MIN_ZOOM, min(MAX_ZOOM, z))
        self.update()

    def _recenter(self):
        self.follow = True
        if self._pos:
            self._center = self._pos
        self.update()

    def resizeEvent(self, _):
        x = self.width() - 36
        for i, b in enumerate((self._btn_in, self._btn_out, self._btn_center)):
            b.move(x, 8 + i * 32)

    def wheelEvent(self, e):
        self._set_zoom(self.zoom + (1 if e.angleDelta().y() > 0 else -1))

    def mousePressEvent(self, e):
        self._drag_last = e.position()

    def mouseMoveEvent(self, e):
        if self._drag_last is None or self._pos is None:
            return
        d = e.position() - self._drag_last
        self._drag_last = e.position()
        cx, cy = ll_to_px(*self._center, self.zoom)
        self._center = px_to_ll(cx - d.x(), cy - d.y(), self.zoom)
        self.follow = False
        self.update()

    def mouseReleaseEvent(self, _):
        self._drag_last = None

    # ---------- Отрисовка ----------
    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), self.palette().alternateBase())
        if self._pos is None:
            p.setPen(self.palette().placeholderText().color())
            p.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Waiting for position fix…")
            return
        w, h, z = self.width(), self.height(), self.zoom
        cx, cy = ll_to_px(*self._center, z)
        x0, y0 = cx - w / 2, cy - h / 2
        n = 2 ** z
        for tx in range(math.floor(x0 / TILE), math.floor((x0 + w) / TILE) + 1):
            for ty in range(math.floor(y0 / TILE), math.floor((y0 + h) / TILE) + 1):
                if 0 <= ty < n:
                    pm = self._tiles.tile(z, tx % n, ty)
                    if pm is not None:
                        p.drawPixmap(int(tx * TILE - x0), int(ty * TILE - y0), pm)
        mx, my = ll_to_px(*self._pos, z)
        pt = QPointF(mx - x0, my - y0)
        p.setPen(QPen(QColor("white"), 3))
        p.setBrush(QColor("#e02020"))
        p.drawEllipse(pt, 8, 8)

        f = p.font()
        f.setPointSize(8)
        p.setFont(f)
        tw = p.fontMetrics().horizontalAdvance(ATTRIBUTION) + 8
        box = QRectF(w - tw, h - 16, tw, 16)
        p.fillRect(box, QColor(255, 255, 255, 190))
        p.setPen(QColor("#333"))
        p.drawText(box, Qt.AlignmentFlag.AlignCenter, ATTRIBUTION)
