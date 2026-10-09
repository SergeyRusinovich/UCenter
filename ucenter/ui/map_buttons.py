"""Круглые кнопки карты со значками, нарисованными цветом текста текущей темы."""
from PyQt6.QtCore import Qt, QPointF
from PyQt6.QtGui import QPainter, QColor, QPalette, QPen
from PyQt6.QtWidgets import QPushButton


class IconButton(QPushButton):
    """Кнопка карты со значком, нарисованным цветом текста текущей темы."""

    def paintEvent(self, e):
        super().paintEvent(e)
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        group = QPalette.ColorGroup.Active if self.isEnabled() else QPalette.ColorGroup.Disabled
        self.paint_icon(p, QPointF(self.width() / 2, self.height() / 2),
                        self.palette().color(group, QPalette.ColorRole.ButtonText))

    def paint_icon(self, p: QPainter, c: QPointF, color: QColor):
        raise NotImplementedError


class CrosshairButton(IconButton):
    """«Прицел»: вернуться к текущей позиции. Цветная точка в углу — проблема с позицией."""

    def __init__(self, *args):
        super().__init__(*args)
        self._alert: QColor | None = None

    def set_alert(self, color: QColor | None):
        self._alert = color
        self.update()

    def paintEvent(self, e):
        super().paintEvent(e)
        if self._alert is not None:
            p = QPainter(self)
            p.setRenderHint(QPainter.RenderHint.Antialiasing)
            p.setPen(QPen(QColor("white"), 1.5))
            p.setBrush(self._alert)
            p.drawEllipse(QPointF(self.width() - 6, 6), 4.5, 4.5)

    def paint_icon(self, p: QPainter, c: QPointF, color: QColor):
        p.setPen(QPen(color, 1.6))
        p.setBrush(Qt.BrushStyle.NoBrush)
        p.drawEllipse(c, 5.5, 5.5)
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            d = QPointF(dx, dy)
            p.drawLine(c + d * 3, c + d * 9)
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(color)
        p.drawEllipse(c, 1.5, 1.5)


class ExpandButton(IconButton):
    """Развернуть/свернуть карту: уголки наружу или (когда нажата) внутрь."""

    def paint_icon(self, p: QPainter, c: QPointF, color: QColor):
        p.setPen(QPen(color, 1.6, cap=Qt.PenCapStyle.RoundCap, join=Qt.PenJoinStyle.RoundJoin))
        inward = self.isChecked()
        for sx in (-1, 1):
            for sy in (-1, 1):
                corner = c + QPointF(sx * (2 if inward else 7), sy * (2 if inward else 7))
                arm = 4 if inward else -4  # усы уголка: наружу у «свернуть», к центру у «развернуть»
                p.drawLine(corner, corner + QPointF(sx * arm, 0))
                p.drawLine(corner, corner + QPointF(0, sy * arm))
