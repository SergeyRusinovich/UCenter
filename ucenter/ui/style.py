"""Оформление приложения: светлая/тёмная палитра по системной теме, акцент ОС и QSS."""
from dataclasses import dataclass
from pathlib import Path
from string import Template

from PyQt6.QtCore import Qt
from PyQt6.QtGui import QColor, QPalette
from PyQt6.QtWidgets import QApplication, QLabel, QWidget

ASSETS = Path(__file__).with_name("assets")
FALLBACK_ACCENT = QColor("#0A6CFF")


@dataclass(frozen=True)
class Colors:
    window: str    # фон окна
    card: str      # фон карточек (QGroupBox), полей ввода
    border: str
    hover: str
    text: str
    muted: str     # второстепенный текст
    console: str
    success: str
    danger: str
    chevron: str   # файл стрелки выпадающего списка


LIGHT = Colors(window="#F3F4F6", card="#FFFFFF", border="#DDE1E6", hover="#EEF0F3",
               text="#1F2328", muted="#6B7280", console="#FAFBFC",
               success="#1FA35A", danger="#D93F3F", chevron="chevron-light.svg")
DARK = Colors(window="#1C1D20", card="#26282C", border="#3A3D43", hover="#30333A",
              text="#E6E7EA", muted="#9AA0A8", console="#1A1B1E",
              success="#3CC97A", danger="#EF6464", chevron="chevron-dark.svg")

QSS = Template("""
QWidget { color: $text; }
QMainWindow, QDialog { background: $window; }
QToolTip { background: $card; color: $text; border: 1px solid $border; padding: 4px 6px; }

/* Карточки */
QGroupBox {
    background: $card; border: 1px solid $border; border-radius: 10px;
    margin-top: 0; padding: 26px 6px 6px 6px; font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: padding; subcontrol-position: top left;
    left: 12px; top: 8px; color: $muted;
}
QGroupBox QWidget { font-weight: normal; }
QGroupBox > QWidget, QGroupBox QLabel, QGroupBox QCheckBox { background: transparent; }

/* Вкладки: «таблетки» на фоне окна */
QTabWidget::pane { border: none; top: 6px; }
QTabWidget::tab-bar { alignment: left; }
QTabBar::tab {
    background: transparent; color: $muted; border: 1px solid transparent;
    border-radius: 7px; padding: 6px 16px; margin-right: 4px;
}
QTabBar::tab:hover { background: $hover; color: $text; }
QTabBar::tab:selected { background: $card; color: $text; border-color: $border; }

/* Кнопки */
QPushButton {
    background: $card; border: 1px solid $border; border-radius: 6px;
    padding: 5px 14px; min-height: 18px;
}
QPushButton:hover { background: $hover; }
QPushButton:pressed { background: $border; }
QPushButton:disabled { color: $muted; background: $window; }
QPushButton[variant="primary"] { background: $accent; border-color: $accent; color: #FFFFFF; font-weight: 600; }
QPushButton[variant="primary"]:hover { background: $accent_hover; border-color: $accent_hover; }
QPushButton[variant="danger"] { background: $card; border-color: $danger; color: $danger; font-weight: 600; }
QPushButton[variant="danger"]:hover { background: $danger_soft; }
QPushButton#mapButton {
    padding: 0; min-height: 0; border-radius: 14px; font-size: 15px; font-weight: 600;
}

/* Поля ввода и списки */
QComboBox, QLineEdit {
    background: $card; border: 1px solid $border; border-radius: 6px;
    padding: 4px 8px; min-height: 18px;
}
QComboBox { padding-right: 26px; }
QComboBox:hover, QLineEdit:hover { border-color: $muted; }
QComboBox:focus, QLineEdit:focus { border-color: $accent; }
QComboBox:disabled { color: $muted; background: $window; }
QComboBox QLineEdit { border: none; padding: 0; background: transparent; }
QComboBox::drop-down {
    subcontrol-origin: padding; subcontrol-position: center right;
    width: 24px; border: none;
}
QComboBox::down-arrow { image: url("$assets/$chevron"); width: 12px; height: 12px; }
QComboBox QAbstractItemView {
    background: $card; border: 1px solid $border; border-radius: 6px; padding: 4px;
    outline: 0; selection-background-color: $accent_soft; selection-color: $text;
}

/* Флажки */
QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px; border-radius: 4px;
    border: 1.5px solid $border; background: $card;
}
QCheckBox::indicator:hover { border-color: $accent; }
QCheckBox::indicator:checked { background: $accent; border-color: $accent; image: url("$assets/check.svg"); }
QCheckBox::indicator:disabled { background: $window; }

/* Ползунок */
QSlider::groove:horizontal { height: 4px; border-radius: 2px; background: $border; }
QSlider::sub-page:horizontal { border-radius: 2px; background: $accent; }
QSlider::handle:horizontal {
    width: 16px; height: 16px; margin: -6px 0; border-radius: 8px;
    background: #FFFFFF; border: 1px solid $border;
}

/* Консоль и полосы прокрутки */
QPlainTextEdit#console {
    background: $console; border: 1px solid $border; border-radius: 10px; padding: 6px;
    selection-background-color: $accent_soft; selection-color: $text;
}
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: $scroll; border-radius: 3px; min-height: 30px; }
QScrollBar::handle:horizontal { background: $scroll; border-radius: 3px; min-width: 30px; }
QScrollBar::handle:hover { background: $muted; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* Текстовые роли */
QLabel[role="value"] { font-size: 14px; font-weight: 600; }
QLabel[role="caption"] { color: $muted; }
QLabel[role="section"] { font-weight: 600; }

/* Индикатор подключения */
QLabel#statusLed { min-width: 10px; max-width: 10px; min-height: 10px; max-height: 10px; border-radius: 5px; }
QLabel#statusLed[state="on"] { background: $success; }
QLabel#statusLed[state="off"] { background: $danger; }

QStatusBar { background: $window; color: $muted; }
QStatusBar::item { border: none; }
""")


