// Замеры ядра на эталонной записи (дорожная карта, фаза 1). Не входит в ctest: запускается вручную,
//   build/bench_core            — все замеры
//   build/bench_core frameReader — один
// Время — на всю запись (10 минут, 6000 эпох); на эпоху — разделить на ReferenceEpochs.
#include "recording.h"

#include "core/frame_reader.h"
#include "core/messages.h"
#include "core/navigation.h"
#include "core/satellites.h"

#include <QTest>

class CoreBench : public QObject
{
    Q_OBJECT

    static constexpr qsizetype ReadSize = 4096;  // столько отдаёт порт за одно чтение при 10 Гц

    QByteArray m_raw;
    QList<GnssMessage> m_messages;
    QList<double> m_times;  // время эпохи каждого сообщения, с: трекеру нужно время для устаревания

private slots:
    void initTestCase()
    {
        m_raw = recording::reference();
        FrameReader reader;
        m_messages = reader.feed(m_raw);
        const double perEpoch = double(m_messages.size()) / recording::ReferenceEpochs;
        for (qsizetype i = 0; i < m_messages.size(); ++i)
            m_times.append(double(qsizetype(i / perEpoch)) / recording::RateHz);
        qInfo("Reference recording: %lld bytes, %lld messages, %d epochs", qint64(m_raw.size()),
              qint64(m_messages.size()), recording::ReferenceEpochs);
    }

    // Сборка кадров из байтов порта (поток чтения)
    void frameReader()
    {
        QBENCHMARK {
            FrameReader reader;
            qsizetype frames = 0;
            for (qsizetype at = 0; at < m_raw.size(); at += ReadSize)
                frames += reader.feed(m_raw.mid(at, ReadSize)).size();
            QCOMPARE(frames, m_messages.size());
        }
    }

    // Навигационное решение (сейчас — GUI-поток)
    void navigationApply()
    {
        QBENCHMARK {
            NavigationState nav;
            for (qsizetype i = 0; i < m_messages.size(); ++i)
                nav.apply(m_messages[i], m_times[i]);
        }
    }

    // Трекер спутников (сейчас — GUI-поток)
    void satelliteHandle()
    {
        QBENCHMARK {
            SatelliteTracker tracker;
            for (qsizetype i = 0; i < m_messages.size(); ++i)
                tracker.handle(m_messages[i], m_times[i]);
        }
    }

    // Снимок для вкладки «Спутники»: один на эпоху
    void satelliteSnapshot()
    {
        SatelliteTracker tracker;
        for (qsizetype i = 0; i < m_messages.size(); ++i)
            tracker.handle(m_messages[i], m_times[i]);
        const double end = m_times.last();
        QBENCHMARK {
            QCOMPARE(tracker.snapshot(end).sats.size(), 32);
        }
    }

    // Текст для консоли (сейчас — GUI-поток)
    void consoleText()
    {
        QBENCHMARK {
            qsizetype chars = 0;
            for (const GnssMessage &msg : std::as_const(m_messages))
                chars += messages::messageText(msg).size();
            QVERIFY(chars > 0);
        }
    }
};

QTEST_GUILESS_MAIN(CoreBench)
#include "bench_core.moc"
