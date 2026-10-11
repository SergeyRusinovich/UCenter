// Вкладка конфигуратора: формирует команды конфигурации, отправку делает владелец.
#pragma once

#include "core/ubx.h"

#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QPushButton;

class ConfigTab : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigTab(QWidget *parent = nullptr);

    void setConnected(bool on);
    // Показывает текущие значения из CFG-VALGET
    void applyValues(const ubx::ConfigValues &values);

signals:
    void valsetRequested(const ubx::ConfigItems &items, bool flash);  // список (ключ, значение)
    void factoryResetRequested();
    void refreshRequested();  // перечитать текущие значения из приёмника

private:
    void applyRate();
    void applyConstellations();
    void applyOutputs();
    void confirmFactoryReset();

    QComboBox *m_rateCombo;
    QList<std::pair<quint32, QCheckBox *>> m_gnssCbs;
    QList<std::pair<quint32, QCheckBox *>> m_outputCbs;
    QCheckBox *m_flashCb;
    QList<QPushButton *> m_buttons;
};
