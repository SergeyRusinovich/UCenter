// Цвета для отображения спутников.
#pragma once

#include <QColor>

namespace theme {

QColor systemColor(char sys);
QColor notUsedColor();  // обводка: отслеживается, но не используется в решении
QColor lostColor();     // обводка: спутник не отслеживается

}  // namespace theme