def _rgba(color: QColor, alpha: int) -> str:
    return f"rgba({color.red()}, {color.green()}, {color.blue()}, {alpha})"


def _system_accent(app: QApplication) -> QColor:
    """Акцентный цвет ОС; слишком светлый/тёмный/серый заменяем, чтобы белый текст читался."""
    accent = app.palette().color(QPalette.ColorRole.Accent)
    if not accent.isValid() or accent.hslSaturation() < 40:
        return QColor(FALLBACK_ACCENT)
    h, s, lightness, _ = accent.getHsl()
    return QColor.fromHsl(h, s, max(90, min(150, lightness)))


def _palette(c: Colors, accent: QColor) -> QPalette:
    """Палитра для того, что рисуется без QSS: свои виджеты и элементы стиля Fusion."""
    p = QPalette()
    roles = {
        QPalette.ColorRole.Window: c.window, QPalette.ColorRole.WindowText: c.text,
        QPalette.ColorRole.Base: c.card, QPalette.ColorRole.AlternateBase: c.hover,
        QPalette.ColorRole.Text: c.text, QPalette.ColorRole.Button: c.card,
        QPalette.ColorRole.ButtonText: c.text, QPalette.ColorRole.ToolTipBase: c.card,
        QPalette.ColorRole.ToolTipText: c.text, QPalette.ColorRole.PlaceholderText: c.muted,
        QPalette.ColorRole.Mid: c.muted, QPalette.ColorRole.Midlight: c.border,
        QPalette.ColorRole.Light: c.card, QPalette.ColorRole.Dark: c.border,
        QPalette.ColorRole.HighlightedText: "#FFFFFF",
    }
    for role, color in roles.items():
        p.setColor(role, QColor(color))
    for role in (QPalette.ColorRole.Highlight, QPalette.ColorRole.Accent, QPalette.ColorRole.Link):
        p.setColor(role, accent)
    for role in (QPalette.ColorRole.Text, QPalette.ColorRole.WindowText, QPalette.ColorRole.ButtonText):
        p.setColor(QPalette.ColorGroup.Disabled, role, QColor(c.muted))
    return p


def _stylesheet(c: Colors, accent: QColor) -> str:
    return QSS.substitute(
        **{k: getattr(c, k) for k in Colors.__dataclass_fields__},
        accent=accent.name(), accent_hover=accent.lighter(112).name(),
        accent_soft=_rgba(accent, 50), danger_soft=_rgba(QColor(c.danger), 30),
        scroll=_rgba(QColor(c.muted), 110), assets=ASSETS.as_posix(),
    )


def apply_theme(app: QApplication):
    """Применяет тему и переключает её вслед за системной (светлая/тёмная)."""
    accent = _system_accent(app)

    def apply():
        colors = DARK if app.styleHints().colorScheme() == Qt.ColorScheme.Dark else LIGHT
        app.setPalette(_palette(colors, accent))
        app.setStyleSheet(_stylesheet(colors, accent))

    apply()
    app.styleHints().colorSchemeChanged.connect(lambda _: apply())


def role_label(text: str, role: str) -> QLabel:
    """QLabel с текстовой ролью из QSS: value, caption или section."""
    label = QLabel(text)
    label.setProperty("role", role)
    return label


def set_style_property(widget: QWidget, name: str, value):
    """Меняет свойство, от которого зависит QSS, и сразу перерисовывает виджет."""
    widget.setProperty(name, value)
    widget.style().unpolish(widget)
    widget.style().polish(widget)
    widget.update()
