// Источник сообщений приёмника: последовательный порт (GnssWorker) или запись (ReplayWorker).
// Живёт в потоке чтения (moveToThread); все слоты вызываются в нём, наружу — только сигналы.
#pragma once

#include "core/messages.h"

#include <QList>
#include <QObject>

class ReceiverSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

public slots:
    // Открывает источник; при ошибке сообщает error() и finished()
    virtual void start() = 0;
    // Закрывает источник (повторные вызовы безопасны)
    virtual void stop() = 0;
    // Отправка байт в приёмник; ошибка — sendFailed()
    virtual void send(const QByteArray &data, const QString &what) = 0;
    // Запись сырого потока в файл (только у порта)
    virtual void startRecording(const QString &path);
    virtual void stopRecording();

signals:
    void messages(const QList<GnssMessage> &batch);  // все кадры, собранные за одно чтение
    void error(const QString &text);                 // ошибка соединения: порт занят, отключён и т.д.
    void connected();                                // источник открыт
    void finished();                                 // источник закрылся сам: поток можно останавливать
    void sendFailed(const QString &what, const QString &error);
    void recordingChanged(bool active, const QString &error);  // error — причина остановки записи
};
