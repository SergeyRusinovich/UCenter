"""Виджеты отрисовки: небесная карта и диаграмма C/N0."""
import math

from PyQt6.QtCore import Qt, QPointF, QRectF
from PyQt6.QtGui import QPainter, QColor, QPen
from PyQt6.QtWidgets import QWidget

from ucenter.ui.painting import polar
from ucenter.ui.theme import SYSTEM_COLORS, NOT_USED_COLOR, LOST_COLOR

LABEL_GAP = 6  # минимальный зазор между подписями оси X, px


def _label_indices(systems: list[str], every: int) -> set[int]:
    """Какие столбики подписать: каждый every-й внутри группы системы, причём начало
    группы важнее — если оно слишком близко к предыдущей подписи, убираем ту."""
    result: list[int] = []
    start = 0
    for i, sys_char in enumerate(systems):
        if i and sys_char != systems[i - 1]:
            start = i
        if (i - start) % every:
            continue
        if result and i - result[-1] < every:
            if i != start:
                continue
            result.pop()  # начало группы вытесняет соседнюю подпись
        result.append(i)
    return set(result)


class SkyView(QWidget):
    """Небесная карта: азимут по окружности, возвышение к центру."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sats = []
        self.used = frozenset()
        self.marker = 10
        self.setMinimumSize(300, 300)

    def set_data(self, sats, used, marker):
        self.sats, self.used, self.marker = sats, used, marker
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        text = self.palette().text().color()

        # Спутники без азимута/возвышения (нет эфемерид) перечисляем над диаграммой,
        # отводя под надпись отдельную полосу, чтобы она не налезала на круг
        unplaced = [s.label for s in self.sats if s.elv is None or s.az is None]
        top = 0.0
        if unplaced:
            note = QRectF(6, 4, self.width() - 12, self.height() / 4)
            msg = "No azimuth/elevation yet: " + ", ".join(unplaced)
            flags = Qt.AlignmentFlag.AlignLeft | Qt.TextFlag.TextWordWrap
            note.setHeight(min(note.height(), p.boundingRect(note, flags, msg).height()))
            p.setPen(text)
            p.drawText(note, flags, msg)
            top = note.bottom() + 4

        side = min(self.width(), self.height() - top) - 36
        c = QPointF(self.width() / 2, top + (self.height() - top) / 2)
        r = max(side, 10) / 2
        p.setPen(QPen(text, 2))
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawEllipse(c, r, r)
        p.setPen(QPen(text, 1))
        p.drawEllipse(c, r * 2 / 3, r * 2 / 3)  # 30° возвышения
        p.drawEllipse(c, r / 3, r / 3)          # 60° возвышения
        p.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()))
        p.drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r))
        for txt, dx, dy in (("N", 0, -r - 10), ("S", 0, r + 10),
                            ("E", r + 10, 0), ("W", -r - 10, 0)):
            p.drawText(QRectF(c.x() + dx - 8, c.y() + dy - 8, 16, 16),
                       Qt.AlignmentFlag.AlignCenter, txt)
        m = self.marker
        for s in self.sats:
            if s.elv is None or s.az is None:
                continue
            pt = polar(c, r * (90 - s.elv) / 90, s.az)
            if not s.cno:
                p.setPen(QPen(LOST_COLOR, 3))
            elif s.key not in self.used:
                p.setPen(QPen(NOT_USED_COLOR, 3))
            else:
                p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(SYSTEM_COLORS[s.sys])
            p.drawEllipse(pt, m, m)
            p.setPen(QColor("black"))
            font = p.font()
            font.setPointSizeF(max(6.0, m * 0.65))
            p.setFont(font)
            p.drawText(QRectF(pt.x() - m, pt.y() - m, 2 * m, 2 * m),
                       Qt.AlignmentFlag.AlignCenter, s.label)
            p.setFont(self.font())


class SignalView(QWidget):
    """Столбчатая диаграмма C/N0 по спутникам, сгруппированная по системам."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.sats = []
        self.used = frozenset()
        self.setMinimumHeight(220)

    def set_data(self, sats, used):
        self.sats, self.used = sats, used
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        text = self.palette().text().color()
        left, bottom, top, right = 64, 24, 8, 8
        plot = QRectF(left, top, self.width() - left - right,
                      self.height() - top - bottom)
        grid = QColor(text)
        grid.setAlpha(50)
        for v in range(0, 51, 10):  # сетка и подписи оси Y
            y = plot.bottom() - plot.height() * v / 50
            p.setPen(QPen(grid, 1))
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y))
            p.setPen(text)
            p.drawText(QRectF(0, y - 8, left - 6, 16),
                       Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter,
                       f"{v} dB-Hz")
        if self.sats:
            step = plot.width() / len(self.sats)
            bw = max(4.0, min(28.0, step - 4))
            # Подписываем каждый N-й столбик, чтобы подписи не налезали друг на друга
            fm = p.fontMetrics()
            label_w = max(fm.horizontalAdvance(s.label) for s in self.sats) + LABEL_GAP
            labeled = _label_indices([s.sys for s in self.sats], max(1, math.ceil(label_w / step)))
            for i, s in enumerate(self.sats):
                cno = s.cno or 0
                x = plot.left() + i * step + (step - bw) / 2
                bh = plot.height() * min(cno, 50) / 50
                if cno and s.key not in self.used:
                    p.setPen(QPen(NOT_USED_COLOR, 2))
                else:
                    p.setPen(Qt.PenStyle.NoPen)
                p.setBrush(SYSTEM_COLORS[s.sys])
                p.drawRect(QRectF(x, plot.bottom() - bh, bw, bh))
                p.setPen(QColor("black"))
                if cno and bw >= 14:
                    p.drawText(QRectF(x, plot.bottom() - 16, bw, 14),
                               Qt.AlignmentFlag.AlignCenter, str(cno))
                if i in labeled:
                    cx = x + bw / 2
                    p.setPen(QPen(text, 1))
                    p.drawLine(QPointF(cx, plot.bottom()), QPointF(cx, plot.bottom() + 3))
                    p.drawText(QRectF(cx - label_w / 2, plot.bottom() + 3, label_w, 14),
                               Qt.AlignmentFlag.AlignCenter, s.label)

