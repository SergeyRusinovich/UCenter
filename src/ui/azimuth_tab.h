// Вкладка Azimuth: собирает панели измерения A->B, карту, поиск и таблицу и связывает их.
//
// Правила установки точек — в MeasurementPoints (core), каждая панель отвечает за своё;
// вкладка только передаёт события между ними. Точка A по умолчанию — геопозиция
// компьютера (службы геолокации ОС), а не GNSS-приёмника.
#pragma once

#include "core/host_location.h"
#include "core/places.h"
#include "core/points.h"

#include <QList>
#include <QWidget>

#include <optional>

class BaselineMapView;
class ImageryPicker;
class PlaceSearchBar;
class PointsEditor;
class QShortcut;
class QSplitter;
class ResultOverlay;
class ResultPanel;
class SavedMeasurementsPanel;

class AzimuthTab : public QWidget
{
    Q_OBJECT

public:
    static constexpr int LeftWidth = 340;

    explicit AzimuthTab(QWidget *parent = nullptr);

    void setExpanded(bool on);
    // Вид карты и делитель с таблицей — между запусками
    void saveSettings() const;

signals:
    void expandChanged(bool on);  // карта во всё окно: владелец прячет свои панели

protected:
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    void buildLayout();
    void restoreSettings();
    void wire();

    // ---------- Точки ----------
    // Переносит точки из модели в поля ввода и на карту, пересчитывает результат
    void sync(const QList<PointKey> &keys = {PointKey::A, PointKey::B}, bool fromDevice = false);
    // Ручная установка точки (кнопки «→ A/B», ввод координат)
    void placePoint(PointKey key, std::optional<LatLon> ll);
    void onMapClick(double lat, double lon);
    void onMarkerMoved(PointKey key, double lat, double lon);
    void onPointEdited(PointKey key, std::optional<LatLon> ll);
    void attachA();
    void newMeasurement();
    void refresh();
    QString hint() const;

    // ---------- Геопозиция компьютера ----------
    void onLocationStatus(LocationLevel level, const QString &text);
    void onDevicePosition(double lat, double lon, std::optional<double> accuracy);

    void showPlace(const std::optional<Place> &place);
    void save();
    void showSaved(const Baseline &m);
    void applyExpanded(bool on);

    MeasurementPoints m_points;
    HostLocation *m_location;
    LocationLevel m_locLevel = LocationLevel::Ok;  // последний статус геопозиции
    QString m_locText;

    PointsEditor *m_pointsEditor;
    ResultPanel *m_resultPanel;
    BaselineMapView *m_map;
    ResultOverlay *m_overlay;
    PlaceSearchBar *m_searchBar;
    ImageryPicker *m_imagery;
    SavedMeasurementsPanel *m_saved;
    QWidget *m_left = nullptr;
    QShortcut *m_esc = nullptr;
    QSplitter *m_splitter = nullptr;
};
