// Панель подключения: выбор порта/скорости, кнопка, индикатор статуса и время сеанса,
// воспроизведение записи и запись потока в файл.
#pragma once

#include <QGroupBox>
#include <QStringList>

#include <functional>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

class ConnectionPanel : public QGroupBox
{
    Q_OBJECT

public:
    static constexpr int PortPollMs = 2000;     // как часто искать новые порты, пока нет подключения
    static constexpr int MaxStatusWidth = 220;  // px: строка состояния не раздвигает окно

    // findPorts — источник списка портов (внедряется, чтобы панель не зависела от ОС)
    explicit ConnectionPanel(std::function<QStringList()> findPorts, QWidget *parent = nullptr);

    void refreshPorts();
    // Идёт открытие порта или записи
    void setBusy();
    // replay — подключена запись, а не приёмник
    void setConnected(bool on, bool replay = false);
    void setRecording(bool on);

signals:
    void connectRequested(const QString &port, int baudrate);
    void replayRequested(const QString &path);
    void disconnectRequested();
    void recordRequested(const QString &path);
    void stopRecordRequested();

private:
    void pollPorts();
    void updateUptime();
    void onButton();
    void onReplay();
    void onRecord();
    QString recordingDir() const;

    std::function<QStringList()> m_findPorts;
    bool m_connected = false;
    bool m_replay = false;
    bool m_recording = false;
    QString m_replayFile;
    QStringList m_ports;
    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QPushButton *m_refreshBtn;
    QPushButton *m_connectBtn;
    QPushButton *m_replayBtn;
    QPushButton *m_recordBtn;
    QLabel *m_statusLed;
    QLabel *m_statusLabel;
    QLabel *m_uptimeLabel;
    double m_uptimeStart = 0.0;
    QTimer *m_uptimeTimer;
};
