#include "ui/imagery_picker.h"

#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>

#include <algorithm>
#include <functional>

namespace {

const char *const Setting = "azimuth/map_source";
const char *const GoogleAcceptedSetting = "azimuth/google_terms_accepted";

// (подпись, источник тайлов); первый — по умолчанию
const QList<std::pair<const char *, std::reference_wrapper<const TileSource>>> &choices()
{
    static const QList<std::pair<const char *, std::reference_wrapper<const TileSource>>> list = {
        {QT_TRANSLATE_NOOP("ImageryPicker", "Satellite — Esri"), std::cref(tiles::esriImagery())},
        {QT_TRANSLATE_NOOP("ImageryPicker", "Map — OpenStreetMap"), std::cref(tiles::osm())},
        {QT_TRANSLATE_NOOP("ImageryPicker", "Satellite — Google (personal use)"), std::cref(tiles::googleImagery())},
    };
    return list;
}

}  // namespace

ImageryPicker::ImageryPicker(QWidget *parent)
    : QComboBox(parent)
{
    for (const auto &[label, source] : choices())
        addItem(tr(label), source.get().cacheDir);
    setToolTip(tr("Imagery provider. If tiles stay empty, the provider\n"
                  "may be blocked on this network — pick another one."));
    QSettings settings;
    const QString saved = settings.value(QLatin1String(Setting), choices().first().second.get().cacheDir).toString();
    // Google, выбранный до появления этого вопроса, — выбор сознательный: не спрашиваем ещё раз
    if (saved == tiles::googleImagery().cacheDir && !settings.contains(QLatin1String(GoogleAcceptedSetting)))
        settings.setValue(QLatin1String(GoogleAcceptedSetting), true);
    setCurrentIndex(std::max(0, findData(saved)));
    m_previous = currentIndex();
    connect(this, &QComboBox::currentIndexChanged, this, &ImageryPicker::onChanged);
}

const TileSource &ImageryPicker::source() const
{
    return choices()[std::max(0, currentIndex())].second.get();
}

void ImageryPicker::onChanged(int index)
{
    if (source() == tiles::googleImagery() && !confirmGoogle()) {
        const QSignalBlocker block(this);
        setCurrentIndex(m_previous);
        return;
    }
    m_previous = index;
    QSettings().setValue(QLatin1String(Setting), currentData());
    emit sourceChanged(source());
}

bool ImageryPicker::confirmGoogle()
{
    QSettings settings;
    if (settings.value(QLatin1String(GoogleAcceptedSetting), false).toBool())
        return true;
    const auto answer = QMessageBox::question(
        this, tr("Google satellite imagery"),
        tr("Google's terms allow this imagery only for personal, non-commercial use and do not allow apps to "
           "load the tiles directly.\n\nUse it only when other imagery does not load, at your own "
           "responsibility. Show Google imagery?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return false;
    settings.setValue(QLatin1String(GoogleAcceptedSetting), true);
    return true;
}
