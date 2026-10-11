// Строка поиска мест: запрос по Enter, выбор из результатов, «→ A» / «→ B», строка состояния.
//
// С картой не связана напрямую: видимую область получает через переданную функцию,
// а о выбранном месте и запросе поставить точку сообщает сигналами.
#pragma once

#include "core/places.h"
#include "core/points.h"

#include <QList>
#include <QWidget>

#include <functional>
#include <optional>

class PlaceSearch;
class QLabel;
class QLineEdit;
class QPushButton;

class PlaceSearchBar : public QWidget
{
    Q_OBJECT

public:
    explicit PlaceSearchBar(std::function<Bounds()> viewBounds, QWidget *parent = nullptr);

    // Флажок места перетащили на карте
    void movePlace(double lat, double lon);

signals:
    void placeChanged(const std::optional<Place> &place);       // выбранное место или nullopt — убрать
    void pointRequested(PointKey key, double lat, double lon);  // поставить точку на место флажка

private:
    void setStatus(const QString &text);
    void startSearch();
    void onResults(const QString &text, const QList<Place> &found);
    QString menuText(const Place &place) const;
    void select(const std::optional<Place> &place);
    void requestPoint(PointKey key);

    std::function<Bounds()> m_viewBounds;
    std::optional<Place> m_place;
    PlaceSearch *m_search;
    QLineEdit *m_edit;
    QList<QPushButton *> m_pointButtons;
    QLabel *m_status;  // найденное место или ошибка
};
