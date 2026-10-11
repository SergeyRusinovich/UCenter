#include "ui/dashboard_tab.h"

#include "core/format.h"
#include "core/timeutil.h"
#include "ui/compass_view.h"
#include "ui/map_view.h"
#include "ui/style.h"

#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include <functional>

namespace {

const char *const OffsetSetting = "dashboard/utc_offset";

// (подпись, форматирование поля NavData для показа)
const QList<std::pair<const char *, std::function<QString(const NavData &)>>> &fields()
{
    static const QList<std::pair<const char *, std::function<QString(const NavData &)>>> list = {
        {QT_TRANSLATE_NOOP("DashboardTab", "Latitude"), [](const NavData &d) { return fmt::orEmpty(d.lat, 7); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "Longitude"), [](const NavData &d) { return fmt::orEmpty(d.lon, 7); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "Altitude (m)"), [](const NavData &d) { return fmt::orEmpty(d.altM, 1); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "UTC Time"),
         [](const NavData &d) { return d.utcSeconds ? timeutil::formatHms(*d.utcSeconds) : fmt::Empty; }},
        {QT_TRANSLATE_NOOP("DashboardTab", "Fix Type"),
         [](const NavData &d) { return d.fix && !d.fix->isEmpty() ? *d.fix : fmt::Empty; }},
        {QT_TRANSLATE_NOOP("DashboardTab", "PDOP"), [](const NavData &d) { return fmt::orEmpty(d.pdop, 2); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "HDOP"), [](const NavData &d) { return fmt::orEmpty(d.hdop, 2); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "SVs used"), [](const NavData &d) { return fmt::orEmpty(d.numSv); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "Speed (km/h)"),
         [](const NavData &d) { return fmt::orEmpty(d.speedKmh, 1); }},
        {QT_TRANSLATE_NOOP("DashboardTab", "Course (COG)"),
         [](const NavData &d) { return d.cog ? fmt::fixed(*d.cog, 1) + u'°' : fmt::Empty; }},
    };
    return list;
}

}  // namespace

DashboardTab::DashboardTab(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QHBoxLayout(this);

    auto *left = new QVBoxLayout;
    left->addWidget(buildValues());
    left->addWidget(buildTimeCorrection());
    auto *compassBox = new QGroupBox(tr("Compass"));
    auto *cl = new QVBoxLayout(compassBox);
    m_compass = new CompassView;
    cl->addWidget(m_compass);
    left->addWidget(compassBox, 1);
    auto *leftW = new QWidget;
    leftW->setLayout(left);
    leftW->setFixedWidth(320);

    auto *mapBox = new QGroupBox(tr("Map View"));
    auto *ml = new QVBoxLayout(mapBox);
    m_map = new MapView;
    ml->addWidget(m_map);

    root->addWidget(leftW);
    root->addWidget(mapBox, 1);
}

QGroupBox *DashboardTab::buildValues()
{
    auto *box = new QGroupBox(tr("Data View"));
    auto *grid = new QGridLayout(box);
    for (int i = 0; i < fields().size(); ++i) {
        QLabel *val = style::roleLabel(fmt::Empty, "value");
        grid->addWidget(style::roleLabel(tr(fields()[i].first), "caption"), i, 0);
        grid->addWidget(val, i, 1);
        m_labels.append(val);
    }
    grid->setColumnStretch(1, 1);
    return box;
}

QGroupBox *DashboardTab::buildTimeCorrection()
{
    auto *box = new QGroupBox(tr("UTC correction"));
    auto *lay = new QGridLayout(box);
    m_offsetCombo = new QComboBox;
    for (double hours : timeutil::utcOffsets())
        m_offsetCombo->addItem(timeutil::offsetLabel(hours), hours);
    // По умолчанию — часовой пояс системы, затем — выбранный в прошлый раз
    const double offset = QSettings().value(QLatin1String(OffsetSetting), timeutil::systemUtcOffset()).toDouble();
    const int idx = m_offsetCombo->findData(offset);
    m_offsetCombo->setCurrentIndex(idx >= 0 ? idx : m_offsetCombo->findData(timeutil::systemUtcOffset()));
    connect(m_offsetCombo, &QComboBox::currentIndexChanged, this, [this] {
        QSettings().setValue(QLatin1String(OffsetSetting), m_offsetCombo->currentData());
        updateLocalTime();
    });
    m_localLabel = style::roleLabel(fmt::Empty, "value");
    lay->addWidget(style::roleLabel(tr("Offset"), "caption"), 0, 0);
    lay->addWidget(m_offsetCombo, 0, 1);
    lay->addWidget(style::roleLabel(tr("Local time"), "caption"), 1, 0);
    lay->addWidget(m_localLabel, 1, 1);
    return box;
}

void DashboardTab::updateLocalTime()
{
    if (!m_utcSeconds) {
        m_localLabel->setText(fmt::Empty);
        return;
    }
    const int local = timeutil::toLocal(*m_utcSeconds, m_offsetCombo->currentData().toDouble());
    m_localLabel->setText(timeutil::formatHms(local));
}

void DashboardTab::updateState(const NavData &data)
{
    for (int i = 0; i < fields().size(); ++i)
        m_labels[i]->setText(fields()[i].second(data));
    m_utcSeconds = data.utcSeconds;
    updateLocalTime();
    m_map->setPosition(data.lat, data.lon);
    m_compass->setState(data.headingSupported, data.heading, data.cog);
}
