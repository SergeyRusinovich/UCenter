#include "ui/measurement_panels.h"

#include "core/format.h"
#include "ui/style.h"
#include "ui/value_label.h"

#include <QDoubleValidator>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>

#include <functional>

namespace {

// (подпись, форматирование результата для показа)
const QList<std::pair<const char *, std::function<QString(const Baseline &)>>> &results()
{
    static const QList<std::pair<const char *, std::function<QString(const Baseline &)>>> list = {
        {QT_TRANSLATE_NOOP("ResultPanel", "Azimuth A→B"), [](const Baseline &m) { return formatAzimuth(m.azimuth); }},
        {QT_TRANSLATE_NOOP("ResultPanel", "Deviation from true N"),
         [](const Baseline &m) { return formatDeviation(m.deviation()); }},
        {QT_TRANSLATE_NOOP("ResultPanel", "Back azimuth B→A"),
         [](const Baseline &m) { return formatAzimuth(m.backAzimuth); }},
        {QT_TRANSLATE_NOOP("ResultPanel", "Distance (m)"),
         [](const Baseline &m) { return fmt::fixed(m.distanceM, 3); }},
        {QT_TRANSLATE_NOOP("ResultPanel", "ΔNorth (m)"),
         [](const Baseline &m) { return fmt::signedFixed(m.dNorth(), 3); }},
        {QT_TRANSLATE_NOOP("ResultPanel", "ΔEast (m)"),
         [](const Baseline &m) { return fmt::signedFixed(m.dEast(), 3); }},
    };
    return list;
}

QLineEdit *coordEdit(double limit, const QString &placeholder)
{
    auto *edit = new QLineEdit;
    auto *validator = new QDoubleValidator(-limit, limit, 8, edit);
    validator->setNotation(QDoubleValidator::StandardNotation);
    validator->setLocale(QLocale::c());  // десятичная точка независимо от локали ОС
    edit->setValidator(validator);
    edit->setPlaceholderText(placeholder);
    return edit;
}

QPushButton *primaryButton(const QString &text)
{
    auto *btn = new QPushButton(text);
    btn->setProperty("variant", QStringLiteral("primary"));
    return btn;
}

constexpr size_t index(PointKey key)
{
    return key == PointKey::A ? 0 : 1;
}

}  // namespace

PointsEditor::PointsEditor(QWidget *parent)
    : QGroupBox(tr("Points"), parent)
{
    auto *grid = new QGridLayout(this);
    grid->addWidget(style::roleLabel(tr("Latitude"), "caption"), 0, 1);
    grid->addWidget(style::roleLabel(tr("Longitude"), "caption"), 0, 2);
    int row = 1;
    for (PointKey key : PointKeys) {
        QLineEdit *lat = coordEdit(90, QStringLiteral("lat"));
        QLineEdit *lon = coordEdit(180, QStringLiteral("lon"));
        for (QLineEdit *edit : {lat, lon})
            connect(edit, &QLineEdit::editingFinished, this, [this, key] { onEdit(key); });
        grid->addWidget(style::roleLabel(pointName(key), "section"), row, 0);
        grid->addWidget(lat, row, 1);
        grid->addWidget(lon, row, 2);
        m_edits[index(key)] = {lat, lon};
        ++row;
    }
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    m_device = style::roleLabel(QString(), "caption");
    m_device->hide();
    grid->addWidget(m_device, row, 0, 1, 3);
    m_hint = style::roleLabel(QString(), "caption");
    m_hint->setWordWrap(true);
    grid->addWidget(m_hint, row + 1, 0, 1, 3);
}

void PointsEditor::showPoint(PointKey key, std::optional<LatLon> ll, bool fromDevice)
{
    const auto [lat, lon] = m_edits[index(key)];
    const std::pair<QLineEdit *, double> fields[] = {{lat, ll ? ll->lat : 0.0}, {lon, ll ? ll->lon : 0.0}};
    for (const auto &[edit, value] : fields)
        if (!(fromDevice && edit->hasFocus()))
            edit->setText(ll ? fmt::fixed(value, 8) : QString());
}

void PointsEditor::setDeviceLocation(const QString &text)
{
    m_device->setText(text);
    m_device->setVisible(!text.isEmpty());
}

void PointsEditor::setHint(const QString &text)
{
    m_hint->setText(text);
}

void PointsEditor::onEdit(PointKey key)
{
    const auto [latE, lonE] = m_edits[index(key)];
    if (!latE->isModified() && !lonE->isModified())
        return;  // потеря фокуса без правки
    const QString latT = latE->text().trimmed(), lonT = lonE->text().trimmed();
    if (latT.isEmpty() && lonT.isEmpty()) {
        emit pointEdited(key, std::nullopt);
    } else if (!latT.isEmpty() && !lonT.isEmpty()) {
        bool okLat = false, okLon = false;
        const LatLon ll{latT.toDouble(&okLat), lonT.toDouble(&okLon)};
        if (okLat && okLon)
            emit pointEdited(key, ll);
    }
}

ResultPanel::ResultPanel(QWidget *parent)
    : QGroupBox(tr("Result (WGS84, true north)"), parent)
{
    auto *grid = new QGridLayout(this);
    for (int i = 0; i < results().size(); ++i) {
        auto *val = new ValueLabel;
        grid->addWidget(style::roleLabel(tr(results()[i].first), "caption"), i, 0);
        grid->addWidget(val, i, 1);
        m_labels.append(val);
    }
    grid->setColumnStretch(1, 1);

    auto *buttons = new QHBoxLayout;
    m_save = primaryButton(tr("Save"));
    m_save->setToolTip(tr("Add to saved measurements and start a new one"));
    connect(m_save, &QPushButton::clicked, this, &ResultPanel::saveRequested);
    auto *clear = new QPushButton(tr("Clear"));
    connect(clear, &QPushButton::clicked, this, &ResultPanel::clearRequested);
    buttons->addWidget(m_save);
    buttons->addWidget(clear);
    const int n = int(results().size());
    grid->setRowStretch(n, 1);  // свободное место — между значениями и кнопками
    grid->addLayout(buttons, n + 1, 0, 1, 2);
}

void ResultPanel::setResult(const std::optional<Baseline> &result)
{
    for (int i = 0; i < results().size(); ++i)
        m_labels[i]->setText(result ? results()[i].second(*result) : fmt::Empty);
    m_save->setEnabled(result.has_value());
}

ResultOverlay::ResultOverlay(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("mapOverlay"));
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 6, 6, 6);
    m_label = style::roleLabel(QString(), "value");
    m_save = primaryButton(tr("Save"));
    connect(m_save, &QPushButton::clicked, this, &ResultOverlay::saveRequested);
    lay->addWidget(m_label);
    lay->addWidget(m_save);
    move(8, 8);
    hide();
}

void ResultOverlay::setResult(const std::optional<Baseline> &r)
{
    if (!r) {
        m_label->setText(tr("Place points A and B"));
    } else {
        m_label->setText(
            tr("A→B %1   %2   %3 m")
                .arg(formatAzimuth(r->azimuth), formatDeviation(r->deviation()), fmt::fixed(r->distanceM, 1)));
    }
    m_save->setEnabled(r.has_value());
    adjustSize();
}
