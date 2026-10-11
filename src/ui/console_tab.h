// Консоль данных NMEA/UBX: список сообщений с Freeze и фильтрами, под ним — поля выбранного сообщения.
#pragma once

#include "core/messages.h"

#include <QElapsedTimer>
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
    // Вставка строк и перерисовка дороже самих данных: выводим не чаще 4 раз в секунду
    static constexpr int FlushIntervalMs = 250;

    explicit ConsoleTab(QWidget *parent = nullptr);

    void appendMessage(const GnssMessage &msg);
    // Переносит накопленные сообщения в список (не чаще FlushIntervalMs)
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
    QElapsedTimer m_lastFlush;
};
