#include "ui/console_tab.h"

#include "ui/console_model.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QListView>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const char *const NmeaOnlySetting = "console/nmea_only";
const char *const UbxOnlySetting = "console/ubx_only";

}  // namespace

ConsoleTab::ConsoleTab(QWidget *parent)
    : QWidget(parent)
    , m_model(new ConsoleModel(Capacity, this))
{
    auto *lay = new QVBoxLayout(this);
    auto *row = new QHBoxLayout;
    m_freezeCb = new QCheckBox(tr("Freeze"));
    m_nmeaOnlyCb = new QCheckBox(tr("NMEA only"));
    m_ubxOnlyCb = new QCheckBox(tr("UBX only"));
    const QSettings settings;
    m_nmeaOnlyCb->setChecked(settings.value(QLatin1String(NmeaOnlySetting), false).toBool());
    m_ubxOnlyCb->setChecked(!m_nmeaOnlyCb->isChecked()
                            && settings.value(QLatin1String(UbxOnlySetting), false).toBool());
    auto *clearBtn = new QPushButton(tr("Clear"));
    // Фильтры взаимоисключающие и сразу применяются ко всей истории
    connect(m_nmeaOnlyCb, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            m_ubxOnlyCb->setChecked(false);
        applyFilter();
    });
    connect(m_ubxOnlyCb, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            m_nmeaOnlyCb->setChecked(false);
        applyFilter();
    });
    for (QCheckBox *c : {m_freezeCb, m_nmeaOnlyCb, m_ubxOnlyCb})
        row->addWidget(c);
    row->addStretch();
    row->addWidget(clearBtn);

    // Системный моноширинный шрифт: Menlo/SF Mono на macOS, Consolas на Windows
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_view = new QListView;
    m_view->setObjectName(QStringLiteral("console"));
    m_view->setModel(m_model);
    m_view->setFont(mono);
    m_view->setUniformItemSizes(true);  // высота строк одна: виду не нужно измерять каждую
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setTextElideMode(Qt::ElideRight);
    auto *copy = new QAction(tr("Copy"), m_view);
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetShortcut);
    connect(copy, &QAction::triggered, this, &ConsoleTab::copySelection);
    m_view->addAction(copy);
    m_view->setContextMenuPolicy(Qt::ActionsContextMenu);
    // Esc снимает выделение — и список снова едет за новыми строками
    auto *deselect = new QShortcut(QKeySequence(Qt::Key_Escape), m_view, nullptr, nullptr, Qt::WidgetShortcut);
    connect(deselect, &QShortcut::activated, this, [this] {
        m_view->clearSelection();
        m_view->scrollToBottom();
    });
    connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &current) { showDetails(current.isValid() ? current.row() : -1); });

    m_details = new QPlainTextEdit;
    m_details->setObjectName(QStringLiteral("console"));
    m_details->setReadOnly(true);
    m_details->setFont(mono);
    m_details->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_details->setPlaceholderText(tr("Select a message to see its fields"));

    auto *split = new QSplitter(Qt::Vertical);
    split->addWidget(m_view);
    split->addWidget(m_details);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 1);
    split->setChildrenCollapsible(false);

    connect(clearBtn, &QPushButton::clicked, this, &ConsoleTab::clear);
    lay->addLayout(row);
    lay->addWidget(split);
    applyFilter();
}

void ConsoleTab::applyFilter()
{
    m_model->setFilter(m_nmeaOnlyCb->isChecked()  ? MessageLog::Filter::Nmea
                       : m_ubxOnlyCb->isChecked() ? MessageLog::Filter::Ubx
                                                  : MessageLog::Filter::All);
    m_view->scrollToBottom();
    saveFilters();
}

void ConsoleTab::saveFilters() const
{
    QSettings settings;
    settings.setValue(QLatin1String(NmeaOnlySetting), m_nmeaOnlyCb->isChecked());
    settings.setValue(QLatin1String(UbxOnlySetting), m_ubxOnlyCb->isChecked());
}

void ConsoleTab::appendMessage(const GnssMessage &msg)
{
    if (m_freezeCb->isChecked())
        return;
    m_pending.append(msg);
    if (m_pending.size() > Capacity)
        m_pending.removeFirst();
}

void ConsoleTab::flush()
{
    if (m_pending.isEmpty() || (m_lastFlush.isValid() && m_lastFlush.elapsed() < FlushIntervalMs))
        return;
    m_lastFlush.start();
    // Список едет за новыми строками, пока он прокручен до конца и в нём ничего не выбрано:
    // выбранная строка не должна уезжать, пока читают её поля
    const QScrollBar *bar = m_view->verticalScrollBar();
    const bool follow = bar->value() == bar->maximum() && !m_view->selectionModel()->hasSelection();
    const qsizetype evicted = m_model->append(m_pending);
    m_pending.clear();
    if (follow) {
        m_view->scrollToBottom();
    } else if (evicted > 0) {
        // Строки ушли сверху — сдвигаем прокрутку, чтобы видимые строки остались на месте
        const int step = m_view->verticalScrollMode() == QAbstractItemView::ScrollPerItem
                             ? 1
                             : std::max(1, m_view->sizeHintForRow(0));
        m_view->verticalScrollBar()->setValue(int(bar->value() - evicted * step));
    }
}

void ConsoleTab::clear()
{
    m_pending.clear();
    m_model->clear();
    m_details->clear();
}

void ConsoleTab::showDetails(int row)
{
    if (row < 0) {
        m_details->clear();
        return;
    }
    const auto rows = messages::details(m_model->message(row));
    qsizetype width = 0;
    for (const auto &[name, value] : rows)
        width = std::max(width, name.size());
    QStringList lines;
    for (const auto &[name, value] : rows) {
        // Многострочное значение (кадр в hex) — с отступом под первой строкой
        const QString indent(width + 2, u' ');
        lines << name.leftJustified(width + 2) + QString(value).replace(u'\n', u'\n' + indent);
    }
    m_details->setPlainText(lines.join(u'\n'));
}

void ConsoleTab::copySelection() const
{
    QModelIndexList rows = m_view->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end());
    QStringList lines;
    for (const QModelIndex &index : std::as_const(rows))
        lines << index.data().toString();
    if (!lines.isEmpty())
        QApplication::clipboard()->setText(lines.join(u'\n'));
}
