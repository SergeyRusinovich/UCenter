#include "ui/satellites_tab.h"

#include "ui/filters.h"
#include "ui/satellite_views.h"
#include "ui/style.h"
#include "ui/theme.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

SatellitesTab::SatellitesTab(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);

    // --- Sky view + боковая панель фильтров ---
    auto *skyBox = new QGroupBox(tr("Satellite Position View"));
    auto *skyLay = new QHBoxLayout(skyBox);
    m_sky = new SkyView;
    m_skyFilters = new ConstellationFilters(true, true);
    connect(m_skyFilters, &ConstellationFilters::changed, this, &SatellitesTab::redraw);
    auto *side = new QVBoxLayout;
    side->addWidget(m_skyFilters);
    side->addLayout(buildZoom());
    side->addStretch();
    auto *sideW = new QWidget;
    sideW->setLayout(side);
    sideW->setFixedWidth(220);
    skyLay->addWidget(m_sky, 1);
    skyLay->addWidget(sideW);

    // --- Signal view + строка фильтров под диаграммой ---
    auto *sigBox = new QGroupBox(tr("Satellite Signal View"));
    auto *sigLay = new QVBoxLayout(sigBox);
    m_signal = new SignalView;
    m_signalFilters = new ConstellationFilters(false, false);
    connect(m_signalFilters, &ConstellationFilters::changed, this, &SatellitesTab::redraw);
    QLabel *legend =
        style::roleLabel(QStringLiteral("<span style=\"color:%1\">&#9633;</span> %2")
                             .arg(theme::notUsedColor().name(), tr("Not used in navigation").toHtmlEscaped()),
                         "caption");
    sigLay->addWidget(m_signal, 1);
    sigLay->addWidget(m_signalFilters);
    sigLay->addWidget(legend);

    lay->addWidget(skyBox, 3);
    lay->addWidget(sigBox, 2);
    redraw();
}

QVBoxLayout *SatellitesTab::buildZoom()
{
    auto *lay = new QVBoxLayout;
    m_zoomLabel = new QLabel;
    lay->addWidget(style::roleLabel(tr("Marker size"), "section"));
    m_zoom = new QSlider(Qt::Horizontal);  // размер маркеров на карте
    m_zoom->setRange(1, 14);
    m_zoom->setValue(8);
    connect(m_zoom, &QSlider::valueChanged, this, &SatellitesTab::redraw);
    lay->addWidget(m_zoom);
    lay->addWidget(m_zoomLabel);
    return lay;
}

void SatellitesTab::updateData(const SatelliteSnapshot &snapshot)
{
    m_data = snapshot;
    m_skyFilters->setCounts(countBySystem(snapshot.sats, snapshot.used));  // счётчики до фильтрации
    redraw();
}

void SatellitesTab::redraw()
{
    const int z = m_zoom->value();
    m_zoomLabel->setText(QString::number(z));
    QList<Satellite> sky, signal;
    for (const Satellite &s : std::as_const(m_data.sats)) {
        if (m_skyFilters->accepts(s))
            sky << s;
        if (m_signalFilters->accepts(s))
            signal << s;
    }
    m_sky->setData(sky, m_data.used, 6 + z * 0.8);
    m_signal->setData(signal, m_data.used);
}
