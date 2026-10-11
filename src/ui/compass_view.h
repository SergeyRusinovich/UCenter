// Компас: показывает курс из датчика приёмника или сообщение, что компаса нет.
#pragma once

#include <QWidget>

#include <optional>

class CompassView : public QWidget
{
    Q_OBJECT

public:
    explicit CompassView(QWidget *parent = nullptr);
    void setState(bool supported, std::optional<double> heading, std::optional<double> cog);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    bool m_supported = false;
    std::optional<double> m_heading;
    std::optional<double> m_cog;
};
