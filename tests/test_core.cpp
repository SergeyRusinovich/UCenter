// Тесты логики без GUI: геодезия, измерение A->B, хранение, поиск мест, правила точек,
// разбор NMEA/UBX, навигационное решение и спутники.
//
// Запуск: ctest --test-dir build (или build/test_core)
#include "core/baseline.h"
#include "core/baseline_store.h"
#include "core/cpu_time.h"
#include "core/frame_reader.h"
#include "core/geo.h"
#include "core/language.h"
#include "core/message_log.h"
#include "core/messages.h"
#include "core/navigation.h"
#include "core/nmea.h"
#include "core/places.h"
#include "core/points.h"
#include "core/receiver_config.h"
#include "core/replay.h"
#include "core/satellites.h"
#include "core/timeutil.h"
#include "core/ubx.h"
#include "recording.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

namespace {

double dms(double d, double m, double s)
{
    return d + m / 60 + s / 3600;
}

bool near(double a, double b, int places)
{
    return std::abs(a - b) < 0.5 * std::pow(10.0, -places);
}

QJsonObject item(const QString &name, const QString &address, double lat, double lon, const QJsonValue &geo)
{
    return {{"name", name},
            {"display_name", address},
            {"lat", QString::number(lat)},
            {"lon", QString::number(lon)},
            {"boundingbox", QJsonArray{QString::number(lat - 0.01), QString::number(lat + 0.01),
                                       QString::number(lon - 0.01), QString::number(lon + 0.01)}},
            {"geojson", geo}};
}

const QJsonObject Line{{"type", "LineString"},
                       {"coordinates", QJsonArray{QJsonArray{37.6, 55.7}, QJsonArray{37.61, 55.71}}}};

// Строка NMEA с правильной контрольной суммой
QByteArray nmeaLine(const QByteArray &body)
{
    quint8 sum = 0;
    for (char c : body)
        sum ^= quint8(c);
    return '$' + body + '*' + QByteArray::number(sum, 16).rightJustified(2, '0').toUpper() + "\r\n";
}

GnssMessage nmeaMsg(const QByteArray &body)
{
    return *nmea::parse(nmeaLine(body));
}

GnssMessage ubxMsg(const QByteArray &hex)
{
    return ubx::message(QByteArray::fromHex(hex));
}

// Эталонные кадры, собранные pyubx2 1.3.8 (на нём работала Python-версия)
const QByteArray NavPvtHex =
    "b56201075c00e8030000ea070a0b091b320300000000000000000321000cc8f16b16f0a83b21f0490200e022020000000000"
    "00000000000000000000000000000000dc050000a85ebc0000000000000000007b000000000000008094450000000000c2e3";

// NAV-SAT: G5 (C/N0 42, 45°/270°, в решении), R12 (не отслеживается, положение неизвестно),
// S124 (C/N0 35, 20°/150°)
const QByteArray NavSatHex =
    "b56201352c00e80300000103000000052a2d0e0100000f000000060c00a50000000000000000017c23149600000004000000d0c3";
// NAV-DOP: gDOP 1.8, pDOP 1.5, tDOP 0.9, vDOP 1.2, hDOP 0.85, nDOP 0.6, eDOP 0.6
const QByteArray NavDopHex = "b56201041200e8030000b40096005a00780055003c003c00ebdf";

QByteArray ggaAt(const QByteArray &time)
{
    return nmeaLine("GNGGA," + time + ",5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,");
}

}  // namespace

