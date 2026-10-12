// Выбор подложки карты (спутник Esri, OSM или — только с явного согласия — спутник Google, ADR 0022);
// выбор запоминается между запусками.
#pragma once

#include "ui/tile_loader.h"

#include <QComboBox>

class ImageryPicker : public QComboBox
{
    Q_OBJECT

public:
    explicit ImageryPicker(QWidget *parent = nullptr);
    const TileSource &source() const;

signals:
    void sourceChanged(const TileSource &source);

private:
    void onChanged(int index);
    // Условия Google разрешают его снимки только для личного использования: спросить один раз
    bool confirmGoogle();

    int m_previous = 0;  // куда вернуться, если от Google отказались
};
