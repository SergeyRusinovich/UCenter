// Таблица сохранённых измерений: хранение между запусками, удаление, экспорт в CSV.
#pragma once

#include "core/baseline.h"

#include <QGroupBox>
#include <QList>

class QPushButton;
class QTableWidget;

class SavedMeasurementsPanel : public QGroupBox
{
    Q_OBJECT

public:
    // Пустой storePath — measurements.json в каталоге данных приложения
    explicit SavedMeasurementsPanel(const QString &storePath = QString(), QWidget *parent = nullptr);

    void add(const Baseline &item);

signals:
    void showRequested(const Baseline &item);  // показать на карте

private:
    void requestShow(int row);
    void deleteSelected();
    void fill();
    void updateButtons();
    void load();
    void store();
    void exportCsv();

    QString m_storePath;
    QList<Baseline> m_items;
    QTableWidget *m_table;
    QPushButton *m_show;
    QPushButton *m_delete;
    QPushButton *m_export;
};
