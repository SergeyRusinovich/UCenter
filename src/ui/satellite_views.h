// Виджеты отрисовки: небесная карта и диаграмма C/N0.
#pragma once

#include "core/satellites.h"

#include <QList>
#include <QSet>
#include <QWidget>

// Какие столбики подписать: каждый every-й внутри группы системы, причём начало
// группы важнее — если оно слишком близко к предыдущей подписи, убираем ту.
QSet<int> labelIndices(const QList<char> &systems, int every);

// Небесная карта: азимут по окружности, возвышение к центру
class SkyView : public QWidget
{
    Q_OBJECT

public:
    explicit SkyView(QWidget *parent = nullptr);
    void setData(const QList<Satellite> &sats, const QSet<SatKey> &used, double marker);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QList<Satellite> m_sats;
    QSet<SatKey> m_used;
    double m_marker = 10;
};

// Столбчатая диаграмма C/N0 по спутникам, сгруппированная по системам
class SignalView : public QWidget
{
    Q_OBJECT

public:
    explicit SignalView(QWidget *parent = nullptr);
    void setData(const QList<Satellite> &sats, const QSet<SatKey> &used);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QList<Satellite> m_sats;
    QSet<SatKey> m_used;
};
