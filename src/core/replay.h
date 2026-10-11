// Воспроизведение записанного потока (.ubx: сырые байты порта, NMEA и UBX вперемешку)
// с исходным темпом эпох, как будто данные идут от приёмника.
#pragma once

#include "core/source.h"

#include <QTimer>

// Эпоха записи: сообщения [first, first + count) и пауза после них до следующей эпохи
struct ReplayEpoch {
    qsizetype first = 0;
    qsizetype count = 0;
    double delay = 0.0;  // секунд
};

// Делит сообщения на эпохи по времени решения: iTOW из NAV-PVT, если он есть в записи,
// иначе UTC из GGA/RMC. Без времени — пачки по ChunkSize сообщений через DefaultDelay.
QList<ReplayEpoch> replayEpochs(const QList<GnssMessage> &messages);

class ReplayWorker : public ReceiverSource
{
    Q_OBJECT

public:
    static constexpr int ChunkSize = 20;
    static constexpr double DefaultDelay = 0.1;  // секунд, когда время эпохи неизвестно
    static constexpr double MaxDelay = 2.0;      // разрывы в записи не ждём дольше

    // speed — во сколько раз быстрее реального времени
    explicit ReplayWorker(const QString &path, double speed = 1.0);

    void start() override;
    void stop() override;
    void send(const QByteArray &data, const QString &what) override;

private:
    void playNext();

    QString m_path;
    double m_speed;
    QList<GnssMessage> m_messages;
    QList<ReplayEpoch> m_epochs;
    qsizetype m_next = 0;
    QTimer m_timer;
};
