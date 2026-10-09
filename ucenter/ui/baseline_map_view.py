"""Карта для измерения A->B: маркеры точек и флажок найденного места (перетаскиваются мышью),
линия между точками, подсветка найденного места, состояние геопозиции на «прицеле»."""
import math
from dataclasses import replace

from PyQt6.QtCore import Qt, QPointF, QRectF, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QPolygonF, QPainterPath

from ucenter.core.geo import TILE_SIZE
from ucenter.core.places import Place, Ring
from ucenter.ui.location_popover import LocationPopover
from ucenter.ui.map_buttons import ExpandButton
from ucenter.ui.map_view import MapView
from ucenter.ui.tile_loader import ESRI_IMAGERY

POINT_COLORS = {"A": QColor("#1FA35A"), "B": QColor("#1e6fff")}
MARKER_R = 11
HIT_R = 15  # px: радиус, в котором нажатие захватывает маркер
FLAG = "flag"  # ключ захвата флажка
EQUATOR_M = 40075016.686  # длина экватора WGS84, м
HIGHLIGHT = QColor("#FFB300")
FLAG_COLOR = QColor("#E53935")
FLAG_POLE = 34   # px: высота древка
LABEL_MAX_W = 220
LOCATE_TIP = "My location: center the map and put point A here"
ALERT_COLORS = {"wait": QColor("#F5A623"), "error": QColor("#D93F3F")}


