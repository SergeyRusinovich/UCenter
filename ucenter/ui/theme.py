"""Цвета для отображения спутников."""
from PyQt6.QtGui import QColor

SYSTEM_COLORS = {
    "G": QColor("#f5a742"),
    "S": QColor("#9aa0a6"),
    "E": QColor("#f2e82e"),
    "B": QColor("#a4d65e"),
    "R": QColor("#6bb6e3"),
    "Q": QColor("#4db6ac"),
}
NOT_USED_COLOR = QColor("#1e3cff")  # обводка: отслеживается, но не используется в решении
LOST_COLOR = QColor("#e02020")      # обводка: спутник не отслеживается
