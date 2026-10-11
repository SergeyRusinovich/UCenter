// Фильтры спутников: цветной чекбокс и набор фильтров по созвездиям.
#pragma once

#include "core/satellites.h"

#include <QCheckBox>
#include <QHash>
#include <QWidget>

class QLabel;

// Чекбокс с индикатором в цвете системы (как в u-center)
class ColorCheckBox : public QCheckBox
{
    Q_OBJECT

public:
    ColorCheckBox(const QString &text, const QColor &color, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QColor m_color;
};

// Флажок «Show not tracked» и флажки созвездий; счётчики «в решении/видно» по желанию
class ConstellationFilters : public QWidget
{
    Q_OBJECT

public:
    explicit ConstellationFilters(bool vertical = true, bool showCounts = true, QWidget *parent = nullptr);

    bool accepts(const Satellite &sat) const;
    // {система: (в решении, видно)}
    void setCounts(const QHash<char, std::pair<int, int>> &counts);

signals:
    void changed();

private:
    ColorCheckBox *m_showLostCb;
    QHash<char, ColorCheckBox *> m_sysCbs;
    QHash<char, QLabel *> m_sysCounts;
};
