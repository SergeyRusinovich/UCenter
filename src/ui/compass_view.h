// Компас: показывает курс из датчика приёмника или сообщение, что компаса нет.
#pragma once

#include <QWidget>

#include <optional>

class CompassView : public QWidget
{
    Q_OBJECT

public:
    // Меньшее изменение курса не видно (подпись — в целых градусах, стрелка — меньше пикселя)
    static constexpr double VisibleDeg = 0.5;

    explicit CompassView(QWidget *parent = nullptr);
    // Перерисовывает, только если изменение заметно
    void setState(bool supported, std::optional<double> heading, std::optional<double> cog);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    bool m_supported = false;
    std::optional<double> m_heading;
    std::optional<double> m_cog;
};
