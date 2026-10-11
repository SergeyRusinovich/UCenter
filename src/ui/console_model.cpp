#include "ui/console_model.h"

ConsoleModel::ConsoleModel(qsizetype capacity, QObject *parent)
    : QAbstractListModel(parent)
    , m_log(capacity)
{}

int ConsoleModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_log.rowCount());
}

QVariant ConsoleModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole || !index.isValid() || index.row() >= m_log.rowCount())
        return {};
    return messages::messageText(m_log.row(index.row()));
}

qsizetype ConsoleModel::append(const QList<GnssMessage> &batch)
{
    const qsizetype evicted = m_log.rowsEvictedBy(batch.size());
    if (evicted) {
        beginRemoveRows(QModelIndex(), 0, int(evicted - 1));
        m_log.evict(batch.size());
        endRemoveRows();
    } else {
        m_log.evict(batch.size());  // могли уйти только скрытые фильтром сообщения
    }
    if (const qsizetype added = m_log.rowsAddedBy(batch)) {
        const int first = int(m_log.rowCount());
        beginInsertRows(QModelIndex(), first, int(first + added - 1));
        m_log.append(batch);
        endInsertRows();
    } else {
        m_log.append(batch);
    }
    return evicted;
}

void ConsoleModel::setFilter(MessageLog::Filter filter)
{
    beginResetModel();
    m_log.setFilter(filter);
    endResetModel();
}

void ConsoleModel::clear()
{
    beginResetModel();
    m_log.clear();
    endResetModel();
}
