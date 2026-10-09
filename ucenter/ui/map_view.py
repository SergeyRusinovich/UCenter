"""Тайловая карта (OSM или спутник; только PyQt6, без WebEngine): отрисовка и управление."""
import math

from PyQt6.QtCore import Qt, QPointF, QRectF, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen
from PyQt6.QtWidgets import QWidget, QPushButton

from ucenter.core.geo import TILE_SIZE as TILE, ll_to_px, px_to_ll
from ucenter.ui.map_buttons import CrosshairButton
from ucenter.ui.tile_loader import OSM, TileLoader, TileSource

MIN_ZOOM, MAX_ZOOM = 3, 19
FIX_ZOOM = 16      # масштаб, с которым карта открывается на первой позиции
CLICK_SLOP = 4     # px: сдвиг мыши меньше этого — щелчок, а не перетаскивание


class MapView(QWidget):
    clicked = pyqtSignal(float, float)  # широта, долгота точки щелчка (без перетаскивания)
    recentered = pyqtSignal()           # нажат «прицел»: вернуться к текущей позиции

    def __init__(self, parent=None, need_fix: bool = True, source: TileSource = OSM):
        """need_fix=False — карта доступна и до первой позиции (показывает мир)."""
        super().__init__(parent)
        self.setMinimumSize(320, 260)
        self._need_fix = need_fix
        self.zoom = FIX_ZOOM if need_fix else MIN_ZOOM
        self.follow = True
        self._pos: tuple[float, float] | None = None
        self._center = (0.0, 0.0)
        self._drag_last = None
        self._press_at = None
        self._tiles = TileLoader(source, self)
        self._tiles.tile_loaded.connect(self.update)

        self._btn_in = self._make_button("+", lambda: self._set_zoom(self.zoom + 1))
        self._btn_out = self._make_button("−", lambda: self._set_zoom(self.zoom - 1))
        self._btn_center = self._make_button("", self._recenter, CrosshairButton)
        self._btn_center.setToolTip("Center on the current position")
        self._buttons = [self._btn_in, self._btn_out, self._btn_center]  # столбец справа сверху

    def _make_button(self, text, slot, cls=QPushButton) -> QPushButton:
        b = cls(text, self)
        b.setObjectName("mapButton")
        b.setFixedSize(28, 28)
        b.clicked.connect(slot)
        return b

    # ---------- Публичный API ----------
    def set_position(self, lat: float | None, lon: float | None):
        if lat is None or lon is None or self._pos == (lat, lon):
            return
        if self._pos is None and self.follow:
            self.zoom = max(self.zoom, FIX_ZOOM)
        self._pos = (lat, lon)
        if self.follow:
            self._center = self._pos
        self.update()

    def set_source(self, source: TileSource):
        if source == self._tiles.source:
            return
        self._tiles.deleteLater()  # вместе с ним прерываются его незавершённые запросы
        self._tiles = TileLoader(source, self)
        self._tiles.tile_loaded.connect(self.update)
        self.update()

    def stop_following(self):
        """Карта больше не уезжает за позицией (пользователь работает с ней сам)."""
        self.follow = False

    def center_on(self, lat: float, lon: float):
        self.follow = False
        self._center = (lat, lon)
        self.update()

    def fit_bounds(self, south: float, west: float, north: float, east: float, max_zoom: int = 18):
        """Показывает прямоугольник целиком: центр в его середине, наибольший подходящий масштаб."""
        self.follow = False
        self._center = ((south + north) / 2, (west + east) / 2)
        w, h = self.width() * 0.8, self.height() * 0.8
        for z in range(max_zoom, MIN_ZOOM - 1, -1):
            x0, y0 = ll_to_px(north, west, z)
            x1, y1 = ll_to_px(south, east, z)
            if x1 - x0 <= w and y1 - y0 <= h:
                break
        self.zoom = z
        self.update()

    def visible_bounds(self) -> tuple[float, float, float, float]:
        """Видимая область: юг, запад, север, восток."""
        north, west = self.to_geo(QPointF(0, 0))
        south, east = self.to_geo(QPointF(self.width(), self.height()))
        return south, west, north, east

    def to_screen(self, lat: float, lon: float) -> QPointF:
        x0, y0 = self._origin()
        x, y = ll_to_px(lat, lon, self.zoom)
        return QPointF(x - x0, y - y0)

    def to_geo(self, pt: QPointF) -> tuple[float, float]:
        x0, y0 = self._origin()
        lat, lon = px_to_ll(x0 + pt.x(), y0 + pt.y(), self.zoom)
        return lat, (lon + 180.0) % 360.0 - 180.0

    def _origin(self) -> tuple[float, float]:
        """Мировые пиксели левого верхнего угла виджета."""
        cx, cy = ll_to_px(*self._center, self.zoom)
        return cx - self.width() / 2, cy - self.height() / 2

    def _ready(self) -> bool:
        return self._pos is not None or not self._need_fix

    # ---------- Управление ----------
    def _set_zoom(self, z: int):
        self.zoom = max(MIN_ZOOM, min(MAX_ZOOM, z))
        self.update()

    def _recenter(self):
        self.follow = True
        if self._pos:
            self._center = self._pos
        self.update()
        self.recentered.emit()

    def resizeEvent(self, _):
        x = self.width() - 36
        for i, b in enumerate(self._buttons):
            b.move(x, 8 + i * 32)

    def wheelEvent(self, e):
        self._set_zoom(self.zoom + (1 if e.angleDelta().y() > 0 else -1))

    def mousePressEvent(self, e):
        self._drag_last = self._press_at = e.position()

    def mouseMoveEvent(self, e):
        if self._drag_last is None or not self._ready():
            return
        if self._press_at is not None:
            if (e.position() - self._press_at).manhattanLength() < CLICK_SLOP:
                return
            self._press_at = None  # началось перетаскивание
        d = e.position() - self._drag_last
        self._drag_last = e.position()
        cx, cy = ll_to_px(*self._center, self.zoom)
        self._center = px_to_ll(cx - d.x(), cy - d.y(), self.zoom)
        self.follow = False
        self.update()

    def mouseReleaseEvent(self, e):
        if self._press_at is not None and self._ready() and e.button() == Qt.MouseButton.LeftButton:
            self.clicked.emit(*self.to_geo(e.position()))
        self._drag_last = self._press_at = None

    # ---------- Отрисовка ----------
    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.fillRect(self.rect(), self.palette().alternateBase())
        if not self._ready():
            p.setPen(self.palette().placeholderText().color())
            p.drawText(self.rect(), Qt.AlignmentFlag.AlignCenter, "Waiting for position fix…")
            return
        w, h, z = self.width(), self.height(), self.zoom
        x0, y0 = self._origin()
        n = 2 ** z
        for tx in range(math.floor(x0 / TILE), math.floor((x0 + w) / TILE) + 1):
            for ty in range(math.floor(y0 / TILE), math.floor((y0 + h) / TILE) + 1):
                if 0 <= ty < n:
                    pm = self._tiles.tile(z, tx % n, ty)
                    if pm is not None:
                        p.drawPixmap(int(tx * TILE - x0), int(ty * TILE - y0), pm)
        if self._pos is not None:
            p.setPen(QPen(QColor("white"), 3))
            p.setBrush(QColor("#e02020"))
            p.drawEllipse(self.to_screen(*self._pos), 8, 8)
        p.save()
        self._paint_overlay(p)
        p.restore()

        f = p.font()
        f.setPointSize(8)
        p.setFont(f)
        text = self._tiles.source.attribution
        tw = p.fontMetrics().horizontalAdvance(text) + 8
        box = QRectF(w - tw, h - 16, tw, 16)
        p.fillRect(box, QColor(255, 255, 255, 190))
        p.setPen(QColor("#333"))
        p.drawText(box, Qt.AlignmentFlag.AlignCenter, text)

    def _paint_overlay(self, p: QPainter):
        """Дополнительные слои поверх тайлов (для наследников)."""
