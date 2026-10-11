// Общее для сетевых запросов к картографическим сервисам.
#pragma once

#include <QNetworkRequest>
#include <QUrl>

namespace network {

// Требуется политиками OSM и Nominatim
inline constexpr char UserAgent[] = "UCenter/1.0 (desktop GNSS viewer)";

QNetworkRequest makeRequest(const QUrl &url);

}  // namespace network