class BaselineMapView(MapView):
    point_moved = pyqtSignal(str, float, float)  # ключ точки, широта, долгота
    place_moved = pyqtSignal(float, float)       # новое место флажка
    expand_toggled = pyqtSignal(bool)            # карта во всё окно / обычный вид (кнопкой)
    location_retry_requested = pyqtSignal()      # «Retry» в карточке о геопозиции

    def __init__(self, parent=None):
        super().__init__(parent, need_fix=False, source=ESRI_IMAGERY)
        self._points: dict[str, tuple[float, float] | None] = {"A": None, "B": None}
        self._grab: str | None = None
        self._grab_offset = QPointF()  # от курсора до точки привязки: объект не прыгает под курсор
        self._accuracy: float | None = None
        self._place: Place | None = None
        self.setMouseTracking(True)  # курсор-«рука» над тем, что можно перетащить
        self._btn_center.setToolTip(LOCATE_TIP)
        self._btn_expand = self._make_button("", self.expand_toggled.emit, ExpandButton)
        self._btn_expand.setCheckable(True)
        self._btn_expand.setToolTip("Map over the whole window (Esc — back)")
        self._buttons.append(self._btn_expand)
        self._popover: LocationPopover | None = None

    # ---------- Развёрнутый вид ----------
    def is_expanded(self) -> bool:
        return self._btn_expand.isChecked()

    def set_expanded(self, on: bool):
        """Программная смена вида; expand_toggled не испускается (его шлёт только кнопка)."""
        self._btn_expand.setChecked(on)

    def set_place(self, place: Place | None):
        """Найденное место: подсветка контура и флажок (None — убрать)."""
        self._place = place
        self.update()

    def set_location_alert(self, level: str | None, text: str = ""):
        """Состояние геопозиции на «прицеле»: точка-индикатор и подсказка (None/"ok" — всё в порядке)."""
        self._btn_center.set_alert(ALERT_COLORS.get(level))
        self._btn_center.setToolTip(f"{LOCATE_TIP}\n\n{text}" if text else LOCATE_TIP)

    def show_location_help(self, level: str, text: str):
        """Карточка у «прицела»: почему нет геопозиции и что сделать."""
        if self._popover is None:
            self._popover = LocationPopover(self)
            self._popover.retry_requested.connect(self.location_retry_requested)
        self._popover.show_at(self._btn_center, level, text)

    def set_accuracy(self, meters: float | None):
        """Радиус неопределённости позиции (круг вокруг неё)."""
        self._accuracy = meters

    def set_point(self, key: str, ll: tuple[float, float] | None):
        self._points[key] = ll
        self.update()

    def _anchor(self, key: str) -> QPointF | None:
        """Экранная точка привязки маркера или основания флажка."""
        if key == FLAG:
            return None if self._place is None else self.to_screen(self._place.lat, self._place.lon)
        ll = self._points[key]
        return None if ll is None else self.to_screen(*ll)

    def _hit(self, pos: QPointF) -> str | None:
        for key in ("B", "A"):  # маркеры рисуются поверх флажка, B — поверх A
            pt = self._anchor(key)
            if pt is not None and (pt - pos).manhattanLength() <= HIT_R:
                return key
        base = self._anchor(FLAG)
        if base is not None:
            # древко и полотнище флажка
            if QRectF(base.x() - 6, base.y() - FLAG_POLE - 4, 32, FLAG_POLE + 10).contains(pos):
                return FLAG
        return None

    # ---------- Перетаскивание маркеров и флажка ----------
    def mousePressEvent(self, e):
        self._grab = self._hit(e.position()) if e.button() == Qt.MouseButton.LeftButton else None
        if self._grab is None:
            super().mousePressEvent(e)
            return
        self._grab_offset = self._anchor(self._grab) - e.position()
        self.setCursor(Qt.CursorShape.ClosedHandCursor)

    def mouseMoveEvent(self, e):
        if self._grab is None:
            if not e.buttons():
                hover = self._hit(e.position()) is not None
                self.setCursor(Qt.CursorShape.OpenHandCursor if hover else Qt.CursorShape.ArrowCursor)
            super().mouseMoveEvent(e)
            return
        lat, lon = self.to_geo(e.position() + self._grab_offset)
        if self._grab == FLAG:
            self._place = replace(self._place, lat=lat, lon=lon)
            self.place_moved.emit(lat, lon)
        else:
            self._points[self._grab] = (lat, lon)
            self.point_moved.emit(self._grab, lat, lon)
        self.update()

    def mouseReleaseEvent(self, e):
        if self._grab is None:
            super().mouseReleaseEvent(e)
            return
        self._grab = None
        self.setCursor(Qt.CursorShape.OpenHandCursor)

    # ---------- Отрисовка ----------
    def _paint_overlay(self, p: QPainter):
        if self._pos is not None and self._accuracy:
            m_per_px = EQUATOR_M * math.cos(math.radians(self._pos[0])) / (TILE_SIZE * 2 ** self.zoom)
            r = self._accuracy / m_per_px
            if r > 10:
                p.setPen(QPen(QColor(255, 255, 255, 200), 1.5))
                p.setBrush(QColor(30, 111, 255, 45))
                p.drawEllipse(self.to_screen(*self._pos), r, r)
        if self._place is not None:
            self._paint_place_area(p, self._place)
        a, b = self._points["A"], self._points["B"]
        if a is not None and b is not None:
            pa, pb = self.to_screen(*a), self.to_screen(*b)
            for color, width in (("white", 5), ("#202124", 2)):
                p.setPen(QPen(QColor(color), width, Qt.PenStyle.SolidLine, Qt.PenCapStyle.RoundCap))
                p.drawLine(pa, pb)
            self._paint_arrow(p, pa, pb)
        if self._place is not None:
            self._paint_flag(p, self._place)
        f = p.font()
        f.setBold(True)
        p.setFont(f)
        for key in ("A", "B"):
            ll = self._points[key]
            if ll is None:
                continue
            pt = self.to_screen(*ll)
            p.setPen(QPen(QColor("white"), 2.5))
            p.setBrush(POINT_COLORS[key])
            p.drawEllipse(pt, MARKER_R, MARKER_R)
            p.setPen(QColor("white"))
            p.drawText(QRectF(pt.x() - MARKER_R, pt.y() - MARKER_R, 2 * MARKER_R, 2 * MARKER_R),
                       Qt.AlignmentFlag.AlignCenter, key)

    def _path(self, ring: Ring, closed: bool) -> QPainterPath:
        path = QPainterPath()
        path.addPolygon(QPolygonF([self.to_screen(lat, lon) for lat, lon in ring]))
        if closed:
            path.closeSubpath()
        return path

    def _paint_place_area(self, p: QPainter, place: Place):
        fill = QColor(HIGHLIGHT)
        fill.setAlpha(70)
        for rings, closed in ((place.areas, True), (place.lines, False)):
            for ring in rings:
                path = self._path(ring, closed)
                p.setBrush(fill if closed else Qt.BrushStyle.NoBrush)
                p.setPen(QPen(QColor(255, 255, 255, 220), 5, cap=Qt.PenCapStyle.RoundCap,
                              join=Qt.PenJoinStyle.RoundJoin))
                p.drawPath(path)
                p.setPen(QPen(HIGHLIGHT, 2.5, cap=Qt.PenCapStyle.RoundCap,
                              join=Qt.PenJoinStyle.RoundJoin))
                p.drawPath(path)

    def _paint_flag(self, p: QPainter, place: Place):
        """Флажок: древко стоит основанием на месте, рядом — название."""
        base = self.to_screen(place.lat, place.lon)
        top = base - QPointF(0, FLAG_POLE)
        p.setPen(QPen(QColor("white"), 2))
        p.setBrush(HIGHLIGHT)
        p.drawEllipse(base, 4, 4)
        for color, width in (("white", 4), ("#202124", 2)):
            p.setPen(QPen(QColor(color), width, cap=Qt.PenCapStyle.RoundCap))
            p.drawLine(base, top)
        cloth = QPolygonF([top, top + QPointF(22, 7), top + QPointF(0, 14)])
        p.setPen(QPen(QColor("white"), 1.5, join=Qt.PenJoinStyle.RoundJoin))
        p.setBrush(FLAG_COLOR)
        p.drawPolygon(cloth)

        fm = p.fontMetrics()
        text = fm.elidedText(place.name, Qt.TextElideMode.ElideRight, LABEL_MAX_W)
        box = QRectF(top.x() + 26, top.y() - 2, fm.horizontalAdvance(text) + 12, fm.height() + 4)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(255, 255, 255, 230))
        p.drawRoundedRect(box, 4, 4)
        p.setPen(QColor("#202124"))
        p.drawText(box, Qt.AlignmentFlag.AlignCenter, text)

    @staticmethod
    def _paint_arrow(p: QPainter, pa: QPointF, pb: QPointF):
        """Стрелка направления A->B у середины отрезка."""
        d = pb - pa
        length = math.hypot(d.x(), d.y())
        if length < 4 * MARKER_R:
            return
        ux, uy = d.x() / length, d.y() / length
        tip = (pa + pb) / 2 + QPointF(ux, uy) * 7
        base = tip - QPointF(ux, uy) * 14
        side = QPointF(-uy, ux) * 6
        p.setPen(QPen(QColor("white"), 1.5))
        p.setBrush(QColor("#202124"))
        p.drawPolygon(QPolygonF([tip, base + side, base - side]))
