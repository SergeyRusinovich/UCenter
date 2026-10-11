// Оформление приложения: светлая/тёмная палитра по системной теме, акцент ОС и QSS.
#pragma once

#include <QString>
#include <QVariant>

class QApplication;
class QLabel;
class QWidget;

namespace style {

// Применяет тему и переключает её вслед за системной (светлая/тёмная)
void applyTheme(QApplication &app);

// QLabel с текстовой ролью из QSS: value, caption или section
QLabel *roleLabel(const QString &text, const char *role);

// Меняет свойство, от которого зависит QSS, и сразу перерисовывает виджет
void setStyleProperty(QWidget *widget, const char *name, const QVariant &value);

}  // namespace style
