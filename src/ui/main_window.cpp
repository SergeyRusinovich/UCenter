#include "ui/main_window.h"

#include "core/language.h"
#include "core/ports.h"
#include "core/session.h"
#include "ui/azimuth_tab.h"
#include "ui/config_tab.h"
#include "ui/connection_panel.h"
#include "ui/console_tab.h"
#include "ui/dashboard_tab.h"
#include "ui/satellites_tab.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const char *const GeometrySetting = "window/geometry";
const char *const TabSetting = "window/tab";
constexpr int CornerMargin = 8;  // px

}  // namespace

MainWindow::MainWindow()
{
    setWindowTitle(tr("UCenter %1 — u-blox M9").arg(QCoreApplication::applicationVersion()));
    resize(1000, 750);
    m_session = new ReceiverSession(this);

    m_connection = new ConnectionPanel(ports::findPorts);
    m_consoleTab = new ConsoleTab;
    m_dashboardTab = new DashboardTab;
    m_satellitesTab = new SatellitesTab;
    m_azimuthTab = new AzimuthTab;
    m_configTab = new ConfigTab;
    buildLayout();
    wire();
    restoreSettings();

    // Сообщения только обновляют модели, а перерисовка идёт по таймерам
    auto *uiTimer = new QTimer(this);
    connect(uiTimer, &QTimer::timeout, this, &MainWindow::refreshUi);
    uiTimer->start(UiIntervalMs);
    auto *satTimer = new QTimer(this);
    connect(satTimer, &QTimer::timeout, this, &MainWindow::refreshSatellites);
    satTimer->start(SatIntervalMs);
}

void MainWindow::buildLayout()
{
    auto *central = new QWidget;
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->addWidget(m_connection);
    m_tabs = new QTabWidget;
    m_tabs->addTab(m_consoleTab, tr("Console"));
    m_tabs->addTab(m_dashboardTab, tr("Dashboard"));
    m_tabs->addTab(m_satellitesTab, tr("Satellites"));
    m_tabs->addTab(m_azimuthTab, tr("Azimuth"));
    m_tabs->addTab(m_configTab, tr("Configuration"));
    root->addWidget(m_tabs);
    setStatusBar(new QStatusBar);
    buildLanguageSelector();
}

void MainWindow::buildLanguageSelector()
{
    // Выпадающий список в правом углу строки вкладок; названия — на самих языках,
    // чтобы найти свой при любом текущем
    m_languageCombo = new QComboBox;
    m_languageCombo->setToolTip(tr("Interface language"));
    for (const language::Language &lang : language::available())
        m_languageCombo->addItem(lang.nativeName, lang.code);
    m_languageCombo->setCurrentIndex(m_languageCombo->findData(language::current()));
    connect(m_languageCombo, &QComboBox::activated, this,
            [this] { requestLanguage(m_languageCombo->currentData().toString()); });
    // Угловой виджет выходит за край области вкладок — отступ выравнивает его с ней
    auto *corner = new QWidget;
    auto *cornerLayout = new QHBoxLayout(corner);
    cornerLayout->setContentsMargins(0, 0, CornerMargin, 0);
    cornerLayout->addWidget(m_languageCombo);
    m_tabs->setCornerWidget(corner, Qt::TopRightCorner);
}

