// Поиск мест через Nominatim (OpenStreetMap) с ответами на русском.
//
// Политика Nominatim: не чаще 1 запроса в секунду, свой User-Agent, без автодополнения —
// поэтому поиск запускается только по Enter, а слишком частый запрос откладывается.
#pragma once

#include "core/places.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <optional>

class QNetworkAccessManager;
class QNetworkReply;

class PlaceSearch : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxResults = 8;
    static constexpr double MinInterval = 1.0;  // секунд между запросами

    explicit PlaceSearch(QObject *parent = nullptr);
    // Указание источника данных, которого требует лицензия OSM
    static QString attribution();
    // viewbox — видимая область: близкие места выше в списке
    void search(const QString &text, std::optional<Bounds> viewbox = std::nullopt);

signals:
    void results(const QString &query, const QList<Place> &places);
    void failed(const QString &text);  // текст ошибки

private:
    void send();
    void onReply(QNetworkReply *reply, const QString &text);

    QNetworkAccessManager *m_nam;
    QPointer<QNetworkReply> m_reply;
    double m_last = -MinInterval;
    QTimer m_timer;
    std::optional<std::pair<QString, std::optional<Bounds>>> m_query;
};
