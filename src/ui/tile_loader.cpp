#include "ui/tile_loader.h"

#include "core/timeutil.h"
#include "ui/network.h"

#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QStandardPaths>

#include <algorithm>

namespace tiles {

const TileSource &osm()
{
    static const TileSource s{"https://tile.openstreetmap.org/{z}/{x}/{y}.png", "© OpenStreetMap contributors",
                              "osm-tiles", 19, false};
    return s;
}

const TileSource &esriImagery()
{
    static const TileSource s{
        "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
        "Imagery © Esri, Maxar, Earthstar Geographics", "esri-imagery", 19, true};
    return s;
}

const TileSource &googleImagery()
{
    static const TileSource s{"https://mt1.google.com/vt/lyrs=s&x={x}&y={y}&z={z}", "Imagery © Google",
                              "google-imagery", 20, true};
    return s;
}

QString key(int z, int x, int y)
{
    return QStringLiteral("%1/%2/%3").arg(z).arg(x).arg(y);
}

}  // namespace tiles

TileLoader::TileLoader(const TileSource &source, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_cache(MemoryTiles)
    , m_nam(new QNetworkAccessManager(this))
{
    // Дисковый кэш: тайлы переживают перезапуск, как требует политика OSM
    auto *disk = new QNetworkDiskCache(this);
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    disk->setCacheDirectory(QDir(base).filePath(source.cacheDir));
    disk->setMaximumCacheSize(DiskCacheBytes);
    m_nam->setCache(disk);
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    connect(m_nam, &QNetworkAccessManager::finished, this, &TileLoader::onReply);
}

QPixmap TileLoader::tile(int z, int x, int y)
{
    const QString key = tiles::key(z, x, y);
    if (const QPixmap *pm = m_cache.object(key))
        return *pm;
    const auto failed = m_failed.constFind(key);
    if (failed != m_failed.cend() && timeutil::monotonic() < failed->at)
        return {};  // ждём паузу, а не долбим сервер на каждой перерисовке
    if (!m_pending.contains(key) && m_pending.size() < MaxPending)
        request(key, z, x, y);
    return {};
}

QPixmap TileLoader::cached(int z, int x, int y) const
{
    const QPixmap *pm = m_cache.object(tiles::key(z, x, y));
    return pm ? *pm : QPixmap();
}

void TileLoader::retain(const QSet<QString> &keys)
{
    // Запросы ушедших с экрана тайлов занимают очередь и задерживают видимые
    const QList<QString> pending = m_pending.keys();
    for (const QString &key : pending)
        if (!keys.contains(key))
            m_pending.value(key)->abort();  // onReply уберёт его из m_pending
}

void TileLoader::request(const QString &key, int z, int x, int y)
{
    QString url = m_source.url;
    url.replace(QLatin1String("{z}"), QString::number(z))
        .replace(QLatin1String("{x}"), QString::number(x))
        .replace(QLatin1String("{y}"), QString::number(y));
    QNetworkRequest req = network::makeRequest(QUrl(url));
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    req.setAttribute(QNetworkRequest::User, key);
    m_pending.insert(key, m_nam->get(req));
}

void TileLoader::onReply(QNetworkReply *reply)
{
    reply->deleteLater();
    const QString key = reply->request().attribute(QNetworkRequest::User).toString();
    m_pending.remove(key);
    if (reply->error() == QNetworkReply::OperationCanceledError)
        return;  // отменён (ушёл с экрана) — не ошибка, при возврате запросим снова
    QPixmap pm;
    if (reply->error() == QNetworkReply::NoError && pm.loadFromData(reply->readAll())) {
        m_failed.remove(key);
        m_cache.insert(key, new QPixmap(pm));
        emit tileLoaded();
        return;
    }
    const auto it = m_failed.constFind(key);
    const double delay = it != m_failed.cend() ? std::min(RetryMax, it->delay * 2) : RetryMin;
    m_failed.insert(key, {timeutil::monotonic() + delay, delay});
    if (m_failed.size() > MaxFailed)
        pruneFailed();
}

void TileLoader::pruneFailed()
{
    // Тайлы, пауза которых истекла, при следующем показе просто запросятся снова
    const double now = timeutil::monotonic();
    m_failed.removeIf([now](const auto &it) { return it.value().at <= now; });
    if (m_failed.size() > MaxFailed)
        m_failed.clear();  // сеть недоступна долго: начинаем отсчёт пауз заново
}
