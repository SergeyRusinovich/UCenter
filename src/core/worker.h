// Чтение GNSS-приёмника: порт + разбор кадров в отдельном потоке, наружу только Qt-сигналы.
#pragma once

#include "core/frame_reader.h"
#include "core/source.h"

#include <QSerialPort>
#include <QTimer>

class QFile;

class GnssWorker : public ReceiverSource
{
    Q_OBJECT

public:
    static constexpr int WriteTimeoutMs = 2000;  // столько ждём, пока приёмник заберёт отправленное

    GnssWorker(const QString &port, int baudrate);

    void start() override;
    void stop() override;
    void send(const QByteArray &data, const QString &what) override;
    void startRecording(const QString &path) override;
    void stopRecording() override;

private:
    void onReadyRead();
    void onBytesWritten();
    void onWriteTimeout();
    void onError(QSerialPort::SerialPortError err);
    void closeRecording(const QString &error);

    QString m_port;
    int m_baudrate;
    QSerialPort *m_serial = nullptr;
    FrameReader m_reader;
    QTimer m_writeTimer;
    QString m_writing;  // что отправляется сейчас (для сообщения о таймауте)
    QFile *m_record = nullptr;
};
