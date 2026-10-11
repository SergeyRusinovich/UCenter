#include "ui/saved_measurements.h"

#include "core/baseline_store.h"
#include "core/format.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace {

const char *const StoreFile = "measurements.json";

QString latLon(double lat, double lon)
{
    return QStringLiteral("%1, %2").arg(fmt::fixed(lat, 7), fmt::fixed(lon, 7));
}

// (заголовок столбца, текст ячейки)
const QList<std::pair<const char *, std::function<QString(const Baseline &)>>> &tableColumns()
{
    static const QList<std::pair<const char *, std::function<QString(const Baseline &)>>> list = {
        {QT_TRANSLATE_NOOP("SavedMeasurementsPanel", "Saved"), [](const Baseline &m) { return m.savedAt; }},
        {QT_TRANSLATE_NOOP("SavedMeasurementsPanel", "A (lat, lon)"),
         [](const Baseline &m) { return latLon(m.aLat, m.aLon); }},
        {QT_TRANSLATE_NOOP("SavedMeasurementsPanel", "B (lat, lon)"),
         [](const Baseline &m) { return latLon(m.bLat, m.bLon); }},
        {QT_TRANSLATE_NOOP("SavedMeasurementsPanel", "Azimuth"),
         [](const Baseline &m) { return formatAzimuth(m.azimuth); }},
        {QT_TRANSLATE_NOOP("SavedMeasurementsPanel", "Deviation"),
         [](const Baseline &m) { return formatDeviation(m.deviation()); }},
        {QT_TRANSLATE_NOOP("SavedMeasurementsPanel", "Distance (m)"),
         [](const Baseline &m) { return fmt::fixed(m.distanceM, 3); }},
    };
    return list;
}

QString defaultStorePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString path = QDir(base).filePath(QLatin1String(StoreFile));
#ifdef Q_OS_WIN
    // До 1.0.0 организация в Windows не задавалась (ADR 0016): каталог данных был на уровень выше.
    // Не удалось перенести — работаем с прежним файлом, чтобы замеры не пропали
    const QString legacy = QFileInfo(base).dir().filePath(QLatin1String(StoreFile));
    if (!baseline_store::moveFromLegacy(legacy, path).isEmpty())
        return legacy;
#endif
    return path;
}

}  // namespace

SavedMeasurementsPanel::SavedMeasurementsPanel(const QString &storePath, QWidget *parent)
    : QGroupBox(tr("Saved measurements"), parent)
    , m_storePath(storePath.isEmpty() ? defaultStorePath() : storePath)
{
    auto *lay = new QVBoxLayout(this);
    m_table = new QTableWidget(0, int(tableColumns().size()));
    QStringList headers;
    for (const auto &[name, _] : tableColumns())
        headers << tr(name);
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setToolTip(tr("Double-click a row to show it on the map"));
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { requestShow(row); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &SavedMeasurementsPanel::updateButtons);
    lay->addWidget(m_table);

    auto *buttons = new QHBoxLayout;
    m_show = new QPushButton(tr("Show on map"));
    connect(m_show, &QPushButton::clicked, this, [this] { requestShow(m_table->currentRow()); });
    m_delete = new QPushButton(tr("Delete"));
    m_delete->setProperty("variant", QStringLiteral("danger"));
    connect(m_delete, &QPushButton::clicked, this, &SavedMeasurementsPanel::deleteSelected);
    m_export = new QPushButton(tr("Export CSV…"));
    connect(m_export, &QPushButton::clicked, this, &SavedMeasurementsPanel::exportCsv);
    buttons->addWidget(m_show);
    buttons->addWidget(m_delete);
    buttons->addStretch();
    buttons->addWidget(m_export);
    lay->addLayout(buttons);

    load();
}

void SavedMeasurementsPanel::add(const Baseline &item)
{
    m_items.append(item);
    store();  // при ошибке запись остаётся в таблице до закрытия программы
    fill();
    m_table->scrollToBottom();
}

void SavedMeasurementsPanel::requestShow(int row)
{
    if (row >= 0 && row < m_items.size())
        emit showRequested(m_items[row]);
}

void SavedMeasurementsPanel::deleteSelected()
{
    QSet<int> rowSet;
    for (const QTableWidgetItem *item : m_table->selectedItems())
        rowSet.insert(item->row());
    QList<int> rows(rowSet.cbegin(), rowSet.cend());
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (int row : rows)
        m_items.removeAt(row);
    store();
    fill();
}

void SavedMeasurementsPanel::fill()
{
    m_table->setRowCount(int(m_items.size()));
    for (int row = 0; row < m_items.size(); ++row)
        for (int col = 0; col < tableColumns().size(); ++col)
            m_table->setItem(row, col, new QTableWidgetItem(tableColumns()[col].second(m_items[row])));
    updateButtons();
}

void SavedMeasurementsPanel::updateButtons()
{
    const bool selected = m_table->currentRow() >= 0 && !m_table->selectedItems().isEmpty();
    m_show->setEnabled(selected);
    m_delete->setEnabled(selected);
    m_export->setEnabled(!m_items.isEmpty());
}

void SavedMeasurementsPanel::load()
{
    const QString error = baseline_store::loadJson(m_storePath, &m_items);
    if (!error.isEmpty())
        QMessageBox::warning(this, tr("Saved measurements"), tr("Cannot read %1:\n%2").arg(m_storePath, error));
    fill();
}

void SavedMeasurementsPanel::store()
{
    const QString error = baseline_store::saveJson(m_storePath, m_items);
    if (!error.isEmpty())
        QMessageBox::warning(this, tr("Saved measurements"), tr("Cannot write %1:\n%2").arg(m_storePath, error));
}

void SavedMeasurementsPanel::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export measurements"),
                                                      QStringLiteral("measurements.csv"), tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    const QString error = baseline_store::writeCsv(path, m_items);
    if (!error.isEmpty())
        QMessageBox::warning(this, tr("Export CSV"), tr("Cannot write %1:\n%2").arg(path, error));
}
