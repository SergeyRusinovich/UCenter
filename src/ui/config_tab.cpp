#include "ui/config_tab.h"

#include "core/receiver_config.h"
#include "ui/style.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace cfg = receiver_config;

ConfigTab::ConfigTab(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);

    auto *rateBox = new QGroupBox(tr("Navigation Rate (CFG-RATE-MEAS)"));
    auto *rl = new QHBoxLayout(rateBox);
    m_rateCombo = new QComboBox;
    for (const auto &[label, ms] : cfg::rateOptions())
        m_rateCombo->addItem(label, ms);
    auto *applyRateBtn = new QPushButton(tr("Apply"));
    connect(applyRateBtn, &QPushButton::clicked, this, &ConfigTab::applyRate);
    rl->addWidget(m_rateCombo);
    rl->addWidget(applyRateBtn);
    rl->addStretch();
    auto *readBtn = new QPushButton(tr("Read from receiver"));
    connect(readBtn, &QPushButton::clicked, this, &ConfigTab::refreshRequested);
    rl->addWidget(readBtn);

    auto *gnssBox = new QGroupBox(tr("Constellations (CFG-SIGNAL-*-ENA)"));
    auto *gl = new QVBoxLayout(gnssBox);
    for (const auto &[name, key] : cfg::constellationKeys()) {
        auto *cb = new QCheckBox(name);
        cb->setChecked(true);
        gl->addWidget(cb);
        m_gnssCbs.append({key, cb});
    }
    auto *applyGnss = new QPushButton(tr("Apply constellations"));
    connect(applyGnss, &QPushButton::clicked, this, &ConfigTab::applyConstellations);
    gl->addWidget(applyGnss);

    auto *msgBox = new QGroupBox(tr("Message output on USB"));
    auto *ml = new QVBoxLayout(msgBox);
    for (const auto &[name, key] : cfg::outputKeys()) {
        auto *cb = new QCheckBox(name);
        ml->addWidget(cb);
        m_outputCbs.append({key, cb});
    }
    QLabel *hint = style::roleLabel(
        tr("NAV-SAT gives satellites with an exact \"used\" flag, NAV-DOP gives HDOP: with them NMEA can be "
           "turned off to free the USB link."),
        "caption");
    hint->setWordWrap(true);  // в длинных переводах подсказка не должна раздвигать окно
    ml->addWidget(hint);
    m_outputCbs.last().second->setToolTip(tr("Without NMEA the Satellites tab needs UBX-NAV-SAT"));
    auto *applyOutputs = new QPushButton(tr("Apply output"));
    connect(applyOutputs, &QPushButton::clicked, this, &ConfigTab::applyOutputs);
    ml->addWidget(applyOutputs);

    auto *storageBox = new QGroupBox(tr("Storage"));
    auto *sl = new QVBoxLayout(storageBox);
    m_flashCb = new QCheckBox(tr("Also save changes to Flash (kept after a power loss)"));
    m_flashCb->setToolTip(tr("Changes always go to RAM and BBR; Flash keeps them even without a backup battery"));
    sl->addWidget(m_flashCb);
    auto *resetRow = new QHBoxLayout;
    auto *resetBtn = new QPushButton(tr("Revert to factory defaults…"));
    resetBtn->setProperty("variant", QStringLiteral("danger"));
    connect(resetBtn, &QPushButton::clicked, this, &ConfigTab::confirmFactoryReset);
    resetRow->addWidget(resetBtn);
    resetRow->addStretch();
    sl->addLayout(resetRow);

    m_buttons = {applyRateBtn, applyGnss, applyOutputs, readBtn, resetBtn};
    for (QGroupBox *b : {rateBox, gnssBox, msgBox, storageBox})
        lay->addWidget(b);
    lay->addStretch();
}

void ConfigTab::setConnected(bool on)
{
    for (QPushButton *b : std::as_const(m_buttons))
        b->setEnabled(on);
}

void ConfigTab::applyValues(const ubx::ConfigValues &values)
{
    if (values.contains(cfg::RateKey)) {
        const int idx = m_rateCombo->findData(int(values.value(cfg::RateKey)));
        if (idx >= 0)
            m_rateCombo->setCurrentIndex(idx);
    }
    for (const auto *list : {&m_gnssCbs, &m_outputCbs})
        for (const auto &[key, cb] : *list)
            if (values.contains(key))
                cb->setChecked(values.value(key) != 0);
}

void ConfigTab::applyRate()
{
    emit valsetRequested({{cfg::RateKey, quint64(m_rateCombo->currentData().toInt())}}, m_flashCb->isChecked());
}

void ConfigTab::applyConstellations()
{
    const bool any =
        std::any_of(m_gnssCbs.cbegin(), m_gnssCbs.cend(), [](const auto &item) { return item.second->isChecked(); });
    if (!any) {
        QMessageBox::warning(this, tr("Constellations"), tr("At least one must be enabled."));
        return;
    }
    ubx::ConfigItems items;
    for (const auto &[key, cb] : std::as_const(m_gnssCbs))
        items.append({key, cb->isChecked() ? 1 : 0});
    emit valsetRequested(items, m_flashCb->isChecked());
}

void ConfigTab::applyOutputs()
{
    const bool any = std::any_of(m_outputCbs.cbegin(), m_outputCbs.cend(),
                                 [](const auto &item) { return item.second->isChecked(); });
    if (!any) {
        QMessageBox::warning(this, tr("Message output"),
                             tr("With every output off the receiver sends nothing on USB."));
        return;
    }
    ubx::ConfigItems items;
    for (const auto &[key, cb] : std::as_const(m_outputCbs))
        items.append({key, cb->isChecked() ? 1 : 0});  // 1 — каждую эпоху
    emit valsetRequested(items, m_flashCb->isChecked());
}

void ConfigTab::confirmFactoryReset()
{
    const auto answer =
        QMessageBox::warning(this, tr("Factory defaults"),
                             tr("Erase the saved configuration (BBR and Flash) and load the factory defaults?\n\n"
                                "Rate, constellations and message output return to their default values."),
                             QMessageBox::Reset | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Reset)
        emit factoryResetRequested();
}
