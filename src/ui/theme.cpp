#include "ui/theme.h"

#include <QHash>

namespace theme {

QColor systemColor(char sys)
{
    static const QHash<char, QColor> colors = {
        {'G', QColor("#f5a742")}, {'S', QColor("#9aa0a6")}, {'E', QColor("#f2e82e")},
        {'B', QColor("#a4d65e")}, {'R', QColor("#6bb6e3")}, {'Q', QColor("#4db6ac")},
    };
    return colors.value(sys, QColor("#9aa0a6"));
}

QColor notUsedColor()
{
    return QColor("#1e3cff");
}

QColor lostColor()
{
    return QColor("#e02020");
}

}  // namespace theme