void MainWindow::requestLanguage(const QString &code)
{
    if (code == language::current())
        return;
    // Окно создаётся заново на новом языке — соединение при этом закрывается
    if (m_session->isOpen()) {
        const auto answer = QMessageBox::question(
            this, tr("Language"), tr("The window will reopen in the new language and the connection will be closed."),
            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
        if (answer != QMessageBox::Ok) {
            // В списке снова текущий язык
            const QSignalBlocker block(m_languageCombo);
            m_languageCombo->setCurrentIndex(m_languageCombo->findData(language::current()));
            return;
        }
    }
    emit languageChangeRequested(code);
}

void MainWindow::wire()
{
    ReceiverSession *s = m_session;
    connect(m_connection, &ConnectionPanel::connectRequested, this, &MainWindow::connectPort);
    connect(m_connection, &ConnectionPanel::replayRequested, this, [this](const QString &path) {
        m_connection->setBusy();
        m_session->openReplay(path);
    });
    connect(m_connection, &ConnectionPanel::disconnectRequested, s, &ReceiverSession::close);
    connect(m_connection, &ConnectionPanel::recordRequested, s, &ReceiverSession::startRecording);
    connect(m_connection, &ConnectionPanel::stopRecordRequested, s, &ReceiverSession::stopRecording);
    connect(m_configTab, &ConfigTab::valsetRequested, s, &ReceiverSession::applyConfig);
    connect(m_configTab, &ConfigTab::factoryResetRequested, s, &ReceiverSession::factoryReset);
    connect(m_configTab, &ConfigTab::refreshRequested, s, &ReceiverSession::pollConfig);
    connect(s, &ReceiverSession::connectedChanged, this, [this](bool on) {
        const bool replay = m_session->isReplay();
        m_connection->setConnected(on, replay);
        m_configTab->setConnected(on && !replay);  // записи команды не отправить
    });
    connect(s, &ReceiverSession::recordingChanged, m_connection, &ConnectionPanel::setRecording);
    connect(m_azimuthTab, &AzimuthTab::expandChanged, this, [this](bool on) { m_connection->setVisible(!on); });
    connect(s, &ReceiverSession::message, this, &MainWindow::onMessage);
    connect(s, &ReceiverSession::configValues, m_configTab, &ConfigTab::applyValues);
    connect(s, &ReceiverSession::status, this,
            [this](const QString &text) { statusBar()->showMessage(text, StatusMs); });
    connect(s, &ReceiverSession::error, this, &MainWindow::onError);
    m_configTab->setConnected(false);
}

void MainWindow::startReplay(const QString &path, double speed, bool quitAtEnd)
{
    if (quitAtEnd)
        connect(m_session, &ReceiverSession::connectedChanged, qApp, [](bool on) {
            if (!on)
                QCoreApplication::quit();
        });
    m_connection->setBusy();
    m_session->openReplay(path, speed);
}

QString MainWindow::currentTab() const
{
    return m_tabs->currentWidget()->metaObject()->className();
}

void MainWindow::connectPort(const QString &port, int baudrate)
{
    m_connection->setBusy();
    m_session->open(port, baudrate);
}

void MainWindow::onError(const QString &text)
{
    statusBar()->showMessage(text, 2 * StatusMs);
    if (m_errorShown)  // не плодим окна при серии ошибок
        return;
    m_errorShown = true;
    QMessageBox::critical(this, tr("Connection error"), text);
    m_errorShown = false;
}

// ---------- Данные ----------
void MainWindow::onMessage(const GnssMessage &msg)
{
    m_consoleTab->appendMessage(msg);
    if (m_nav.apply(msg))
        m_navDirty = true;
    m_tracker.handle(msg);
}

void MainWindow::refreshUi()
{
    if (m_navDirty) {
        m_navDirty = false;
        m_dashboardTab->updateState(m_nav.data());
    }
    m_consoleTab->flush();
}

void MainWindow::refreshSatellites()
{
    m_satellitesTab->updateData(m_tracker.snapshot());
}

void MainWindow::restoreSettings()
{
    const QSettings settings;
    restoreGeometry(settings.value(QLatin1String(GeometrySetting)).toByteArray());
    m_tabs->setCurrentIndex(settings.value(QLatin1String(TabSetting), 0).toInt());
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.setValue(QLatin1String(GeometrySetting), saveGeometry());
    settings.setValue(QLatin1String(TabSetting), m_tabs->currentIndex());
    m_azimuthTab->saveSettings();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    m_session->close();
    QMainWindow::closeEvent(event);
}
