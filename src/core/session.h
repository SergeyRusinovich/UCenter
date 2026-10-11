// Сеанс работы с приёмником: жизненный цикл источника (порт или запись) и транзакции конфигурации.
#pragma once

#include "core/messages.h"
#include "core/receiver_config.h"
#include "core/ubx.h"

#include <QObject>
#include <QTimer>

#include <deque>

class QThread;
class ReceiverSource;

// Единственная точка общения с приёмником: открывает/закрывает порт или запись,
// отправляет команды конфигурации и разбирает ответы на них.
class ReceiverSession : public QObject
{
    Q_OBJECT

public:
    static constexpr double AckTimeout = 2.0;  // секунд ожидания ACK на команду конфигурации
    static constexpr int StopTimeoutMs = 3000;

    explicit ReceiverSession(QObject *parent = nullptr);
    ~ReceiverSession() override;

    bool isOpen() const { return m_source != nullptr; }
    bool isReplay() const { return m_replay; }

    void open(const QString &port, int baudrate);
    // Воспроизведение записи; speed — во сколько раз быстрее реального времени
    void openReplay(const QString &path, double speed = 1.0);
    void close();

    // Отправляет CFG-VALSET и ждёт подтверждения; flash — записать и во Flash
    void applyConfig(const ubx::ConfigItems &items, bool flash = false);
    // Возвращает заводскую конфигурацию (CFG-CFG) и перечитывает значения
    void factoryReset();
    // Запрашивает текущие значения ключей конфигурации (CFG-VALGET)
    void pollConfig();

    // Запись сырого потока порта в файл
    void startRecording(const QString &path);
    void stopRecording();

signals:
    void connectedChanged(bool connected);
    void message(const GnssMessage &msg);
    void configValues(const ubx::ConfigValues &values);  // текущие значения ключей из CFG-VALGET
    void status(const QString &text);                    // краткие сообщения для строки статуса
    void error(const QString &text);                     // ошибки соединения
    void recordingChanged(bool active);

private:
    struct Pending {
        receiver_config::Command command;
        double sentAt = 0.0;
    };

    void startSource(ReceiverSource *source, bool replay);
    void onConnected();
    void onWorkerFinished();
    void onMessages(const QList<GnssMessage> &batch);
    void onSendFailed(const QString &what, const QString &err);
    void onRecordingChanged(bool active, const QString &err);
    void onCommandAck(receiver_config::Command command, bool ok);
    void checkAckTimeout();
    bool send(const QByteArray &data, const QString &what);
    void sendCommand(receiver_config::Command command, const QByteArray &data);
    void stopWorker();
    void reset();

    QThread *m_thread = nullptr;
    ReceiverSource *m_source = nullptr;
    bool m_replay = false;
    bool m_recording = false;
    std::deque<Pending> m_pending;  // команды, ждущие ACK, в порядке отправки
    QTimer m_ackTimer;
};
