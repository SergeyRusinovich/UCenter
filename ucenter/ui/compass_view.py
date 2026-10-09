"""Компас: показывает курс из датчика приёмника или сообщение, что компаса нет."""
import math

from PyQt6.QtCore import Qt, QPointF, QRectF
from PyQt6.QtGui import QPainter, QColor, QPen, QPolygonF
from PyQt6.QtWidgets import QWidget

from ucenter.ui.painting import polar

UNSUPPORTED_TEXT = "Compass is not supported by this device\n(no heading sensor data from the receiver)"


class CompassView(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(200, 200)
        self.supported = False
        self.heading: float | None = None
        self.cog: float | None = None

    def set_state(self, supported: bool, heading, cog):
        self.supported, self.heading, self.cog = supported, heading, cog
        self.update()

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        c = QPointF(self.width() / 2, self.height() / 2)
        r = min(self.width(), self.height()) / 2 - 22
        text = self.palette().text().color()
        dial = QColor(text)
        dial.setAlpha(255 if self.supported else 90)

        p.setPen(QPen(dial, 2))
        p.drawEllipse(c, r, r)
        for deg in range(0, 360, 30):
            p.drawLine(polar(c, r, deg), polar(c, r - (10 if deg % 90 == 0 else 5), deg))
        p.setPen(dial)
        for txt, deg in (("N", 0), ("E", 90), ("S", 180), ("W", 270)):
            pt = polar(c, r + 12, deg)
            p.drawText(QRectF(pt.x() - 8, pt.y() - 8, 16, 16), Qt.AlignmentFlag.AlignCenter, txt)

        # Курс движения по земле — маркер на ободе, доступен и без датчика
        if self.cog is not None:
            tip, base = polar(c, r - 2, self.cog), polar(c, r - 14, self.cog)
            side = QPointF(math.cos(math.radians(self.cog)) * 5, math.sin(math.radians(self.cog)) * 5)
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(QColor("#1e6fff"))
            p.drawPolygon(QPolygonF([tip, base + side, base - side]))

        if self.supported and self.heading is not None:
            tip = polar(c, r - 18, self.heading)
            tail = polar(c, 14, self.heading + 180)
            side = QPointF(math.cos(math.radians(self.heading)) * 7,
                           math.sin(math.radians(self.heading)) * 7)
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(QColor("#e02020"))
            p.drawPolygon(QPolygonF([tip, c + side, c - side]))
            p.setBrush(QColor("#888"))
            p.drawPolygon(QPolygonF([tail, c + side, c - side]))
            p.setPen(text)
            p.drawText(QRectF(c.x() - 40, c.y() + r * 0.35, 80, 18),
                       Qt.AlignmentFlag.AlignCenter, f"{self.heading:.0f}°")
        elif not self.supported:
            p.setPen(text)
            p.drawText(QRectF(c.x() - r * 0.8, c.y() - r * 0.5, r * 1.6, r),
                       Qt.AlignmentFlag.AlignCenter | Qt.TextFlag.TextWordWrap, UNSUPPORTED_TEXT)
