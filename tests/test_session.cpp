// Сквозной тест сеанса без приёмника: псевдотерминал (openpty) вместо USB-порта.
// Тест пишет «ответы приёмника» в master-сторону, а ReceiverSession открывает slave-сторону
// как обычный последовательный порт — работают настоящие GnssWorker, поток и QSerialPort.
//
// Только macOS и Linux. Запуск: ctest --test-dir build -R test_session
#include "core/receiver_config.h"
#include "core/replay.h"
#include "core/session.h"
#include "core/ubx.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#if defined(Q_OS_MACOS)
#include <util.h>
#else
#include <pty.h>
#endif

namespace {

QByteArray nmeaLine(const QByteArray &body)
{
    quint8 sum = 0;
    for (char c : body)
        sum ^= quint8(c);
    return '$' + body + '*' + QByteArray::number(sum, 16).rightJustified(2, '0').toUpper() + "\r\n";
}

const QByteArray Gga = nmeaLine("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,");

// Псевдотерминал: master — «приёмник», slave — «порт», который открывает приложение
class Pty
{
public:
    Pty()
    {
        char name[128] = {};
        if (openpty(&m_master, &m_slave, name, nullptr, nullptr) == 0) {
            m_name = QString::fromLocal8Bit(name);
            fcntl(m_master, F_SETFL, fcntl(m_master, F_GETFL) | O_NONBLOCK);
        }
    }
    ~Pty()
    {
        closeMaster();
        if (m_slave >= 0)
            ::close(m_slave);
    }
    Pty(const Pty &) = delete;
    Pty &operator=(const Pty &) = delete;

    bool isOpen() const { return m_master >= 0; }
    QString portName() const { return m_name; }

    void write(const QByteArray &data) const
    {
        QVERIFY(::write(m_master, data.constData(), size_t(data.size())) == data.size());
    }

    // Всё, что приложение отправило в «приёмник», пока не встретится needle или не выйдет время
    QByteArray readUntil(const QByteArray &needle, int timeoutMs = 3000)
    {
        QDeadlineTimer deadline(timeoutMs);
        while (!m_received.contains(needle) && !deadline.hasExpired()) {
            pollfd p{m_master, POLLIN, 0};
            if (::poll(&p, 1, 20) > 0) {
                char buf[4096];
                const ssize_t n = ::read(m_master, buf, sizeof buf);
                if (n > 0)
                    m_received.append(buf, n);
            }
            QCoreApplication::processEvents();
        }
        return m_received;
    }

    void clearReceived() { m_received.clear(); }

    void closeMaster()
    {
        if (m_master >= 0)
            ::close(m_master);
        m_master = -1;
    }

private:
    int m_master = -1;
    int m_slave = -1;
    QString m_name;
    QByteArray m_received;
};

bool anyStatusContains(const QSignalSpy &spy, const QString &text)
{
    for (const QList<QVariant> &args : spy)
        if (args.at(0).toString().contains(text))
            return true;
    return false;
}

}  // namespace

class SessionTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<GnssMessage>();
        qRegisterMetaType<ubx::ConfigValues>();
    }

    void receivesFramesAndPollsConfig()
    {
        Pty pty;
        QVERIFY(pty.isOpen());
        ReceiverSession session;
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        QSignalSpy messages(&session, &ReceiverSession::message);
        session.open(pty.portName(), 115200);
        QVERIFY(connected.wait(3000));
        QCOMPARE(connected.first().first().toBool(), true);

        // Сразу после подключения сеанс опрашивает конфигурацию
        const QByteArray valget = receiver_config::buildValget(receiver_config::pollKeys());
        QVERIFY(pty.readUntil(valget).contains(valget));

        pty.write(Gga + QByteArray::fromHex("b56205010200068a98c1"));
        QTRY_COMPARE_WITH_TIMEOUT(messages.size(), 2, 3000);
        QCOMPARE(messages.at(0).at(0).value<GnssMessage>().identity, QStringLiteral("GNGGA"));
        QCOMPARE(messages.at(1).at(0).value<GnssMessage>().identity, QStringLiteral("ACK-ACK"));

        session.close();
        QCOMPARE(connected.last().first().toBool(), false);
        QVERIFY(!session.isOpen());
    }

    void valsetIsConfirmedByAck()
    {
        Pty pty;
        ReceiverSession session;
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        QSignalSpy status(&session, &ReceiverSession::status);
        QSignalSpy values(&session, &ReceiverSession::configValues);
        session.open(pty.portName(), 115200);
        QVERIFY(connected.wait(3000));
        const QByteArray valget = receiver_config::buildValget(receiver_config::pollKeys());
        pty.readUntil(valget);
        pty.clearReceived();

        const ubx::ConfigItems items{{receiver_config::RateKey, 200}};
        session.applyConfig(items);
        const QByteArray valset = receiver_config::buildValset(items);
        QVERIFY(pty.readUntil(valset).contains(valset));
        pty.clearReceived();

        pty.write(ubx::frame(ubx::ClassAck, ubx::IdAckAck, QByteArray::fromHex("068a")));
        QTRY_VERIFY_WITH_TIMEOUT(anyStatusContains(status, QStringLiteral("applied (ACK)")), 3000);
        // После ACK значения перечитываются; ответ VALGET доходит до вкладки конфигурации
        QVERIFY(pty.readUntil(valget).contains(valget));
        pty.write(ubx::frame(ubx::ClassCfg, ubx::IdCfgValget, QByteArray::fromHex("0100000001002130c800")));
        QTRY_COMPARE_WITH_TIMEOUT(values.size(), 1, 3000);
        QCOMPARE(values.first().first().value<ubx::ConfigValues>().value(receiver_config::RateKey), 200);
    }

    void missingAckTimesOut()
    {
        Pty pty;
        ReceiverSession session;
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        QSignalSpy status(&session, &ReceiverSession::status);
        session.open(pty.portName(), 115200);
        QVERIFY(connected.wait(3000));
        session.factoryReset();
        QVERIFY(pty.readUntil(receiver_config::buildFactoryReset()).contains(receiver_config::buildFactoryReset()));
        QTRY_VERIFY_WITH_TIMEOUT(anyStatusContains(status, QStringLiteral("did not confirm CFG-CFG")), 4000);
    }

    void recordsRawStream()
    {
        Pty pty;
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("rec.ubx"));
        ReceiverSession session;
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        QSignalSpy recording(&session, &ReceiverSession::recordingChanged);
        QSignalSpy messages(&session, &ReceiverSession::message);
        session.open(pty.portName(), 115200);
        QVERIFY(connected.wait(3000));
        session.startRecording(path);
        QVERIFY(recording.wait(3000));
        QCOMPARE(recording.last().first().toBool(), true);

        const QByteArray stream = "noise" + Gga + Gga;
        pty.write(stream);
        QTRY_COMPARE_WITH_TIMEOUT(messages.size(), 2, 3000);
        session.stopRecording();
        QTRY_COMPARE_WITH_TIMEOUT(recording.last().first().toBool(), false, 3000);

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), stream);  // всё как пришло, включая мусор между кадрами
    }

    void replaysRecording()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("rec.ubx"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        for (const char *t : {"120000.00", "120000.10", "120000.20"})
            file.write(nmeaLine(QByteArray("GNGGA,") + t + ",5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,"));
        file.close();

        ReceiverSession session;
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        QSignalSpy messages(&session, &ReceiverSession::message);
        QSignalSpy status(&session, &ReceiverSession::status);
        session.openReplay(path, 100.0);
        QVERIFY(connected.wait(3000));
        QVERIFY(session.isReplay());
        // Конец записи закрывает сеанс сам
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 2, 3000);
        QCOMPARE(connected.last().first().toBool(), false);
        QCOMPARE(messages.size(), 3);
        QVERIFY(anyStatusContains(status, QStringLiteral("Replay finished")));
    }

    void openFailureIsReported()
    {
        ReceiverSession session;
        QSignalSpy error(&session, &ReceiverSession::error);
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        session.open(QStringLiteral("/dev/ucenter-no-such-port"), 115200);
        QVERIFY(error.wait(3000));
        QTRY_VERIFY_WITH_TIMEOUT(!session.isOpen(), 3000);
        QCOMPARE(connected.last().first().toBool(), false);
    }

    void reopenAfterClose()
    {
        Pty pty;
        ReceiverSession session;
        QSignalSpy connected(&session, &ReceiverSession::connectedChanged);
        for (int i = 0; i < 3; ++i) {
            session.open(pty.portName(), 115200);
            QTRY_VERIFY_WITH_TIMEOUT(!connected.isEmpty() && connected.last().first().toBool(), 3000);
            session.close();
            QVERIFY(!connected.last().first().toBool());
        }
    }
};

QTEST_GUILESS_MAIN(SessionTest)
#include "test_session.moc"
