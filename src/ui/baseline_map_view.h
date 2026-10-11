// Карта для измерения A->B: маркеры точек и флажок найденного места (перетаскиваются мышью),
// линия между точками, подсветка найденного места, состояние геопозиции на «прицеле».
#pragma once

#include "core/host_location.h"
#include "core/places.h"
#include "core/points.h"
#include "ui/map_view.h"

#include <array>
#include <optional>

class ExpandButton;
class LocationPopover;

class BaselineMapView : public MapView
{
    Q_OBJECT

public:
    explicit BaselineMapView(QWidget *parent = nullptr);

    // ---------- Развёрнутый вид ----------
    bool isExpanded() const;
    // Программная смена вида; expandToggled не испускается (его шлёт только кнопка)
    void setExpanded(bool on);

    // Найденное место: подсветка контура и флажок (nullopt — убрать)
    void setPlace(const std::optional<Place> &place);
    // Состояние геопозиции на «прицеле»: точка-индикатор и подсказка (Ok — всё в порядке)
    void setLocationAlert(LocationLevel level, const QString &text = QString());
    // Карточка у «прицела»: почему нет геопозиции и что сделать
    void showLocationHelp(LocationLevel level, const QString &text);
    // Радиус неопределённости позиции (круг вокруг неё)
    void setAccuracy(std::optional<double> meters) { m_accuracy = meters; }
    void setPoint(PointKey key, std::optional<LatLon> ll);

signals:
    void pointMoved(PointKey key, double lat, double lon);
    void placeMoved(double lat, double lon);  // новое место флажка
    void expandToggled(bool on);              // карта во всё окно / обычный вид (кнопкой)
    void locationRetryRequested();            // «Retry» в карточке о геопозиции

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintOverlay(QPainter &p) override;

private:
    static QString locateTip();

    enum class Handle { A, B, Flag };  // что можно захватить мышью

    std::optional<QPointF> anchor(Handle h) const;  // экранная точка привязки маркера или основания флажка
    std::optional<Handle> hit(const QPointF &pos) const;
    std::optional<LatLon> &point(PointKey key) { return m_points[key == PointKey::A ? 0 : 1]; }
    std::optional<LatLon> point(PointKey key) const { return m_points[key == PointKey::A ? 0 : 1]; }

    void paintPlaceArea(QPainter &p, const Place &place);
    void paintFlag(QPainter &p, const Place &place);

    std::array<std::optional<LatLon>, 2> m_points;
    std::optional<Handle> m_grab;
    QPointF m_grabOffset;  // от курсора до точки привязки: объект не прыгает под курсор
    std::optional<double> m_accuracy;
    std::optional<Place> m_place;
    ExpandButton *m_btnExpand;
    LocationPopover *m_popover = nullptr;
};
