#include "ui/console_tab.h"

#include <QCheckBox>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>

namespace {

const char *const NmeaOnlySetting = "console/nmea_only";
const char *const UbxOnlySetting = "console/ubx_only";

}  // namespace

ConsoleTab::ConsoleTab(QWidget *parent)
    : QWidget(parent)
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
    // Фильтры взаимоисключающие
    connect(m_nmeaOnlyCb, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            m_ubxOnlyCb->setChecked(false);
        saveFilters();
    });
    connect(m_ubxOnlyCb, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            m_nmeaOnlyCb->setChecked(false);
        saveFilters();
    });
    for (QCheckBox *c : {m_freezeCb, m_nmeaOnlyCb, m_ubxOnlyCb})
        row->addWidget(c);
    row->addStretch();
    row->addWidget(clearBtn);
    m_console = new QPlainTextEdit;
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(MaxLines);
    m_console->setObjectName(QStringLiteral("console"));
    // Системный моноширинный шрифт: Menlo/SF Mono на macOS, Consolas на Windows
    m_console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    connect(clearBtn, &QPushButton::clicked, this, &ConsoleTab::clear);
    lay->addLayout(row);
    lay->addWidget(m_console);
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
    const bool ubx = msg.isUbx();
    if ((m_nmeaOnlyCb->isChecked() && ubx) || (m_ubxOnlyCb->isChecked() && !ubx))
        return;
    m_pending.append(msg);
    if (m_pending.size() > MaxLines)
        m_pending.removeFirst();
}

void ConsoleTab::flush()
{
    if (m_pending.isEmpty() || !isVisible() || window()->isMinimized())
        return;
    if (m_lastFlush.isValid() && m_lastFlush.elapsed() < FlushIntervalMs)
        return;
    m_lastFlush.start();
    QStringList lines;
    lines.reserve(m_pending.size());
    for (const GnssMessage &msg : std::as_const(m_pending))
        lines << messages::messageText(msg);
    m_console->appendPlainText(lines.join(u'\n'));  // автоскролл вниз при добавлении
    m_pending.clear();
}

void ConsoleTab::clear()
{
    m_pending.clear();
    m_console->clear();
}
