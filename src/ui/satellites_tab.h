// Вкладка спутников: небесная карта и диаграмма сигналов, у каждой свои фильтры.
#pragma once

#include "core/satellites.h"

#include <QWidget>

class ConstellationFilters;
class QLabel;
class QSlider;
class QVBoxLayout;
class SignalView;
class SkyView;

class SatellitesTab : public QWidget
{
    Q_OBJECT

public:
    explicit SatellitesTab(QWidget *parent = nullptr);
    void updateData(const SatelliteSnapshot &snapshot);

private:
    QVBoxLayout *buildZoom();
    void redraw();

    SatelliteSnapshot m_data;
    SkyView *m_sky;
    SignalView *m_signal;
    ConstellationFilters *m_skyFilters;
    ConstellationFilters *m_signalFilters;
    QSlider *m_zoom = nullptr;
    QLabel *m_zoomLabel = nullptr;
};
