// Журнал сообщений консоли: последние capacity сообщений и фильтр по протоколу.
// Строки журнала — сообщения, прошедшие фильтр; смена фильтра сразу показывает или прячет накопленные.
// Добавление разбито на шаги, чтобы модель Qt могла сообщить виду об удалении и вставке строк заранее:
//   rowsEvictedBy(n) -> evict(n) -> rowsAddedBy(batch) -> append(batch)
#pragma once

#include "core/messages.h"

#include <QList>

class MessageLog
{
public:
    enum class Filter { All, Nmea, Ubx };

    explicit MessageLog(qsizetype capacity);

    qsizetype capacity() const { return m_capacity; }
    Filter filter() const { return m_filter; }
    void setFilter(Filter filter);
    void clear();

    qsizetype rowCount() const { return m_rows.size(); }
    const GnssMessage &row(qsizetype index) const { return m_all[m_rows[index] - m_firstSeq]; }

    // Сколько строк с начала пропадёт, чтобы освободить место для count сообщений (вытесняются старые)
    qsizetype rowsEvictedBy(qsizetype count) const;
    void evict(qsizetype count);
    // Сколько сообщений пачки пройдут фильтр
    qsizetype rowsAddedBy(const QList<GnssMessage> &batch) const;
    // Место должно быть освобождено evict(); пачка больше capacity обрезается до последних сообщений
    void append(const QList<GnssMessage> &batch);

private:
    bool accepts(const GnssMessage &msg) const;
    qsizetype overflow(qsizetype count) const;

    qsizetype m_capacity;
    Filter m_filter = Filter::All;
    QList<GnssMessage> m_all;  // все сообщения по порядку
    qint64 m_firstSeq = 0;     // номер m_all[0]: номера растут с каждым сообщением
    QList<qint64> m_rows;      // номера сообщений, прошедших фильтр
};
