#include "core/replay.h"

#include "core/frame_reader.h"

#include <QFile>
#include <QFileInfo>

#include <cmath>

#include <algorithm>
#include <optional>

namespace {

// Время эпохи сообщения, секунд; nullopt — сообщение время не несёт
std::optional<double> epochTime(const GnssMessage &msg, bool usePvt)
{
    if (usePvt) {
        if (const auto *pvt = msg.get<ubx::NavPvt>())
            return pvt->iTow / 1000.0;
        return std::nullopt;
    }
    if (const auto *gga = msg.get<nmea::Gga>())
        return gga->utcTime;
    if (const auto *rmc = msg.get<nmea::Rmc>())
        return rmc->utcTime;
    return std::nullopt;
}

}  // namespace

QList<ReplayEpoch> replayEpochs(const QList<GnssMessage> &messages)
{
    const bool usePvt = std::any_of(messages.cbegin(), messages.cend(),
                                    [](const GnssMessage &m) { return m.get<ubx::NavPvt>() != nullptr; });
    QList<ReplayEpoch> epochs;
    std::optional<double> current;
    for (qsizetype i = 0; i < messages.size(); ++i) {
        const auto t = epochTime(messages[i], usePvt);
        const bool newEpoch = t && current && std::abs(*t - *current) > 1e-6;
        if (epochs.isEmpty() || newEpoch) {
            if (newEpoch) {
                // Переход через полночь (UTC) или неделю (iTOW) и разрывы записи — без долгого ожидания
                const double delta = *t - *current;
                epochs.last().delay = delta > 0 && delta <= ReplayWorker::MaxDelay ? delta : ReplayWorker::DefaultDelay;
            }
            epochs.append({i, 0, 0.0});
        }
        if (t)
            current = t;
        ++epochs.last().count;
    }
    if (!current) {
        // Время неизвестно: равномерные пачки
        epochs.clear();
        for (qsizetype i = 0; i < messages.size(); i += ReplayWorker::ChunkSize)
            epochs.append(
                {i, std::min<qsizetype>(ReplayWorker::ChunkSize, messages.size() - i), ReplayWorker::DefaultDelay});
    }
    return epochs;
}

ReplayWorker::ReplayWorker(const QString &path, double speed)
    : m_path(path)
    , m_speed(speed > 0 ? speed : 1.0)
    , m_timer(this)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ReplayWorker::playNext);
}

void ReplayWorker::start()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(tr("Cannot open %1: %2").arg(m_path, file.errorString()));
        emit finished();
        return;
    }
    FrameReader reader;
    m_messages = reader.feed(file.readAll());
    if (m_messages.isEmpty()) {
        emit error(tr("%1 contains no NMEA or UBX frames.").arg(QFileInfo(m_path).fileName()));
        emit finished();
        return;
    }
    m_epochs = replayEpochs(m_messages);
    m_next = 0;
    emit connected();
    m_timer.start(0);
}

void ReplayWorker::playNext()
{
    if (m_next >= m_epochs.size()) {
        emit finished();  // запись закончилась
        return;
    }
    const ReplayEpoch &e = m_epochs[m_next++];
    emit messages(m_messages.mid(e.first, e.count));
    m_timer.start(int(e.delay * 1000 / m_speed));
}

void ReplayWorker::stop()
{
    m_timer.stop();
}

void ReplayWorker::send(const QByteArray &, const QString &what)
{
    emit sendFailed(what, tr("a recording is being replayed, there is no receiver"));
}
