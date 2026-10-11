#include "ui/imagery_picker.h"

#include <QSettings>

#include <algorithm>
#include <functional>

namespace {

const char *const Setting = "azimuth/map_source";

// (подпись, источник тайлов); первый — по умолчанию
const QList<std::pair<const char *, std::reference_wrapper<const TileSource>>> &choices()
{
    static const QList<std::pair<const char *, std::reference_wrapper<const TileSource>>> list = {
        {QT_TRANSLATE_NOOP("ImageryPicker", "Satellite — Esri"), std::cref(tiles::esriImagery())},
        {QT_TRANSLATE_NOOP("ImageryPicker", "Satellite — Google"), std::cref(tiles::googleImagery())},
        {QT_TRANSLATE_NOOP("ImageryPicker", "Map — OpenStreetMap"), std::cref(tiles::osm())},
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
    const QString saved = QSettings().value(QLatin1String(Setting), choices().first().second.get().cacheDir).toString();
    setCurrentIndex(std::max(0, findData(saved)));
    connect(this, &QComboBox::currentIndexChanged, this, &ImageryPicker::onChanged);
}

const TileSource &ImageryPicker::source() const
{
    return choices()[std::max(0, currentIndex())].second.get();
}

void ImageryPicker::onChanged(int)
{
    QSettings().setValue(QLatin1String(Setting), currentData());
    emit sourceChanged(source());
}
