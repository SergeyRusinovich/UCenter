#include "ui/azimuth_tab.h"

#include "core/format.h"
#include "ui/baseline_map_view.h"
#include "ui/imagery_picker.h"
#include "ui/measurement_panels.h"
#include "ui/place_search_bar.h"
#include "ui/saved_measurements.h"
#include "ui/style.h"

#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QSettings>
#include <QShortcut>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char *const SplitterSetting = "azimuth/splitter";
const char *const MapLatSetting = "azimuth/map_lat";
const char *const MapLonSetting = "azimuth/map_lon";
const char *const MapZoomSetting = "azimuth/map_zoom";

}  // namespace

AzimuthTab::AzimuthTab(QWidget *parent)
    : QWidget(parent)
    , m_location(new HostLocation(this))
{
    m_pointsEditor = new PointsEditor;
    m_resultPanel = new ResultPanel;
    m_map = new BaselineMapView;
    m_overlay = new ResultOverlay(m_map);
    m_searchBar = new PlaceSearchBar([this] { return m_map->visibleBounds(); });
    m_imagery = new ImageryPicker;
    m_saved = new SavedMeasurementsPanel;
    m_map->setSource(m_imagery->source());
    buildLayout();
    wire();
    restoreSettings();
    refresh();
}

void AzimuthTab::restoreSettings()
{
    const QSettings settings;
    m_splitter->restoreState(settings.value(QLatin1String(SplitterSetting)).toByteArray());
    const QVariant lat = settings.value(QLatin1String(MapLatSetting)),
                   lon = settings.value(QLatin1String(MapLonSetting));
    if (lat.isValid() && lon.isValid())  // вид карты прошлого сеанса; первая позиция компьютера всё равно центрирует
        m_map->setView({lat.toDouble(), lon.toDouble()}, settings.value(QLatin1String(MapZoomSetting)).toInt());
}

void AzimuthTab::saveSettings() const
{
    QSettings settings;
    settings.setValue(QLatin1String(SplitterSetting), m_splitter->saveState());
    settings.setValue(QLatin1String(MapLatSetting), m_map->center().lat);
    settings.setValue(QLatin1String(MapLonSetting), m_map->center().lon);
    settings.setValue(QLatin1String(MapZoomSetting), m_map->zoom());
}

void AzimuthTab::buildLayout()
{
    m_left = new QWidget;
    auto *left = new QVBoxLayout(m_left);
    left->setContentsMargins(0, 0, 0, 0);
    left->addWidget(m_pointsEditor);
    left->addWidget(m_resultPanel, 1);  // растягивается до таблицы сохранённых
    m_left->setFixedWidth(LeftWidth);

    auto *mapBox = new QGroupBox(tr("Map — click to place points"));
    auto *ml = new QVBoxLayout(mapBox);
    auto *imagery = new QWidget;  // подпись и список — по одной линии с полем поиска
    auto *il = new QHBoxLayout(imagery);
    il->setContentsMargins(0, 0, 0, 0);
    il->addWidget(style::roleLabel(tr("Imagery"), "caption"));
    il->addWidget(m_imagery);
    auto *tools = new QHBoxLayout;
    tools->addWidget(m_searchBar, 1);
    tools->addWidget(imagery, 0, Qt::AlignTop);
    ml->addLayout(tools);
    ml->addWidget(m_map, 1);

    auto *top = new QWidget;
    auto *tl = new QHBoxLayout(top);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->addWidget(m_left);
    tl->addWidget(mapBox, 1);

    m_splitter = new QSplitter(Qt::Vertical);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->addWidget(top);
    m_splitter->addWidget(m_saved);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 1);
    (new QVBoxLayout(this))->addWidget(m_splitter);

    m_esc = new QShortcut(QKeySequence(Qt::Key_Escape), this, [this] { setExpanded(false); });
    m_esc->setContext(Qt::WidgetWithChildrenShortcut);
    m_esc->setEnabled(false);
}

void AzimuthTab::wire()
{
    connect(m_location, &HostLocation::position, this, &AzimuthTab::onDevicePosition);
    connect(m_location, &HostLocation::status, this, &AzimuthTab::onLocationStatus);
    connect(m_map, &MapView::clicked, this, &AzimuthTab::onMapClick);
    connect(m_map, &BaselineMapView::pointMoved, this, &AzimuthTab::onMarkerMoved);
    connect(m_map, &BaselineMapView::placeMoved, m_searchBar, &PlaceSearchBar::movePlace);
    connect(m_map, &MapView::recentered, this, &AzimuthTab::attachA);
    connect(m_map, &BaselineMapView::expandToggled, this, &AzimuthTab::applyExpanded);
    connect(m_map, &BaselineMapView::locationRetryRequested, m_location, &HostLocation::retry);
    connect(m_pointsEditor, &PointsEditor::pointEdited, this, &AzimuthTab::onPointEdited);
    connect(m_resultPanel, &ResultPanel::saveRequested, this, &AzimuthTab::save);
    connect(m_resultPanel, &ResultPanel::clearRequested, this, &AzimuthTab::newMeasurement);
    connect(m_overlay, &ResultOverlay::saveRequested, this, &AzimuthTab::save);
    connect(m_searchBar, &PlaceSearchBar::placeChanged, this, &AzimuthTab::showPlace);
    connect(m_searchBar, &PlaceSearchBar::pointRequested, this,
            [this](PointKey key, double lat, double lon) { placePoint(key, LatLon{lat, lon}); });
    connect(m_imagery, &ImageryPicker::sourceChanged, m_map, &MapView::setSource);
    connect(m_saved, &SavedMeasurementsPanel::showRequested, this, &AzimuthTab::showSaved);
}

