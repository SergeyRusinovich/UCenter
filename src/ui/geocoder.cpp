#include "ui/geocoder.h"

#include "core/timeutil.h"
#include "ui/network.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>

#include <algorithm>

namespace {

const char *const SearchUrl = "https://nominatim.openstreetmap.org/search";
const char *const Language = "ru";

}  // namespace

PlaceSearch::PlaceSearch(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &PlaceSearch::send);
}

QString PlaceSearch::attribution()
{
    return tr("Search: © OpenStreetMap contributors (Nominatim)");
}

void PlaceSearch::search(const QString &text, std::optional<Bounds> viewbox)
{
    const QString query = text.trimmed();
    if (query.isEmpty())
        return;
    m_query = {query, viewbox};
    const double wait = MinInterval - (timeutil::monotonic() - m_last);
    m_timer.start(std::max(0, int(wait * 1000)));
}

void PlaceSearch::send()
{
    if (!m_query)
        return;
    const auto [text, viewbox] = *m_query;
    if (m_reply) {  // новый запрос заменяет незавершённый
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("q"), text);
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("jsonv2"));
    q.addQueryItem(QStringLiteral("limit"), QString::number(MaxResults));
    q.addQueryItem(QStringLiteral("polygon_geojson"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("polygon_threshold"), QStringLiteral("0.00005"));
    q.addQueryItem(QStringLiteral("accept-language"), QLatin1String(Language));
    if (viewbox) {
        q.addQueryItem(QStringLiteral("viewbox"), QStringLiteral("%1,%2,%3,%4")
                                                      .arg(viewbox->west, 0, 'g', 10)
                                                      .arg(viewbox->north, 0, 'g', 10)
                                                      .arg(viewbox->east, 0, 'g', 10)
                                                      .arg(viewbox->south, 0, 'g', 10));
    }
    QUrl url(QString::fromLatin1(SearchUrl));
    url.setQuery(q);
    QNetworkRequest req = network::makeRequest(url);
    req.setRawHeader("Accept-Language", Language);
    m_last = timeutil::monotonic();
    QNetworkReply *reply = m_nam->get(req);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, text] { onReply(reply, text); });
}

void PlaceSearch::onReply(QNetworkReply *reply, const QString &text)
{
    if (reply == m_reply)
        m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit failed(tr("Search error: %1").arg(reply->errorString()));
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        emit failed(tr("Search error: unexpected server response"));
        return;
    }
    emit results(text, places::mergeDuplicates(places::parseNominatim(doc.array())));
}
