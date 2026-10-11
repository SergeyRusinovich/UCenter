// Консоль данных NMEA/UBX с Freeze и фильтрами.
#pragma once

#include "core/messages.h"

#include <QList>
#include <QWidget>

class QCheckBox;
class QPlainTextEdit;

class ConsoleTab : public QWidget
{
    Q_OBJECT

public:
    static constexpr int MaxLines = 5000;  // ограничиваем память

    explicit ConsoleTab(QWidget *parent = nullptr);

    void appendMessage(const GnssMessage &msg);
    // Выводит накопленные строки; пока вкладка скрыта, сообщения только копятся
    void flush();

private:
    void clear();
    void saveFilters() const;

    QCheckBox *m_freezeCb;
    QCheckBox *m_nmeaOnlyCb;
    QCheckBox *m_ubxOnlyCb;
    QPlainTextEdit *m_console;
    // Сообщения копятся здесь, а в текст превращаются в flush() — только те, что будут показаны,
    // и одной вставкой вместо сотен в секунду
    QList<GnssMessage> m_pending;
};
