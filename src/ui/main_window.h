// Главное окно: собирает модели, сеанс приёмника и вкладки и связывает их сигналами.
#pragma once

#include "core/navigation.h"
#include "core/satellites.h"

#include <QMainWindow>

class AzimuthTab;
class QComboBox;
class ConfigTab;
class ConnectionPanel;
class ConsoleTab;
class DashboardTab;
class QTabWidget;
class ReceiverSession;
class SatellitesTab;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    static constexpr int UiIntervalMs = 100;  // не чаще 10 перерисовок Dashboard/консоли в секунду
    static constexpr int SatIntervalMs = 1000;
    static constexpr int StatusMs = 5000;

    MainWindow();

    // Размер окна, вкладка и вид карты — между запусками
    void saveSettings() const;

signals:
    // Пользователь выбрал язык; окно пересоздаёт владелец
    void languageChangeRequested(const QString &code);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildLayout();
    void restoreSettings();
    void buildLanguageSelector();
    void requestLanguage(const QString &code);
    void wire();
    void connectPort(const QString &port, int baudrate);
    void onError(const QString &text);
    void onMessage(const GnssMessage &msg);
    void refreshUi();
    void refreshSatellites();

    ReceiverSession *m_session;
    NavigationState m_nav;
    SatelliteTracker m_tracker;
    bool m_navDirty = false;
    bool m_errorShown = false;

    ConnectionPanel *m_connection;
    ConsoleTab *m_consoleTab;
    DashboardTab *m_dashboardTab;
    SatellitesTab *m_satellitesTab;
    AzimuthTab *m_azimuthTab;
    ConfigTab *m_configTab;
    QTabWidget *m_tabs = nullptr;
    QComboBox *m_languageCombo = nullptr;  // язык интерфейса
};
