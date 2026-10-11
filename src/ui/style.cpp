#include "ui/style.h"

#include <QApplication>
#include <QHash>
#include <QLabel>
#include <QPalette>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleHints>

#include <algorithm>

namespace style {

namespace {

const QColor FallbackAccent(0x0A, 0x6C, 0xFF);

struct Colors {
    QString window;  // фон окна
    QString card;    // фон карточек (QGroupBox), полей ввода
    QString border;
    QString hover;
    QString text;
    QString muted;  // второстепенный текст
    QString console;
    QString success;
    QString danger;
    QString chevron;  // файл стрелки выпадающего списка
};

const Colors Light{"#F3F4F6", "#FFFFFF", "#DDE1E6", "#EEF0F3", "#1F2328",
                   "#6B7280", "#FAFBFC", "#1FA35A", "#D93F3F", "chevron-light.svg"};
const Colors Dark{"#1C1D20", "#26282C", "#3A3D43", "#30333A", "#E6E7EA",
                  "#9AA0A8", "#1A1B1E", "#3CC97A", "#EF6464", "chevron-dark.svg"};

const char *const Qss = R"(
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
QPushButton[variant="primary"]:disabled { background: $window; border-color: $border; color: $muted; font-weight: normal; }
QPushButton[variant="danger"] { background: $card; border-color: $danger; color: $danger; font-weight: 600; }
QPushButton[variant="danger"]:hover { background: $danger_soft; }
QPushButton[variant="danger"]:disabled { background: $window; border-color: $border; color: $muted; font-weight: normal; }
QPushButton#mapButton {
    padding: 0; min-height: 0; border-radius: 14px; font-size: 15px; font-weight: 600;
}

/* Плашка поверх карты */
QFrame#mapOverlay { background: $card; border: 1px solid $border; border-radius: 8px; }
QFrame#mapOverlay QLabel { background: transparent; }
QFrame#popover { background: $card; border: 1px solid $border; border-radius: 10px; }
QFrame#popover QLabel { background: transparent; }

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

/* Таблицы */
QTableView {
    background: $card; border: 1px solid $border; border-radius: 6px; gridline-color: $border;
    selection-background-color: $accent_soft; selection-color: $text;
}
QHeaderView::section {
    background: $window; color: $muted; border: none; border-bottom: 1px solid $border;
    padding: 4px 6px;
}
QTableCornerButton::section { background: $window; border: none; }

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
)";

QString rgba(const QColor &color, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

// Акцентный цвет ОС; слишком светлый/тёмный/серый заменяем, чтобы белый текст читался
QColor systemAccent(const QApplication &app)
{
    const QColor accent = app.palette().color(QPalette::Accent);
    if (!accent.isValid() || accent.hslSaturation() < 40)
        return FallbackAccent;
    int h = 0, s = 0, lightness = 0;
    accent.getHsl(&h, &s, &lightness);
    return QColor::fromHsl(h, s, std::clamp(lightness, 90, 150));
}

// Палитра для того, что рисуется без QSS: свои виджеты и элементы стиля Fusion
QPalette palette(const Colors &c, const QColor &accent)
{
    QPalette p;
    const QList<std::pair<QPalette::ColorRole, QString>> roles = {
        {QPalette::Window, c.window},
        {QPalette::WindowText, c.text},
        {QPalette::Base, c.card},
        {QPalette::AlternateBase, c.hover},
        {QPalette::Text, c.text},
        {QPalette::Button, c.card},
        {QPalette::ButtonText, c.text},
        {QPalette::ToolTipBase, c.card},
        {QPalette::ToolTipText, c.text},
        {QPalette::PlaceholderText, c.muted},
        {QPalette::Mid, c.muted},
        {QPalette::Midlight, c.border},
        {QPalette::Light, c.card},
        {QPalette::Dark, c.border},
        {QPalette::HighlightedText, "#FFFFFF"},
    };
    for (const auto &[role, color] : roles)
        p.setColor(role, QColor(color));
    for (QPalette::ColorRole role : {QPalette::Highlight, QPalette::Accent, QPalette::Link})
        p.setColor(role, accent);
    for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor(c.muted));
    return p;
}

QString stylesheet(const Colors &c, const QColor &accent)
{
    const QHash<QString, QString> vars = {
        {"window", c.window},
        {"card", c.card},
        {"border", c.border},
        {"hover", c.hover},
        {"text", c.text},
        {"muted", c.muted},
        {"console", c.console},
        {"success", c.success},
        {"danger", c.danger},
        {"chevron", c.chevron},
        {"accent", accent.name()},
        {"accent_hover", accent.lighter(112).name()},
        {"accent_soft", rgba(accent, 50)},
        {"danger_soft", rgba(QColor(c.danger), 30)},
        {"scroll", rgba(QColor(c.muted), 110)},
        {"assets", ":/assets"},
    };
    static const QRegularExpression placeholder(QStringLiteral("\\$(\\w+)"));
    const QString qss = QString::fromUtf8(Qss);
    QString result;
    qsizetype last = 0;
    for (const QRegularExpressionMatch &m : placeholder.globalMatch(qss)) {
        result += QStringView(qss).mid(last, m.capturedStart() - last);
        result += vars.value(m.captured(1), m.captured(0));
        last = m.capturedEnd();
    }
    result += QStringView(qss).mid(last);
    return result;
}

}  // namespace

void applyTheme(QApplication &app)
{
    const QColor accent = systemAccent(app);
    auto apply = [&app, accent] {
        const Colors &colors = app.styleHints()->colorScheme() == Qt::ColorScheme::Dark ? Dark : Light;
        app.setPalette(palette(colors, accent));
        app.setStyleSheet(stylesheet(colors, accent));
    };
    apply();
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, &app, apply);
}

QLabel *roleLabel(const QString &text, const char *role)
{
    auto *label = new QLabel(text);
    label->setProperty("role", QString::fromLatin1(role));
    return label;
}

void setStyleProperty(QWidget *widget, const char *name, const QVariant &value)
{
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

}  // namespace style
