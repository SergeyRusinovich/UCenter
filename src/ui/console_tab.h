// Консоль данных NMEA/UBX: список сообщений с Freeze и фильтрами, под ним — поля выбранного сообщения.
#pragma once

#include "core/messages.h"

#include <QList>
#include <QWidget>

class ConsoleModel;
class QCheckBox;
class QListView;
class QPlainTextEdit;

class ConsoleTab : public QWidget
{
    Q_OBJECT

public:
    static constexpr int Capacity = 50000;  // сообщений в истории; фильтры показывают её часть

    explicit ConsoleTab(QWidget *parent = nullptr);

    void appendMessage(const GnssMessage &msg);
    // Переносит накопленные сообщения в список; как часто — решает владелец (таймер MainWindow)
    void flush();

private:
    void clear();
    void applyFilter();
    void saveFilters() const;
    void showDetails(int row);
    void copySelection() const;

    QCheckBox *m_freezeCb;
    QCheckBox *m_nmeaOnlyCb;
    QCheckBox *m_ubxOnlyCb;
    ConsoleModel *m_model;
    QListView *m_view;
    QPlainTextEdit *m_details;
    // Сообщения копятся здесь и переходят в список одной пачкой вместо сотен вставок в секунду
    QList<GnssMessage> m_pending;
    bool m_appending = false;  // строки меняет добавление, а не пользователь
};
