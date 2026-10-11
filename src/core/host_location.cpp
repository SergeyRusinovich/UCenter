#include "core/host_location.h"

#include <QCoreApplication>
#include <QMetaEnum>
#include <QPermissions>

namespace {

// Короткие тексты: показываются в подсказке и карточке у «прицела»
const char *const DeniedText = QT_TRANSLATE_NOOP("HostLocation", "Allow UCenter in Location Services.");

}  // namespace

HostLocation::HostLocation(QObject *parent)
    : QObject(parent)
{}

void HostLocation::start()
{
    if (m_started)
        return;
    m_started = true;
    QLocationPermission perm;
    perm.setAccuracy(QLocationPermission::Precise);
    if (qApp->checkPermission(perm) == Qt::PermissionStatus::Undetermined) {
        emit status(LocationLevel::Wait, tr("Requesting access…"));
        qApp->requestPermission(perm, this, [this](const QPermission &p) { onPermission(p.status()); });
    } else {
        onPermission(qApp->checkPermission(perm));
    }
}

void HostLocation::retry()
{
    if (m_source) {
        m_source->stopUpdates();
        m_source->deleteLater();
        m_source = nullptr;
    }
    m_started = false;
    start();
}

void HostLocation::onPermission(Qt::PermissionStatus permission)
{
    if (permission != Qt::PermissionStatus::Granted) {
        emit status(LocationLevel::Error, tr(DeniedText));
        return;
    }
    m_source = QGeoPositionInfoSource::createDefaultSource(this);
    if (!m_source) {
        emit status(LocationLevel::Error, tr("No location provider on this computer."));
        return;
    }
    m_source->setUpdateInterval(UpdateMs);
    connect(m_source, &QGeoPositionInfoSource::positionUpdated, this, &HostLocation::onUpdate);
    connect(m_source, &QGeoPositionInfoSource::errorOccurred, this, &HostLocation::onError);
    m_source->startUpdates();
    emit status(LocationLevel::Wait, tr("Waiting for location…"));
}

void HostLocation::onUpdate(const QGeoPositionInfo &info)
{
    const QGeoCoordinate c = info.coordinate();
    if (!c.isValid())
        return;
    std::optional<double> accuracy;
    if (info.hasAttribute(QGeoPositionInfo::HorizontalAccuracy))
        accuracy = info.attribute(QGeoPositionInfo::HorizontalAccuracy);
    emit position(c.latitude(), c.longitude(), accuracy);
    emit status(LocationLevel::Ok, QString());
}

void HostLocation::onError(QGeoPositionInfoSource::Error error)
{
    switch (error) {
    case QGeoPositionInfoSource::AccessError: emit status(LocationLevel::Error, tr(DeniedText)); break;
    case QGeoPositionInfoSource::ClosedError:
        emit status(LocationLevel::Error, tr("Location Services are off."));
        break;
    case QGeoPositionInfoSource::UpdateTimeoutError: emit status(LocationLevel::Wait, tr("No location yet…")); break;
    default:
        emit status(LocationLevel::Error,
                    tr("Location error: %1")
                        .arg(QLatin1String(QMetaEnum::fromType<QGeoPositionInfoSource::Error>().valueToKey(error))));
    }
}
