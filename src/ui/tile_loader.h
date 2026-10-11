// Загрузка тайлов карты: сеть, кэш в памяти и на диске, повторы с паузой, отмена ненужных запросов.
#pragma once

#include <QCache>
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

struct TileSource {
    QString url;  // шаблон с {z}, {x}, {y}
    QString attribution;
    QString cacheDir;  // подкаталог дискового кэша
    int maxZoom = 19;
    // Тайлы следующего уровня для экранов Retina: подходит снимкам, а у схемы мельчат подписи
    bool hiDpi = false;

    friend bool operator==(const TileSource &a, const TileSource &b) { return a.cacheDir == b.cacheDir; }
};

namespace tiles {

const TileSource &osm();
const TileSource &esriImagery();
// Запасной вариант там, где Esri недоступен. Прямой доступ к тайлам Google не разрешён
// условиями Google Maps Platform: только для личного/внутреннего использования.
const TileSource &googleImagery();

QString key(int z, int x, int y);

}  // namespace tiles

// Отдаёт тайл из кэша или запрашивает его; tileLoaded — когда пришёл новый
class TileLoader : public QObject
{
    Q_OBJECT

public:
    static constexpr int MemoryTiles = 600;
    static constexpr int MaxPending = 64;
    static constexpr int MaxFailed = 2000;
    static constexpr qint64 DiskCacheBytes = 200 * 1024 * 1024;
    static constexpr double RetryMin = 10.0, RetryMax = 300.0;  // секунд до повтора неудачного тайла (растёт вдвое)

    explicit TileLoader(const TileSource &source, QObject *parent = nullptr);

    const TileSource &source() const { return m_source; }
    // Тайл из кэша; если его нет — запрос и пустой QPixmap
    QPixmap tile(int z, int x, int y);
    // Только из кэша, без запроса (подложка, пока грузится нужный тайл)
    QPixmap cached(int z, int x, int y) const;
    // Отменяет запросы тайлов, которых нет в keys (ушли с экрана)
    void retain(const QSet<QString> &keys);

signals:
    void tileLoaded();

private:
    struct Retry {
        double at = 0.0;     // монотонное время повтора
        double delay = 0.0;  // текущая пауза
    };

    void request(const QString &key, int z, int x, int y);
    void onReply(QNetworkReply *reply);
    void pruneFailed();

    TileSource m_source;
    QCache<QString, QPixmap> m_cache;
    QHash<QString, QNetworkReply *> m_pending;
    QHash<QString, Retry> m_failed;
    QNetworkAccessManager *m_nam;
};
