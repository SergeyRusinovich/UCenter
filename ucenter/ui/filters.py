"""Фильтры спутников: цветной чекбокс и набор фильтров по созвездиям."""
from PyQt6.QtCore import Qt, QRectF, QPointF, QSize, pyqtSignal
from PyQt6.QtGui import QPainter, QPen, QColor, QPolygonF
from PyQt6.QtWidgets import QCheckBox, QWidget, QVBoxLayout, QHBoxLayout, QLabel

from ucenter.core.satellites import SYSTEM_NAMES
from ucenter.ui.style import role_label
from ucenter.ui.theme import SYSTEM_COLORS, LOST_COLOR

BOX = 16


class ColorCheckBox(QCheckBox):
    """Чекбокс с индикатором в цвете системы (как в u-center)."""

    def __init__(self, text: str, color: QColor, parent=None):
        super().__init__(text, parent)
        self._color = QColor(color)

    def sizeHint(self):
        w = self.fontMetrics().horizontalAdvance(self.text()) + BOX + 12
        return QSize(w, max(BOX + 6, self.fontMetrics().height() + 4))

    def paintEvent(self, _):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        box = QRectF(1, (self.height() - BOX) / 2, BOX, BOX)
        if self.isChecked():
            p.setPen(Qt.PenStyle.NoPen)
            p.setBrush(self._color)
            p.drawRoundedRect(box, 3, 3)
            p.setPen(QPen(QColor("#222"), 2))
            p.setBrush(Qt.BrushStyle.NoBrush)
            x, y = box.x(), box.y()
            p.drawPolyline(QPolygonF([QPointF(x + 3.5, y + 8.5), QPointF(x + 6.5, y + 11.5),
                                      QPointF(x + 12.5, y + 4.5)]))
        else:
            p.setPen(QPen(self.palette().mid().color(), 1.5))
            p.setBrush(Qt.BrushStyle.NoBrush)
            p.drawRoundedRect(box, 3, 3)
        p.setPen(self.palette().text().color())
        p.drawText(QRectF(BOX + 8, 0, self.width() - BOX - 8, self.height()),
                   Qt.AlignmentFlag.AlignVCenter | Qt.AlignmentFlag.AlignLeft, self.text())


class ConstellationFilters(QWidget):
    """Флажок 'Show not tracked' и флажки созвездий; счётчики 'в решении/видно' по желанию."""

    changed = pyqtSignal()

    def __init__(self, vertical: bool = True, show_counts: bool = True, parent=None):
        super().__init__(parent)
        self.show_lost_cb = ColorCheckBox("Show not tracked", LOST_COLOR)
        self.show_lost_cb.setChecked(True)
        self.show_lost_cb.toggled.connect(self.changed)
        self.sys_cbs: dict[str, ColorCheckBox] = {}
        self.sys_counts: dict[str, QLabel] = {}

        if vertical:
            lay = QVBoxLayout(self)
            lay.addWidget(role_label("Filter satellites", "section"))
            lay.addWidget(self.show_lost_cb)
            lay.addSpacing(8)
            lay.addWidget(role_label("GNSS constellation", "section"))
        else:
            lay = QHBoxLayout(self)
            lay.addWidget(self.show_lost_cb)

        for ch, name in SYSTEM_NAMES.items():
            cb = ColorCheckBox(name, SYSTEM_COLORS[ch])
            cb.setChecked(True)
            cb.toggled.connect(self.changed)
            self.sys_cbs[ch] = cb
            if show_counts:
                row = QHBoxLayout()
                cnt = QLabel("0/0")
                row.addWidget(cb, 1)
                row.addWidget(cnt)
                lay.addLayout(row)
                self.sys_counts[ch] = cnt
            else:
                lay.addWidget(cb)
        lay.addStretch()

    def accepts(self, sat) -> bool:
        return self.sys_cbs[sat.sys].isChecked() and (
            bool(sat.cno) or self.show_lost_cb.isChecked())

    def set_counts(self, counts: dict[str, tuple[int, int]]):
        """counts: {система: (в решении, видно)}."""
        for ch, lbl in self.sys_counts.items():
            used, visible = counts.get(ch, (0, 0))
            lbl.setText(f"{used}/{visible}")
