#include "core/worker.h"

#include <QFile>

GnssWorker::GnssWorker(const QString &port, int baudrate)
    : m_port(port)
    , m_baudrate(baudrate)
    , m_writeTimer(this)
{
    m_writeTimer.setSingleShot(true);
    connect(&m_writeTimer, &QTimer::timeout, this, &GnssWorker::onWriteTimeout);
}

void GnssWorker::start()
{
    // Порт создаётся здесь, чтобы принадлежать потоку чтения
    m_serial = new QSerialPort(this);
    m_serial->setPortName(m_port);
    m_serial->setBaudRate(m_baudrate);
    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit error(tr("Cannot open %1: %2").arg(m_port, m_serial->errorString()));
        emit finished();
        return;
    }
    connect(m_serial, &QSerialPort::readyRead, this, &GnssWorker::onReadyRead);
    connect(m_serial, &QSerialPort::bytesWritten, this, &GnssWorker::onBytesWritten);
    connect(m_serial, &QSerialPort::errorOccurred, this, &GnssWorker::onError);
    emit connected();
}

void GnssWorker::send(const QByteArray &data, const QString &what)
{
    // Запись неблокирующая: данные уходят из цикла событий, GUI их не ждёт
    if (!m_serial || !m_serial->isOpen()) {
        emit sendFailed(what, tr("port is not open"));
        return;
    }
    if (m_serial->write(data) != data.size()) {
        emit sendFailed(what, tr("write error: %1").arg(m_serial->errorString()));
        return;
    }
    m_writing = what;
    m_writeTimer.start(WriteTimeoutMs);
}

void GnssWorker::onBytesWritten()
{
    if (m_serial->bytesToWrite() == 0)
        m_writeTimer.stop();
}

void GnssWorker::onWriteTimeout()
{
    if (!m_serial || m_serial->bytesToWrite() == 0)
        return;
    // Приёмник не забирает данные (завис или переполнен) — не копим очередь
    m_serial->clear(QSerialPort::Output);
    emit sendFailed(m_writing, tr("the receiver does not accept data"));
}

void GnssWorker::stop()
{
    m_writeTimer.stop();
    closeRecording(QString());
    if (m_serial && m_serial->isOpen()) {
        m_serial->disconnect(this);
        m_serial->close();
    }
}

void GnssWorker::startRecording(const QString &path)
{
    closeRecording(QString());
    auto *file = new QFile(path, this);
    if (!file->open(QIODevice::WriteOnly)) {
        const QString err = tr("Cannot write %1: %2").arg(path, file->errorString());
        delete file;
        emit recordingChanged(false, err);
        return;
    }
    m_record = file;
    emit recordingChanged(true, QString());
}

void GnssWorker::stopRecording()
{
    closeRecording(QString());
}

void GnssWorker::closeRecording(const QString &error)
{
    if (!m_record)
        return;
    m_record->close();
    m_record->deleteLater();
    m_record = nullptr;
    emit recordingChanged(false, error);
}

void GnssWorker::onReadyRead()
{
    const QByteArray data = m_serial->readAll();
    // Пишется весь поток как есть, включая байты, которые не сложились в кадры
    if (m_record && m_record->write(data) != data.size())
        closeRecording(tr("Recording stopped: %1").arg(m_record->errorString()));
    // Кадры собираются из того, что пришло; неполный хвост ждёт следующего чтения
    const QList<GnssMessage> batch = m_reader.feed(data);
    if (!batch.isEmpty())
        emit messages(batch);
}

void GnssWorker::onError(QSerialPort::SerialPortError err)
{
    if (err == QSerialPort::NoError || err == QSerialPort::TimeoutError)
        return;
    // Приёмник выдернули или порт сломался — дальше читать нечего
    const QString text = m_serial->errorString();
    stop();
    emit error(tr("Connection lost: %1").arg(text));
    emit finished();
}
