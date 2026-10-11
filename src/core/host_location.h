// Геопозиция компьютера, на котором запущена программа (службы геолокации ОС, QtPositioning).
//
// macOS выдаёт геопозицию только приложению с NSLocationUsageDescription в Info.plist:
// CMake собирает UCenter.app с этим ключом (cmake/Info.plist.in).
#pragma once

#include <QGeoPositionInfoSource>
#include <QObject>

#include <optional>

enum class LocationLevel { Ok, Wait, Error };

class HostLocation : public QObject
{
    Q_OBJECT

public:
    static constexpr int UpdateMs = 1000;

    explicit HostLocation(QObject *parent = nullptr);

    // Запрашивает разрешение и запускает обновления (повторные вызовы игнорируются)
    void start();
    // Повторная попытка — например, после того как доступ разрешили в настройках
    void retry();

signals:
    void position(double lat, double lon, std::optional<double> accuracy);  // точность, м
    void status(LocationLevel level, const QString &text);                  // текст для пользователя

private:
    void onPermission(Qt::PermissionStatus permission);
    void onUpdate(const QGeoPositionInfo &info);
    void onError(QGeoPositionInfoSource::Error error);

    QGeoPositionInfoSource *m_source = nullptr;
    bool m_started = false;
};
