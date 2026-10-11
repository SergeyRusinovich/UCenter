#include "ui/connection_panel.h"

#include "core/timeutil.h"
#include "ui/style.h"

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>

namespace {

const char *const PortSetting = "connection/port";
const char *const BaudSetting = "connection/baud";
const char *const DirSetting = "connection/recording_dir";

}  // namespace

ConnectionPanel::ConnectionPanel(std::function<QStringList()> findPorts, QWidget *parent)
    : QGroupBox(tr("Connection"), parent)
    , m_findPorts(std::move(findPorts))
{
    auto *lay = new QHBoxLayout(this);
    m_portCombo = new QComboBox;
    m_portCombo->setEditable(true);
    m_portCombo->setMinimumWidth(190);
    m_baudCombo = new QComboBox;
    for (int b : {9600, 38400, 115200})
        m_baudCombo->addItem(QString::number(b), b);
    const int baud = QSettings().value(QLatin1String(BaudSetting), 38400).toInt();
    m_baudCombo->setCurrentIndex(std::max(0, m_baudCombo->findData(baud)));
    m_refreshBtn = new QPushButton;
    m_refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshBtn->setToolTip(tr("Refresh the port list"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &ConnectionPanel::refreshPorts);
    m_connectBtn = new QPushButton;
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectionPanel::onButton);
    m_replayBtn = new QPushButton(tr("▶ From file…"));
    m_replayBtn->setToolTip(tr("Play back a recorded stream (.ubx) instead of a receiver"));
    connect(m_replayBtn, &QPushButton::clicked, this, &ConnectionPanel::onReplay);
    m_recordBtn = new QPushButton;
    connect(m_recordBtn, &QPushButton::clicked, this, &ConnectionPanel::onRecord);
    m_statusLed = new QLabel;
    m_statusLed->setObjectName(QStringLiteral("statusLed"));
    m_statusLabel = new QLabel;
    m_uptimeLabel = new QLabel;
    m_uptimeTimer = new QTimer(this);
    connect(m_uptimeTimer, &QTimer::timeout, this, &ConnectionPanel::updateUptime);
    // Слева — подключение к приёмнику, затем запись и воспроизведение, справа — состояние
    const QList<QWidget *> left = {new QLabel(tr("Port:")),      m_portCombo, m_refreshBtn,
                                   new QLabel(tr("Baud rate:")), m_baudCombo, m_connectBtn};
    for (QWidget *w : left)
        lay->addWidget(w);
    lay->addSpacing(12);
    lay->addWidget(m_recordBtn);
    lay->addWidget(m_replayBtn);
    lay->addStretch();
    for (QWidget *w : {m_statusLed, m_statusLabel, m_uptimeLabel})
        lay->addWidget(w);
    refreshPorts();
    setConnected(false);
    setRecording(false);
    // Приёмник могли переткнуть в другой разъём — подхватываем новый порт сами
    auto *portTimer = new QTimer(this);
    connect(portTimer, &QTimer::timeout, this, &ConnectionPanel::pollPorts);
    portTimer->start(PortPollMs);
}

void ConnectionPanel::refreshPorts()
{
    const QStringList ports = m_findPorts();
    QString current = m_portCombo->currentText();
    if (current.isEmpty())
        current = QSettings().value(QLatin1String(PortSetting)).toString();  // порт прошлого сеанса
    m_ports = ports;
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    // Оставляем выбранный порт, только если он ещё существует, иначе — первый найденный
    if (ports.contains(current))
        m_portCombo->setCurrentText(current);
}

void ConnectionPanel::pollPorts()
{
    if (m_connected || m_portCombo->hasFocus())
        return;  // не мешаем вводу вручную и не трогаем список во время сеанса
    if (m_findPorts() != m_ports)
        refreshPorts();
}

void ConnectionPanel::setBusy()
{
    m_connectBtn->setEnabled(false);
    m_replayBtn->setEnabled(false);
}

void ConnectionPanel::updateUptime()
{
    // Время с момента успешного подключения
    m_uptimeLabel->setText(timeutil::formatDuration(timeutil::monotonic() - m_uptimeStart));
}

void ConnectionPanel::setConnected(bool on, bool replay)
{
    m_connected = on;
    m_replay = on && replay;
    style::setStyleProperty(m_statusLed, "state", on ? QStringLiteral("on") : QStringLiteral("off"));
    if (m_replay)
        // Длинное имя файла сокращается, полностью — в подсказке
        m_statusLabel->setText(fontMetrics().elidedText(QStringLiteral("▶ ") + QFileInfo(m_replayFile).fileName(),
                                                        Qt::ElideMiddle, MaxStatusWidth));
    else
        m_statusLabel->setText(on ? tr("Connected") : tr("Disconnected"));
    m_statusLabel->setToolTip(m_replay ? tr("Replaying %1").arg(m_replayFile) : QString());
    m_connectBtn->setText(on ? tr("Disconnect") : tr("Connect"));
    style::setStyleProperty(m_connectBtn, "variant", on ? QStringLiteral("danger") : QStringLiteral("primary"));
    m_connectBtn->setEnabled(true);
    m_replayBtn->setEnabled(!on);
    m_recordBtn->setEnabled(on && !m_replay);
    m_portCombo->setEnabled(!on);
    m_baudCombo->setEnabled(!on);
    m_refreshBtn->setEnabled(!on);
    // Таймер запускается при подключении и сбрасывается при отключении
    m_uptimeLabel->setVisible(on);
    if (on) {
        m_uptimeStart = timeutil::monotonic();
        updateUptime();
        m_uptimeTimer->start(1000);
    } else {
        m_uptimeTimer->stop();
    }
}

void ConnectionPanel::setRecording(bool on)
{
    m_recording = on;
    m_recordBtn->setText(on ? tr("■ Stop") : tr("● Record…"));
    m_recordBtn->setToolTip(on ? tr("Stop recording") : tr("Save the raw stream from the receiver to a .ubx file"));
    style::setStyleProperty(m_recordBtn, "variant", on ? QStringLiteral("danger") : QString());
}

void ConnectionPanel::onButton()
{
    if (m_connected) {
        emit disconnectRequested();
        return;
    }
    const QString port = m_portCombo->currentText().trimmed();
    if (port.isEmpty()) {
        QMessageBox::warning(this, tr("Port"), tr("Select a port."));
        return;
    }
    QSettings settings;
    settings.setValue(QLatin1String(PortSetting), port);
    settings.setValue(QLatin1String(BaudSetting), m_baudCombo->currentData());
    emit connectRequested(port, m_baudCombo->currentData().toInt());
}

QString ConnectionPanel::recordingDir() const
{
    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QSettings().value(QLatin1String(DirSetting), fallback).toString();
}

void ConnectionPanel::onReplay()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Replay a recording"), recordingDir(),
                                                      tr("GNSS recordings (*.ubx *.nmea *.log *.bin);;All files (*)"));
    if (path.isEmpty())
        return;
    QSettings().setValue(QLatin1String(DirSetting), QFileInfo(path).absolutePath());
    m_replayFile = path;
    emit replayRequested(path);
}

void ConnectionPanel::onRecord()
{
    if (m_recording) {
        emit stopRecordRequested();
        return;
    }
    const QString name =
        QStringLiteral("ucenter-%1.ubx").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Record the stream"), QDir(recordingDir()).filePath(name), tr("UBX recordings (*.ubx)"));
    if (path.isEmpty())
        return;
    QSettings().setValue(QLatin1String(DirSetting), QFileInfo(path).absolutePath());
    emit recordRequested(path);
}
