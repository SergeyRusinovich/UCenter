// Панели измерения A->B: ввод координат точек, результат и плашка результата поверх карты.
#pragma once

#include "core/baseline.h"
#include "core/points.h"

#include <QFrame>
#include <QGroupBox>
#include <QList>

#include <array>
#include <optional>

class QLabel;
class ValueLabel;
class QLineEdit;
class QPushButton;

// Координаты A и B, позиция устройства и подсказка, что делать дальше
class PointsEditor : public QGroupBox
{
    Q_OBJECT

public:
    explicit PointsEditor(QWidget *parent = nullptr);

    // fromDevice — обновление по позиции устройства: не затирает поле, в котором печатают
    void showPoint(PointKey key, std::optional<LatLon> ll, bool fromDevice = false);
    void setDeviceLocation(const QString &text);
    void setHint(const QString &text);

signals:
    void pointEdited(PointKey key, std::optional<LatLon> ll);  // ввод пользователя; nullopt — поля очищены

private:
    void onEdit(PointKey key);

    std::array<std::pair<QLineEdit *, QLineEdit *>, 2> m_edits;  // широта, долгота для A и B
    QLabel *m_device;                                            // координаты устройства, когда они есть
    QLabel *m_hint;
};

// Результат измерения; Save и Clear прижаты к низу блока
class ResultPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit ResultPanel(QWidget *parent = nullptr);
    void setResult(const std::optional<Baseline> &result);

signals:
    void saveRequested();
    void clearRequested();

private:
    QList<ValueLabel *> m_labels;
    QPushButton *m_save;
};

// Плашка поверх карты в развёрнутом виде: результат и Save вместо скрытых панелей
class ResultOverlay : public QFrame
{
    Q_OBJECT

public:
    explicit ResultOverlay(QWidget *parent);
    void setResult(const std::optional<Baseline> &result);

signals:
    void saveRequested();

private:
    QLabel *m_label;
    QPushButton *m_save;
};