// ---------- Точки ----------
void AzimuthTab::sync(const QList<PointKey> &keys, bool fromDevice)
{
    for (PointKey key : keys) {
        m_pointsEditor->showPoint(key, m_points[key], fromDevice);
        m_map->setPoint(key, m_points[key]);
    }
    refresh();
}

void AzimuthTab::placePoint(PointKey key, std::optional<LatLon> ll)
{
    m_points.set(key, ll);
    m_map->stopFollowing();  // карта не уезжает за позицией, пока ставятся точки
    sync({key});
}

void AzimuthTab::onMapClick(double lat, double lon)
{
    if (m_points.click({lat, lon})) {
        m_map->stopFollowing();
        sync();
    }
}

void AzimuthTab::onMarkerMoved(PointKey key, double lat, double lon)
{
    m_points.set(key, LatLon{lat, lon});
    sync({key});
}

void AzimuthTab::onPointEdited(PointKey key, std::optional<LatLon> ll)
{
    placePoint(key, ll);
    if (ll)
        m_map->centerOn(ll->lat, ll->lon);
}

// «Прицел»: A снова следует за устройством; без позиции — объясняем почему
void AzimuthTab::attachA()
{
    if (m_points.attachA()) {
        sync({PointKey::A}, true);
    } else {
        refresh();
        m_map->showLocationHelp(m_locLevel, m_locText);
    }
}

void AzimuthTab::newMeasurement()
{
    m_points.newMeasurement();
    sync({PointKey::A, PointKey::B}, true);
}

void AzimuthTab::refresh()
{
    const auto result = m_points.result();
    m_resultPanel->setResult(result);
    m_overlay->setResult(result);
    m_pointsEditor->setHint(hint());
}

QString AzimuthTab::hint() const
{
    const auto a = m_points[PointKey::A], b = m_points[PointKey::B];
    QString text;
    if (!a)
        text = tr("Click the map to place point A (or enter coordinates).");
    else if (!b)
        text = tr("Click the map to place point B.");
    else
        text = tr("Drag a marker to adjust. Save to store the result and start a new measurement.");
    if (a && !m_points.aLive() && m_points.device())
        text += tr(" ⌖ on the map returns A to this computer's location.");
    return text;
}

// ---------- Геопозиция компьютера ----------
void AzimuthTab::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    m_location->start();  // запрос доступа — только когда вкладку открыли
}

// Проблемы с геопозицией показываются на «прицеле» карты
void AzimuthTab::onLocationStatus(LocationLevel level, const QString &text)
{
    m_locLevel = level;
    m_locText = text;
    m_map->setLocationAlert(level, text);
}

void AzimuthTab::onDevicePosition(double lat, double lon, std::optional<double> accuracy)
{
    const QString acc = accuracy ? tr(" ±%1 m").arg(fmt::fixed(*accuracy, 0)) : QString();
    m_pointsEditor->setDeviceLocation(tr("Device location: %1, %2%3").arg(fmt::fixed(lat, 7), fmt::fixed(lon, 7), acc));
    m_map->setAccuracy(accuracy);
    m_map->setPosition(lat, lon);
    if (m_points.deviceMoved({lat, lon}))
        sync({PointKey::A}, true);
    else
        refresh();  // подсказка зависит от того, известна ли позиция
}

// ---------- Найденное место ----------
void AzimuthTab::showPlace(const std::optional<Place> &place)
{
    m_map->setPlace(place);
    if (place)
        m_map->fitBounds(place->bbox);
}

// ---------- Сохранённые измерения ----------
void AzimuthTab::save()
{
    auto result = m_points.result();
    if (!result)
        return;
    result->savedAt = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    m_saved->add(*result);
    newMeasurement();
}

void AzimuthTab::showSaved(const Baseline &m)
{
    m_points.load(m.a(), m.b());
    sync();
    // Масштаб такой, чтобы отрезок A–B был виден целиком
    m_map->fitBounds(
        {std::min(m.aLat, m.bLat), std::min(m.aLon, m.bLon), std::max(m.aLat, m.bLat), std::max(m.aLon, m.bLon)});
}

// ---------- Карта во всё окно ----------
void AzimuthTab::setExpanded(bool on)
{
    if (m_map->isExpanded() != on) {
        m_map->setExpanded(on);
        applyExpanded(on);
    }
}

void AzimuthTab::applyExpanded(bool on)
{
    m_left->setVisible(!on);
    m_overlay->setVisible(on);
    m_esc->setEnabled(on);
    emit expandChanged(on);
}

void AzimuthTab::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    if (!e->spontaneous())  // ушли на другую вкладку (а не свернули окно) — обычный вид
        setExpanded(false);
}
