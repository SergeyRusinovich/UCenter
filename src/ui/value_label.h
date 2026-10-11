// Значение, которое меняется много раз в секунду (Dashboard, результат замера).
// В отличие от QLabel смена текста не пересчитывает раскладку окна: размер постоянный,
// перерисовывается только сам виджет. Не влезающий текст обрезается многоточием.
#pragma once

#include <QWidget>

class ValueLabel : public QWidget
{
    Q_OBJECT

public:
    explicit ValueLabel(QWidget *parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString &text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QString m_text;
};
