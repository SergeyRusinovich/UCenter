#include "core/session.h"

#include "core/replay.h"
#include "core/timeutil.h"
#include "core/worker.h"

#include <QThread>

#include <algorithm>

namespace cfg = receiver_config;

ReceiverSession::ReceiverSession(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<GnssMessage>();
    qRegisterMetaType<QList<GnssMessage>>();
    connect(&m_ackTimer, &QTimer::timeout, this, &ReceiverSession::checkAckTimeout);
}

ReceiverSession::~ReceiverSession()
{
    stopWorker();  // без сигналов: получатели уже могут быть удалены
}

// ---------- Соединение ----------
void ReceiverSession::open(const QString &port, int baudrate)
{
    if (!m_source)
        startSource(new GnssWorker(port, baudrate), false);
}

void ReceiverSession::openReplay(const QString &path, double speed)
{
    if (!m_source)
        startSource(new ReplayWorker(path, speed), true);
}

void ReceiverSession::startSource(ReceiverSource *source, bool replay)
{
    m_replay = replay;
    m_thread = new QThread(this);
    m_source = source;
    m_source->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_source, &ReceiverSource::start);
    connect(m_thread, &QThread::finished, m_source, &QObject::deleteLater);
    connect(m_source, &ReceiverSource::messages, this, &ReceiverSession::onMessages);
    connect(m_source, &ReceiverSource::error, this, &ReceiverSession::error);
    connect(m_source, &ReceiverSource::connected, this, &ReceiverSession::onConnected);
    connect(m_source, &ReceiverSource::finished, this, &ReceiverSession::onWorkerFinished);
    connect(m_source, &ReceiverSource::sendFailed, this, &ReceiverSession::onSendFailed);
    connect(m_source, &ReceiverSource::recordingChanged, this, &ReceiverSession::onRecordingChanged);
    m_thread->start();
}

void ReceiverSession::close()
{
    if (!m_source)
        return;
    stopWorker();
    reset();
}

void ReceiverSession::stopWorker()
{
    if (!m_source)
        return;
    disconnect(m_source, nullptr, this, nullptr);
    // Источник закрывается в своём потоке; объект удаляется по QThread::finished,
    // а при удалении закрывает порт, даже если stop() не успел выполниться
    QMetaObject::invokeMethod(m_source, &ReceiverSource::stop, Qt::QueuedConnection);
    m_thread->quit();
    if (m_thread->wait(StopTimeoutMs)) {
        delete m_thread;
    } else {
        // Поток завис (например, в драйвере порта): удалять его сейчас нельзя —
        // отцепляем, он удалит себя сам, когда завершится
        m_thread->setParent(nullptr);
        connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
        qWarning("UCenter: reader thread did not stop in %d ms", StopTimeoutMs);
    }
    m_thread = nullptr;
    m_source = nullptr;
}

void ReceiverSession::onConnected()
{
    emit connectedChanged(true);
    if (!m_replay)
        pollConfig();
}

void ReceiverSession::onWorkerFinished()
{
    // Источник закрылся сам (ошибка открытия, обрыв, конец записи) — сбрасываем состояние
    const bool replay = m_replay;
    stopWorker();
    reset();
    if (replay)
        emit status(tr("Replay finished"));
}

void ReceiverSession::reset()
{
    m_pending.clear();
    m_ackTimer.stop();
    if (m_recording) {
        m_recording = false;
        emit recordingChanged(false);
    }
    emit connectedChanged(false);
}

// ---------- Запись ----------
void ReceiverSession::startRecording(const QString &path)
{
    if (m_source)
        QMetaObject::invokeMethod(
            m_source, [source = m_source, path] { source->startRecording(path); }, Qt::QueuedConnection);
}

void ReceiverSession::stopRecording()
{
    if (m_source)
        QMetaObject::invokeMethod(m_source, &ReceiverSource::stopRecording, Qt::QueuedConnection);
}

void ReceiverSession::onRecordingChanged(bool active, const QString &err)
{
    m_recording = active;
    if (!err.isEmpty())
        emit error(err);
    emit recordingChanged(active);
}

// ---------- Конфигурация ----------
void ReceiverSession::applyConfig(const ubx::ConfigItems &items, bool flash)
{
    const QByteArray data = cfg::buildValset(items, flash);
    if (data.isEmpty()) {
        emit status(tr("Configuration error: unknown key"));
        return;
    }
    sendCommand(cfg::Command::Valset, data);
}

void ReceiverSession::factoryReset()
{
    sendCommand(cfg::Command::FactoryReset, cfg::buildFactoryReset());
}

void ReceiverSession::pollConfig()
{
    send(cfg::buildValget(cfg::pollKeys()), QStringLiteral("CFG-VALGET"));
}

void ReceiverSession::sendCommand(cfg::Command command, const QByteArray &data)
{
    const QString name = cfg::commandName(command);
    if (send(data, name)) {
        m_pending.push_back({command, timeutil::monotonic()});
        m_ackTimer.start(500);
        emit status(tr("%1 sent, waiting for confirmation…").arg(name));
    }
}

bool ReceiverSession::send(const QByteArray &data, const QString &what)
{
    if (!m_source) {
        emit status(tr("Cannot send %1: not connected").arg(what));
        return false;
    }
    // Запись — в потоке порта, без ожидания: ошибка придёт сигналом sendFailed
    QMetaObject::invokeMethod(
        m_source, [source = m_source, data, what] { source->send(data, what); }, Qt::QueuedConnection);
    return true;
}

void ReceiverSession::onSendFailed(const QString &what, const QString &err)
{
    emit status(tr("Cannot send %1: %2").arg(what, err));
    // Ответа на неотправленную команду не будет
    const auto it = std::find_if(m_pending.rbegin(), m_pending.rend(),
                                 [&what](const Pending &p) { return cfg::commandName(p.command) == what; });
    if (it != m_pending.rend())
        m_pending.erase(std::next(it).base());
    if (m_pending.empty())
        m_ackTimer.stop();
}

void ReceiverSession::onMessages(const QList<GnssMessage> &batch)
{
    for (const GnssMessage &msg : batch) {
        emit message(msg);
        if (const auto ack = cfg::commandAck(msg))
            onCommandAck(ack->first, ack->second);
        const ubx::ConfigValues values = cfg::valgetValues(msg);
        if (!values.isEmpty())
            emit configValues(values);
    }
}

void ReceiverSession::onCommandAck(cfg::Command command, bool ok)
{
    const auto it =
        std::find_if(m_pending.begin(), m_pending.end(), [command](const Pending &p) { return p.command == command; });
    if (it == m_pending.end())
        return;  // ответ на чужой или уже просроченный запрос
    m_pending.erase(it);
    const QString name = cfg::commandName(command);
    emit status(ok ? tr("%1 applied (ACK)").arg(name) : tr("The receiver rejected %1 (NAK)").arg(name));
    if (m_pending.empty()) {
        m_ackTimer.stop();
        pollConfig();  // показываем фактические значения после изменения
    }
}

void ReceiverSession::checkAckTimeout()
{
    if (!m_pending.empty() && timeutil::monotonic() - m_pending.front().sentAt > AckTimeout) {
        const QString name = cfg::commandName(m_pending.front().command);
        m_pending.clear();
        m_ackTimer.stop();
        emit status(tr("The receiver did not confirm %1").arg(name));
        pollConfig();
    }
}
