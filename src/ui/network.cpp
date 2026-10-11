#include "ui/network.h"

namespace network {

QNetworkRequest makeRequest(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(UserAgent));
    return req;
}

}  // namespace network
