#include "core/message_log.h"

#include <algorithm>

MessageLog::MessageLog(qsizetype capacity)
    : m_capacity(std::max<qsizetype>(capacity, 1))
{}

bool MessageLog::accepts(const GnssMessage &msg) const
{
    switch (m_filter) {
    case Filter::Nmea: return !msg.isUbx();
    case Filter::Ubx: return msg.isUbx();
    case Filter::All: break;
    }
    return true;
}

void MessageLog::setFilter(Filter filter)
{
    m_filter = filter;
    m_rows.clear();
    for (qsizetype i = 0; i < m_all.size(); ++i)
        if (accepts(m_all[i]))
            m_rows.append(m_firstSeq + i);
}

void MessageLog::clear()
{
    m_firstSeq += m_all.size();
    m_all.clear();
    m_rows.clear();
}

qsizetype MessageLog::overflow(qsizetype count) const
{
    return std::clamp<qsizetype>(m_all.size() + std::min(count, m_capacity) - m_capacity, 0, m_all.size());
}

qsizetype MessageLog::rowsEvictedBy(qsizetype count) const
{
    const qint64 keepFrom = m_firstSeq + overflow(count);
    return std::lower_bound(m_rows.cbegin(), m_rows.cend(), keepFrom) - m_rows.cbegin();
}

void MessageLog::evict(qsizetype count)
{
    const qsizetype drop = overflow(count);
    m_rows.remove(0, rowsEvictedBy(count));
    m_all.remove(0, drop);
    m_firstSeq += drop;
}

qsizetype MessageLog::rowsAddedBy(const QList<GnssMessage> &batch) const
{
    const qsizetype from = std::max<qsizetype>(0, batch.size() - m_capacity);
    return std::count_if(batch.cbegin() + from, batch.cend(), [this](const GnssMessage &m) { return accepts(m); });
}

void MessageLog::append(const QList<GnssMessage> &batch)
{
    const qsizetype from = std::max<qsizetype>(0, batch.size() - m_capacity);
    for (qsizetype i = from; i < batch.size(); ++i) {
        if (accepts(batch[i]))
            m_rows.append(m_firstSeq + m_all.size());
        m_all.append(batch[i]);
    }
}
