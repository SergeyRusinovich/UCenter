"""Общие помощники отрисовки."""
import math

from PyQt6.QtCore import QPointF


def polar(center: QPointF, radius: float, deg: float) -> QPointF:
    """Точка на расстоянии radius от center по азимуту deg (0° — вверх, по часовой)."""
    a = math.radians(deg)
    return QPointF(center.x() + radius * math.sin(a), center.y() - radius * math.cos(a))
