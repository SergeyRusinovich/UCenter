// Модель строк консоли поверх MessageLog: текст строки строится, только когда вид её показывает.
#pragma once

#include "core/message_log.h"

#include <QAbstractListModel>

class ConsoleModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit ConsoleModel(qsizetype capacity, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    const GnssMessage &message(int row) const { return m_log.row(row); }
    // Пачка в конец; самые старые сообщения вытесняются, вид узнаёт об этом заранее.
    // Возвращает число строк, ушедших с начала
    qsizetype append(const QList<GnssMessage> &batch);
    void setFilter(MessageLog::Filter filter);
    void clear();

private:
    MessageLog m_log;
};
