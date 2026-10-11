// Выбор подложки карты (спутник Esri/Google или OSM); выбор запоминается между запусками.
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
};