class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Без организации QSettings в Windows ничего не читает и не пишет; своё имя —
        // чтобы тесты не трогали настройки приложения
        QCoreApplication::setOrganizationName(QStringLiteral("UCenter-tests"));
    }

    // ---------- Геодезия ----------
    void vincentyReference()
    {
        // Пример Geoscience Australia: Flinders Peak -> Buninyong
        const auto r =
            geo::inverse(-dms(37, 57, 3.72030), dms(144, 25, 29.52440), -dms(37, 39, 10.15610), dms(143, 55, 35.38390));
        QVERIFY(near(r.distance, 54972.271, 3));
        QVERIFY(near(*r.azimuth, dms(306, 52, 5.37), 5));
        QVERIFY(near(*r.backAzimuth, dms(127, 10, 25.07), 5));
    }

    void samePointHasNoAzimuth()
    {
        const auto r = geo::inverse(55.75, 37.62, 55.75, 37.62);
        QCOMPARE(r.distance, 0.0);
        QVERIFY(!r.azimuth && !r.backAzimuth);
    }

    void nearAntipodalFallsBackToSphere()
    {
        const auto r = geo::inverse(0, 0, 0.5, 179.7);
        QVERIFY(r.distance > 19'000'000);
        QVERIFY(r.azimuth.has_value());
    }

    void mercatorRoundTrip()
    {
        const QPointF px = geo::llToPx(55.75, 37.62, 12);
        const LatLon ll = geo::pxToLl(px.x(), px.y(), 12);
        QVERIFY(near(ll.lat, 55.75, 9));
        QVERIFY(near(ll.lon, 37.62, 9));
    }

    // ---------- Измерение A->B ----------
    void dueNorth()
    {
        const Baseline m = measure({55.75, 37.62}, {55.76, 37.62});
        QVERIFY(near(*m.azimuth, 0.0, 6));
        QVERIFY(near(m.dEast(), 0.0, 6));
        QVERIFY(near(m.dNorth(), m.distanceM, 6));
    }

    void deviationSign()
    {
        const Baseline west = measure({55.75, 37.62}, {55.76, 37.60});
        QVERIFY(*west.deviation() < 0);
        QVERIFY(formatDeviation(west.deviation()).endsWith(QStringLiteral("(W)")));
        QVERIFY(formatDeviation(measure({55.75, 37.62}, {55.76, 37.64}).deviation()).endsWith(QStringLiteral("(E)")));
    }

    // ---------- Хранение ----------
    void jsonRoundtripAndCsv()
    {
        const QList<Baseline> items{measure({55.75, 37.62}, {55.76, 37.63})};
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("sub/m.json"));
        QList<Baseline> loaded;
        QCOMPARE(baseline_store::loadJson(path, &loaded), QString());  // файла ещё нет
        QVERIFY(loaded.isEmpty());
        QCOMPARE(baseline_store::saveJson(path, items), QString());
        QCOMPARE(baseline_store::loadJson(path, &loaded), QString());
        QCOMPARE(loaded, items);
        const QString csvPath = tmp.filePath(QStringLiteral("m.csv"));
        QCOMPARE(baseline_store::writeCsv(csvPath, items), QString());
        QFile csv(csvPath);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        QCOMPARE(csv.readAll().trimmed().split('\n').size(), 2);
    }

    void legacyFileIsMovedOnce()
    {
        QTemporaryDir tmp;
        const QString legacy = tmp.filePath(QStringLiteral("m.json"));
        const QString path = tmp.filePath(QStringLiteral("sub/m.json"));
        // Переносить нечего — новый каталог не создаётся
        QCOMPARE(baseline_store::moveFromLegacy(legacy, path), QString());
        QVERIFY(!QFileInfo::exists(tmp.filePath(QStringLiteral("sub"))));

        const QList<Baseline> old{measure({55.75, 37.62}, {55.76, 37.63})};
        QCOMPARE(baseline_store::saveJson(legacy, old), QString());
        QCOMPARE(baseline_store::moveFromLegacy(legacy, path), QString());
        QVERIFY(!QFile::exists(legacy));
        QList<Baseline> loaded;
        QCOMPARE(baseline_store::loadJson(path, &loaded), QString());
        QCOMPARE(loaded, old);

        // В новом каталоге уже есть файл — прежний не трогаем и ничего не перезаписываем
        const QList<Baseline> other{measure({40.0, -3.7}, {40.1, -3.6})};
        QCOMPARE(baseline_store::saveJson(legacy, other), QString());
        QCOMPARE(baseline_store::moveFromLegacy(legacy, path), QString());
        QVERIFY(QFile::exists(legacy));
        QCOMPARE(baseline_store::loadJson(path, &loaded), QString());
        QCOMPARE(loaded, old);
    }

    void corruptJsonIsReported()
    {
        QTemporaryDir tmp;
        const QString path = tmp.filePath(QStringLiteral("m.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[{\"a_lat\": 1}]");
        f.close();
        QList<Baseline> loaded;
        QVERIFY(!baseline_store::loadJson(path, &loaded).isEmpty());
    }

    // ---------- Поиск мест ----------
    void parseGeometryKinds()
    {
        QJsonArray rings;  // QJsonArray{QJsonArray{…}} скопировал бы массив, а не вложил его
        rings.append(QJsonArray{QJsonArray{37.6, 55.7}, QJsonArray{37.61, 55.7}, QJsonArray{37.6, 55.71}});
        const QJsonObject poly{{"type", "Polygon"}, {"coordinates", rings}};
        const QList<Place> places = places::parseNominatim(
            {item("a", "a, X", 55.7, 37.6, Line), item("b", "b, X", 55.7, 37.6, poly),
             item("c", "c, X", 55.7, 37.6, QJsonObject{{"type", "Point"}}), QJsonObject{{"broken", true}}});
        QCOMPARE(places.size(), 3);
        QCOMPARE(places[0].areas.size(), 0);
        QCOMPARE(places[0].lines.size(), 1);
        QCOMPARE(places[1].areas.size(), 1);
        QCOMPARE(places[1].lines.size(), 0);
        QVERIFY(places[2].areas.isEmpty() && places[2].lines.isEmpty());
        QCOMPARE(places[0].lines[0][0], (LatLon{55.7, 37.6}));  // GeoJSON [lon, lat] -> (lat, lon)
    }

    void mergeStreetSegments()
    {
        const QList<Place> places = places::parseNominatim({
            item("Тверская улица", "Тверская улица, 58, Москва, 125009", 55.76, 37.60, Line),
            item("Тверская улица", "Тверская улица, 57, Москва, 103265", 55.77, 37.61, Line),
            item("Тверская улица", "Тверская улица, Казань", 55.79, 49.10, Line),
        });
        const QList<Place> merged = places::mergeDuplicates(places);
        QCOMPARE(merged.size(), 2);
        QCOMPARE(merged[0].lines.size(), 2);
    }

    void sortByDistance()
    {
        const Place far = places::parseNominatim({item("far", "far", 55.79, 49.10, QJsonValue())})[0];
        const Place nearby = places::parseNominatim({item("near", "near", 55.76, 37.60, QJsonValue())})[0];
        const QList<Place> sorted = places::sortByDistance({far, nearby}, {55.75, 37.62});
        QCOMPARE(sorted[0].name, QStringLiteral("near"));
        QCOMPARE(sorted[1].name, QStringLiteral("far"));
    }

    // ---------- Правила точек ----------
    void withoutDeviceFirstClickPlacesA()
    {
        MeasurementPoints p;
        QCOMPARE(p.click({1.0, 1.0}), std::optional(PointKey::A));
        QCOMPARE(p.click({2.0, 2.0}), std::optional(PointKey::B));
        QVERIFY(!p.click({3.0, 3.0}));
        QVERIFY(p.result().has_value());
    }

    void liveAFollowsDeviceAndClickPlacesB()
    {
        MeasurementPoints p;
        QVERIFY(p.deviceMoved({1.0, 1.0}));
        QCOMPARE(p.click({2.0, 2.0}), std::optional(PointKey::B));
        p.deviceMoved({1.5, 1.5});
        QCOMPARE(p[PointKey::A], std::optional(LatLon{1.5, 1.5}));
    }

    void manualAIsNotOverwrittenUntilAttach()
    {
        MeasurementPoints p;
        p.click({1.0, 1.0});  // A вручную, позиции устройства ещё нет
        QVERIFY(!p.deviceMoved({5.0, 5.0}));
        QCOMPARE(p[PointKey::A], std::optional(LatLon{1.0, 1.0}));
        QVERIFY(p.attachA());
        QCOMPARE(p[PointKey::A], std::optional(LatLon{5.0, 5.0}));
    }

    void newMeasurementKeepsLiveA()
    {
        MeasurementPoints p;
        p.deviceMoved({1.0, 1.0});
        p.click({2.0, 2.0});
        p.newMeasurement();
        QCOMPARE(p[PointKey::A], std::optional(LatLon{1.0, 1.0}));
        QVERIFY(!p[PointKey::B]);
    }

    void loadDetachesA()
    {
        MeasurementPoints p;
        p.deviceMoved({1.0, 1.0});
        p.load({2.0, 2.0}, {3.0, 3.0});
        p.deviceMoved({4.0, 4.0});
        QCOMPARE(p[PointKey::A], std::optional(LatLon{2.0, 2.0}));
    }

    // ---------- Время ----------
    void timeFormatting()
    {
        QCOMPARE(timeutil::formatHms(9 * 3600 + 27 * 60 + 50), QStringLiteral("09:27:50"));
        QCOMPARE(timeutil::formatDuration(2 * 86400 + 61), QStringLiteral("2d 00:01:01"));
        QCOMPARE(timeutil::offsetLabel(-3.5), QStringLiteral("UTC-03:30"));
        QCOMPARE(timeutil::offsetLabel(5.75), QStringLiteral("UTC+05:45"));
        QCOMPARE(timeutil::toLocal(3600, -3), 22 * 3600);
        QCOMPARE(timeutil::utcOffsets().size(), 53);
    }

    // ---------- Язык интерфейса ----------
    void languageIsChosenExplicitly()
    {
        // Язык системы не учитывается: без выбора — английский, иначе выбранный
        QSettings().remove(QStringLiteral("ui/language"));
        QCOMPARE(language::current(), language::Default);
        QCOMPARE(language::Default, language::English);
        for (const char *code : {"ru", "es", "zh_CN", "en"}) {
            language::save(QString::fromLatin1(code));
            QCOMPARE(language::current(), QString::fromLatin1(code));
        }
        language::save(QStringLiteral("fr"));  // неизвестный язык — снова по умолчанию
        QCOMPARE(language::current(), language::Default);
        QSettings().remove(QStringLiteral("ui/language"));
        QCOMPARE(language::available().size(), 4);
        QVERIFY(language::isAvailable(QStringLiteral("zh_CN")) && !language::isAvailable(QStringLiteral("zh")));
    }

    // ---------- NMEA ----------
    void nmeaChecksumIsChecked()
    {
        QVERIFY(nmea::parse(nmeaLine("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,")));
        // Та же строка с неверной суммой (pyubx2 тоже её отбрасывает)
        QVERIFY(!nmea::parse("$GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76\r\n"));
        QVERIFY(!nmea::parse("$GNGGA,092750.000"));
    }

    void decodeGga()
    {
        const GnssMessage msg = nmeaMsg("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,");
        const auto *g = msg.get<nmea::Gga>();
        QVERIFY(g);
        QCOMPARE(*g->utcSeconds, 9 * 3600 + 27 * 60 + 50);
        QVERIFY(near(*g->lat, 53 + 21.6802 / 60, 9));
        QVERIFY(near(*g->lon, -(6 + 30.3372 / 60), 9));
        QCOMPARE(g->quality, 1);
        QCOMPARE(*g->numSv, 8);
        QVERIFY(near(*g->hdop, 1.03, 9));
        QVERIFY(near(*g->alt, 61.7, 9));
    }

    void decodeGgaWithoutFix()
    {
        const GnssMessage msg = nmeaMsg("GPGGA,,,,,,0,00,99.99,,,,,,");
        const auto *g = msg.get<nmea::Gga>();
        QVERIFY(g);
        QVERIFY(!g->utcSeconds && !g->lat && !g->lon && !g->alt);
        QCOMPARE(g->quality, 0);
    }

    void decodeGsvWithSignalId()
    {
        const GnssMessage msg = nmeaMsg("GPGSV,3,1,10,01,40,083,46,02,17,308,,03,07,344,,04,22,228,41,1");
        QCOMPARE(msg.talker, QStringLiteral("GP"));
        QCOMPARE(msg.identity, QStringLiteral("GPGSV"));
        const auto *g = msg.get<nmea::Gsv>();
        QVERIFY(g);
        QCOMPARE(g->signalId, QStringLiteral("1"));
        QCOMPARE(g->sats.size(), 4);
        QCOMPARE(g->sats[0].svid, 1);
        QCOMPARE(*g->sats[0].elv, 40);
        QCOMPARE(*g->sats[0].az, 83);
        QCOMPARE(*g->sats[0].cno, 46);
        QVERIFY(!g->sats[1].cno);
    }

    void proprietaryIdentity()
    {
        const GnssMessage msg = nmeaMsg("PUBX,00,081350.00,4717.113210,N");
        QCOMPARE(msg.identity, QStringLiteral("PUBX00"));
        QVERIFY(!msg.get<nmea::Gga>());
    }

    // ---------- UBX ----------
    void decodeNavPvtReference()
    {
        const GnssMessage msg = ubxMsg(NavPvtHex);
        QCOMPARE(msg.identity, QStringLiteral("NAV-PVT"));
        const auto *p = msg.get<ubx::NavPvt>();
        QVERIFY(p);
        QCOMPARE(p->year, 2026);
        QCOMPARE(p->hour, 9);
        QCOMPARE(p->min, 27);
        QCOMPARE(p->second, 50);
        QVERIFY(p->validDate && p->validTime);
        QCOMPARE(p->fixType, 3);
        QVERIFY(p->gnssFixOk && !p->diffSoln && p->headVehValid);
        QCOMPARE(p->numSv, 12);
        QVERIFY(near(p->lat, 55.7558, 7));
        QVERIFY(near(p->lon, 37.6173, 7));
        QCOMPARE(p->hMsl, 140000);
        QCOMPARE(p->gSpeed, 1500);
        QVERIFY(near(p->headMot, 123.45, 5));
        QVERIFY(near(p->pDop, 1.23, 9));
        QVERIFY(near(p->headVeh, 45.6, 5));
    }

    void buildValsetMatchesPyubx2()
    {
        QCOMPARE(receiver_config::buildValset({{receiver_config::RateKey, 200}}).toHex(),
                 QByteArray("b562068a0a000003000001002130c800b793"));
    }

    void buildValgetMatchesPyubx2()
    {
        QCOMPARE(receiver_config::buildValget({receiver_config::RateKey, receiver_config::GpsEnaKey}).toHex(),
                 QByteArray("b562068b0c0000000000010021301f0031104fd9"));
    }

    void valsetAck()
    {
        const GnssMessage ack = ubxMsg("b56205010200068a98c1");
        QCOMPARE(ack.identity, QStringLiteral("ACK-ACK"));
        QCOMPARE(receiver_config::commandAck(ack), std::optional(std::pair{receiver_config::Command::Valset, true}));
        QCOMPARE(ubx::describe(ack), QStringLiteral("<UBX(ACK-ACK, clsID=CFG, msgID=CFG-VALSET)>"));
        const GnssMessage nak = ubx::message(ubx::frame(ubx::ClassAck, ubx::IdAckNak, QByteArray::fromHex("068a")));
        QCOMPARE(receiver_config::commandAck(nak), std::optional(std::pair{receiver_config::Command::Valset, false}));
        const GnssMessage other = ubx::message(ubx::frame(ubx::ClassAck, ubx::IdAckAck, QByteArray::fromHex("0601")));
        QVERIFY(!receiver_config::commandAck(other));
    }

    void valgetValues()
    {
        // version 1, слой RAM, CFG-RATE-MEAS = 1000, GPS_ENA = 1, неопрошенный CFG-RATE-NAV = 1
        const QByteArray payload = QByteArray::fromHex("01000000"
                                                       "01002130e803"
                                                       "1f00311001"
                                                       "020021300100");
        const GnssMessage msg = ubx::message(ubx::frame(ubx::ClassCfg, ubx::IdCfgValget, payload));
        const ubx::ConfigValues values = receiver_config::valgetValues(msg);
        QCOMPARE(values.size(), 2);
        QCOMPARE(values.value(receiver_config::RateKey), 1000);
        QCOMPARE(values.value(receiver_config::GpsEnaKey), 1);
    }

    // ---------- Замеры ----------
    void cpuTimeGrowsWithWork()
    {
        const double thread0 = cpu_time::thread(), process0 = cpu_time::process();
        QElapsedTimer t;
        t.start();
        volatile double x = 0;
        while (t.elapsed() < 100)
            x = x + std::sqrt(double(t.nsecsElapsed()));
        const double thread = cpu_time::thread() - thread0, process = cpu_time::process() - process0;
        QVERIFY2(thread > 0.05 && thread < 1.0, qPrintable(QString::number(thread)));
        // Процесс — не меньше потока. Счётчики Windows идут квантами по 15.6 мс и снимаются не в один
        // момент: время процесса может отстать от времени потока на квант-другой
        QVERIFY2(process >= thread - 0.035,
                 qPrintable(QStringLiteral("process %1 < thread %2").arg(process).arg(thread)));
    }

    // ---------- Консоль ----------
    void messageLogEvictsAndFilters()
    {
        const auto msg = [](int n, bool ubx) {
            GnssMessage m;
            m.protocol = ubx ? GnssMessage::Protocol::Ubx : GnssMessage::Protocol::Nmea;
            m.identity = QString::number(n);
            return m;
        };
        const auto ids = [](const MessageLog &log) {
            QStringList out;
            for (qsizetype i = 0; i < log.rowCount(); ++i)
                out << log.row(i).identity;
            return out.join(u' ');
        };
        // Число вытесненных строк; -1 — rowsAddedBy() разошёлся с тем, что добавил append()
        const auto add = [](MessageLog &log, const QList<GnssMessage> &batch) -> qsizetype {
            const qsizetype evicted = log.rowsEvictedBy(batch.size());
            log.evict(batch.size());
            const qsizetype added = log.rowsAddedBy(batch);
            const qsizetype before = log.rowCount();
            log.append(batch);
            return log.rowCount() == before + added ? evicted : -1;
        };

        MessageLog log(5);
        QCOMPARE(add(log, {msg(0, false), msg(1, true), msg(2, false), msg(3, true)}), 0);
        QCOMPARE(ids(log), QStringLiteral("0 1 2 3"));
        log.setFilter(MessageLog::Filter::Ubx);  // фильтр — по уже накопленным
        QCOMPARE(ids(log), QStringLiteral("1 3"));
        // Ещё три: вытесняются 0 и 1, из видимых строк — одна («1»)
        QCOMPARE(add(log, {msg(4, false), msg(5, true), msg(6, false)}), 1);
        QCOMPARE(ids(log), QStringLiteral("3 5"));
        log.setFilter(MessageLog::Filter::Nmea);
        QCOMPARE(ids(log), QStringLiteral("2 4 6"));
        log.setFilter(MessageLog::Filter::All);
        QCOMPARE(ids(log), QStringLiteral("2 3 4 5 6"));
        // Пачка больше ёмкости: остаются её последние сообщения
        QList<GnssMessage> big;
        for (int n = 10; n < 18; ++n)
            big << msg(n, false);
        QCOMPARE(add(log, big), 5);
        QCOMPARE(ids(log), QStringLiteral("13 14 15 16 17"));
        log.clear();
        QCOMPARE(log.rowCount(), 0);
        QCOMPARE(add(log, {msg(20, true)}), 0);
        QCOMPARE(ids(log), QStringLiteral("20"));
    }

    // Подробности есть у каждого типа из message_types.h: разобранные или поименованные поля и кадр
    void messageDetailsForEveryType()
    {
        FrameReader reader;
        QList<GnssMessage> msgs = reader.feed(recording::reference(1));
        msgs << ubxMsg("b56205010200068a98c1")
             << ubx::message(ubx::frame(ubx::ClassCfg, ubx::IdCfgValget,
                                        QByteArray::fromHex("01000000"
                                                            "01002130e803")));
        QSet<qsizetype> types;
        for (const GnssMessage &m : std::as_const(msgs)) {
            types.insert(qsizetype(m.data.index()));
            const auto rows = messages::details(m);
            QHash<QString, QString> map;
            for (const auto &[name, value] : rows)
                map.insert(name, value);
            QCOMPARE(map.value(QStringLiteral("message")), m.identity);
            QVERIFY2(rows.size() > 3, qPrintable(m.identity));  // тип, длина, поля, кадр
            QVERIFY(!map.value(QStringLiteral("raw")).isEmpty());
        }
        QCOMPARE(types.size(), qsizetype(std::variant_size_v<MessageData>) - 1);  // все, кроме monostate
        QVERIFY(!types.contains(0));

        const auto value = [&msgs](const QString &identity, const QString &name) {
            for (const GnssMessage &m : std::as_const(msgs))
                if (m.identity == identity)
                    for (const auto &[n, v] : messages::details(m))
                        if (n == name)
                            return v;
            return QString();
        };
        QCOMPARE(value(QStringLiteral("GNGGA"), QStringLiteral("quality")), QStringLiteral("1"));
        QCOMPARE(value(QStringLiteral("GPGSV"), QStringLiteral("signalId")), QStringLiteral("1"));
        QCOMPARE(value(QStringLiteral("GNGSA"), QStringLiteral("systemId")), QStringLiteral("1"));
        QCOMPARE(value(QStringLiteral("NAV-PVT"), QStringLiteral("fixType")), QStringLiteral("3"));
        QCOMPARE(value(QStringLiteral("ACK-ACK"), QStringLiteral("msgID")), QStringLiteral("CFG-VALSET"));
        QCOMPARE(value(QStringLiteral("CFG-VALGET"), QStringLiteral("CFG_RATE_MEAS")), QStringLiteral("1000"));
        QVERIFY(value(QStringLiteral("NAV-PVT"), QStringLiteral("raw")).startsWith(QStringLiteral("B5 62 01 07")));
    }

    // ---------- Разбор потока ----------
    void frameReaderSplitsMixedStream()
    {
        const QByteArray gga = nmeaLine("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,");
        const QByteArray pvt = QByteArray::fromHex(NavPvtHex);
        QByteArray corrupt = pvt;
        corrupt[20] = char(corrupt[20] ^ 0x01);  // битый кадр пропускается
        const QByteArray stream =
            QByteArray("\x00garbage", 8) + gga + corrupt + "\xb5" + pvt + "$GNRMC,broken\r\n" + gga;

        FrameReader reader;
        QList<GnssMessage> all;
        for (qsizetype i = 0; i < stream.size(); i += 7)  // приходит кусками
            all += reader.feed(stream.mid(i, 7));
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].identity, QStringLiteral("GNGGA"));
        QCOMPARE(all[1].identity, QStringLiteral("NAV-PVT"));
        QCOMPARE(all[1].raw, pvt);
        QCOMPARE(all[2].identity, QStringLiteral("GNGGA"));
    }

    // ---------- Навигационное решение ----------
    void navigationFromNmea()
    {
        NavigationState nav;
        QVERIFY(nav.apply(nmeaMsg("GNGSA,A,3,01,02,03,04,,,,,,,,,1.50,0.90,1.20,1"), 0.0));
        QVERIFY(nav.apply(nmeaMsg("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,"), 0.0));
        QVERIFY(nav.apply(nmeaMsg("GNRMC,092750.000,A,5321.6802,N,00630.3372,W,10.0,45.0,111026,,,A"), 0.0));
        const NavData &d = nav.data();
        QCOMPARE(*d.fix, QStringLiteral("3D"));
        QVERIFY(near(*d.pdop, 1.5, 9));
        QVERIFY(near(*d.hdop, 1.03, 9));
        QCOMPARE(*d.numSv, 8);
        QVERIFY(near(*d.speedKmh, 18.52, 9));
        QVERIFY(near(*d.cog, 45.0, 9));
    }

    void navPvtHasPriorityOverNmea()
    {
        NavigationState nav;
        QVERIFY(nav.apply(ubxMsg(NavPvtHex), 0.0));
        QVERIFY(near(*nav.data().lat, 55.7558, 7));
        QVERIFY(nav.data().headingSupported);
        // Пока PVT свежий, GGA меняет только HDOP
        nav.apply(nmeaMsg("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,"), 1.0);
        QVERIFY(near(*nav.data().lat, 55.7558, 7));
        QVERIFY(near(*nav.data().hdop, 1.03, 9));
        // PVT пропал больше чем на 2 с — снова работает NMEA
        nav.apply(nmeaMsg("GNGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,"), 3.0);
        QVERIFY(near(*nav.data().lat, 53 + 21.6802 / 60, 9));
    }

    // ---------- Спутники ----------
    void satellitesFromGsvAndGsa()
    {
        SatelliteTracker t;
        t.handle(nmeaMsg("GPGSV,1,1,03,01,40,083,46,40,30,100,38,05,10,200,,1"), 0.0);
        t.handle(nmeaMsg("GPGSV,1,1,01,01,40,083,30,7"), 0.1);
        t.handle(nmeaMsg("GNGSA,A,3,01,,,,,,,,,,,,1.50,0.90,1.20,1"), 0.2);
        const SatelliteSnapshot snap = t.snapshot(1.0);
        QCOMPARE(snap.sats.size(), 3);
        QCOMPARE(snap.sats[0].label(), QStringLiteral("G1"));
        QCOMPARE(*snap.sats[0].cno, 46);                        // лучший сигнал эпохи
        QCOMPARE(snap.sats[2].label(), QStringLiteral("S40"));  // SBAS по номеру
        QVERIFY(snap.used.contains({'G', 1}));
        const auto counts = countBySystem(snap.sats, snap.used);
        QCOMPARE(counts.value('G'), std::make_pair(1, 2));
        QCOMPARE(counts.value('S'), std::make_pair(0, 1));
        QVERIFY(t.snapshot(10.0).sats.isEmpty());  // устаревшие удаляются
    }

    // ---------- NAV-SAT, NAV-DOP, CFG-CFG ----------
    void decodeNavSatReference()
    {
        const GnssMessage msg = ubxMsg(NavSatHex);
        QCOMPARE(msg.identity, QStringLiteral("NAV-SAT"));
        const auto *s = msg.get<ubx::NavSat>();
        QVERIFY(s);
        QCOMPARE(s->svs.size(), 3);
        QCOMPARE(s->svs[0].sys, 'G');
        QCOMPARE(s->svs[0].svid, 5);
        QCOMPARE(*s->svs[0].cno, 42);
        QCOMPARE(*s->svs[0].elv, 45);
        QCOMPARE(*s->svs[0].az, 270);
        QVERIFY(s->svs[0].used);
        QCOMPARE(s->svs[1].sys, 'R');
        QVERIFY(!s->svs[1].cno && !s->svs[1].elv && !s->svs[1].az);
        QCOMPARE(s->svs[2].sys, 'S');
        QCOMPARE(s->svs[2].svid, 124);
        QVERIFY(!s->svs[2].used);
    }

    void decodeNavDopReference()
    {
        const GnssMessage msg = ubxMsg(NavDopHex);
        const auto *d = msg.get<ubx::NavDop>();
        QVERIFY(d);
        QVERIFY(near(d->hDop, 0.85, 9));
        QVERIFY(near(d->pDop, 1.5, 9));
        NavigationState nav;
        QVERIFY(nav.apply(msg, 0.0));
        QVERIFY(near(*nav.data().hdop, 0.85, 9));  // HDOP без NMEA
    }

    void factoryResetFrame()
    {
        // clearMask = loadMask = 0x0000FFFF, saveMask = 0, deviceMask = BBR | Flash | EEPROM | SPI Flash
        QCOMPARE(receiver_config::buildFactoryReset().toHex(),
                 QByteArray("b56206090d00ffff000000000000ffff0000172fae"));
        const GnssMessage ack = ubx::message(ubx::frame(ubx::ClassAck, ubx::IdAckAck, QByteArray::fromHex("0609")));
        QCOMPARE(receiver_config::commandAck(ack),
                 std::optional(std::pair{receiver_config::Command::FactoryReset, true}));
    }

    void valsetToFlash()
    {
        const QByteArray frame = receiver_config::buildValset({{receiver_config::NavSatUsbKey, 1}}, true);
        QCOMPARE(quint8(frame[7]), quint8(ubx::LayerRam | ubx::LayerBbr | ubx::LayerFlash));
    }

    void navSatHasPriorityOverNmea()
    {
        SatelliteTracker t;
        t.handle(nmeaMsg("GPGSV,1,1,01,07,40,083,46,1"), 0.0);
        t.handle(ubxMsg(NavSatHex), 0.5);
        t.handle(nmeaMsg("GPGSV,1,1,01,09,40,083,46,1"), 1.0);  // игнорируется, пока NAV-SAT свежий
        SatelliteSnapshot snap = t.snapshot(1.0);
        QCOMPARE(snap.sats.size(), 3);  // только из NAV-SAT
        QVERIFY(snap.used.contains({'G', 5}));
        QVERIFY(!snap.used.contains({'S', 124}));
        // NAV-SAT пропал — снова работает NMEA
        t.handle(nmeaMsg("GPGSV,1,1,01,09,40,083,46,1"), 3.0);
        snap = t.snapshot(3.0);
        QVERIFY(std::any_of(snap.sats.cbegin(), snap.sats.cend(), [](const Satellite &s) { return s.svid == 9; }));
    }

    // ---------- Воспроизведение ----------
    void replayEpochsFromNmeaTime()
    {
        FrameReader reader;
        const QList<GnssMessage> msgs =
            reader.feed(nmeaLine("GNGSA,A,3,01,,,,,,,,,,,,1.50,0.90,1.20,1") + ggaAt("120000.00")
                        + nmeaLine("GNGSA,A,3,01,,,,,,,,,,,,1.50,0.90,1.20,1") + ggaAt("120000.10") + ggaAt("120000.20")
                        + ggaAt("120500.00"));
        const QList<ReplayEpoch> epochs = replayEpochs(msgs);
        QCOMPARE(epochs.size(), 4);
        QCOMPARE(epochs[0].first, 0);
        QCOMPARE(epochs[0].count, 3);  // GSA до первого времени входит в первую эпоху
        QVERIFY(near(epochs[0].delay, 0.1, 6));
        QVERIFY(near(epochs[2].delay, ReplayWorker::DefaultDelay, 6));  // разрыв записи не ждём
    }

    void replayEpochsWithoutTime()
    {
        QList<GnssMessage> msgs;
        for (int i = 0; i < 45; ++i)
            msgs << nmeaMsg("GPGSV,1,1,01,07,40,083,46,1");
        const QList<ReplayEpoch> epochs = replayEpochs(msgs);
        QCOMPARE(epochs.size(), 3);
        QCOMPARE(epochs.last().count, 5);
    }

    // ---------- Устойчивость разбора ----------
    // Случайные искажения настоящего потока: разбор не должен падать или читать за границы
    // (смысл проверки — под AddressSanitizer/UBSan; глубокий поиск — fuzz/fuzz_frame_reader.cpp)
    // Эталонная запись для замеров: каждый кадр разбирается, решение и спутники — как в записи
    void referenceRecordingDecodes()
    {
        const int epochs = 20;
        const QByteArray raw = recording::reference(epochs);
        QCOMPARE(raw, recording::reference(epochs));  // детерминированная
        FrameReader reader;
        const QList<GnssMessage> msgs = reader.feed(raw);
        // В эпохе: NAV-PVT, NAV-DOP, NAV-SAT, RMC, GGA, 4 GSA и 3 + 2 + 2 + 2 страницы GSV
        QCOMPARE(msgs.size(), epochs * 18);
        NavigationState nav;
        SatelliteTracker tracker;
        for (const GnssMessage &msg : msgs) {
            QVERIFY2(!std::holds_alternative<std::monostate>(msg.data), qPrintable(msg.identity));
            nav.apply(msg, 0.0);
            tracker.handle(msg, 0.0);
        }
        QCOMPARE(nav.data().fix.value_or(QString()), QStringLiteral("3D"));
        QVERIFY(std::abs(*nav.data().lat - 55.75) < 0.001 && std::abs(*nav.data().lon - 37.62) < 0.001);
        QCOMPARE(tracker.snapshot(0.0).sats.size(), 32);
        QCOMPARE(replayEpochs(msgs).size(), epochs);
    }

    void frameReaderSurvivesMutations()
    {
        const QByteArray seed = ggaAt("092750.000") + QByteArray::fromHex(NavPvtHex) + QByteArray::fromHex(NavSatHex)
                                + QByteArray::fromHex(NavDopHex)
                                + nmeaLine("GPGSV,3,1,10,01,40,083,46,02,17,308,,03,07,344,,04,22,228,41,1")
                                + QByteArray::fromHex("b56205010200068a98c1");
        QRandomGenerator rng(12345);
        int frames = 0;
        for (int round = 0; round < 3000; ++round) {
            QByteArray data = seed;
            const int edits = int(rng.bounded(1, 12));
            for (int e = 0; e < edits; ++e) {
                const auto at = qsizetype(rng.bounded(int(data.size())));
                switch (rng.bounded(3)) {
                case 0: data[at] = char(rng.bounded(256)); break;
                case 1: data.remove(at, 1 + rng.bounded(8)); break;
                default: data.insert(at, char(rng.bounded(256))); break;
                }
                if (data.isEmpty())
                    data = seed;
            }
            // Длина UBX-кадра из искажённого поля тоже проверяется
            if (round % 7 == 0 && data.size() > 6)
                data[4] = char(0xFF);
            FrameReader reader;
            for (qsizetype i = 0; i < data.size();) {
                const qsizetype n = 1 + rng.bounded(64);
                for (const GnssMessage &m : reader.feed(data.mid(i, n))) {
                    QVERIFY(!messages::messageText(m).isEmpty());
                    ++frames;
                }
                i += n;
            }
        }
        QVERIFY(frames > 0);
    }
};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
