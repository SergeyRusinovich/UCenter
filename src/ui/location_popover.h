// Всплывающая карточка у «прицела»: почему нет геопозиции компьютера и что сделать.
#pragma once

#include "core/host_location.h"

#include <QWidget>

class QLabel;

class LocationPopover : public QWidget
{
    Q_OBJECT

public:
    explicit LocationPopover(QWidget *parent = nullptr);
    // Показывает карточку под anchor, выровненную по его правому краю
    void showAt(QWidget *anchor, LocationLevel level, const QString &text);

signals:
    void retryRequested();

private:
    QLabel *m_title;
    QLabel *m_text;
};
