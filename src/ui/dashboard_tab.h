// Вкладка Dashboard: данные навигации, поправка UTC, компас и карта.
#pragma once

#include "core/navigation.h"

#include <QList>
#include <QWidget>

#include <optional>

class CompassView;
class MapView;
class QComboBox;
class QGroupBox;
class QLabel;

class DashboardTab : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardTab(QWidget *parent = nullptr);
    void updateState(const NavData &data);

private:
    QGroupBox *buildValues();
    QGroupBox *buildTimeCorrection();
    void updateLocalTime();

    QList<QLabel *> m_labels;  // по одной на строку FIELDS
    QComboBox *m_offsetCombo = nullptr;
    QLabel *m_localLabel = nullptr;
    CompassView *m_compass;
    MapView *m_map;
    std::optional<int> m_utcSeconds;
};
