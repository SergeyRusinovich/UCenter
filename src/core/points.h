// Точки A и B измерения и правила их установки (чистая логика, без Qt-объектов).
//
// A по умолчанию следует за геопозицией устройства («живая» A). Любая ручная установка A
// отвязывает её; attachA() привязывает снова. Пока позиции устройства нет, A не ждёт её:
// щелчок по карте ставит A вручную.
#pragma once

#include "core/baseline.h"
#include "core/geo.h"

#include <QString>

#include <array>
#include <optional>

enum class PointKey { A, B };
inline constexpr std::array<PointKey, 2> PointKeys = {PointKey::A, PointKey::B};

inline QString pointName(PointKey key)
{
    return key == PointKey::A ? QStringLiteral("A") : QStringLiteral("B");
}

class MeasurementPoints
{
public:
    std::optional<LatLon> operator[](PointKey key) const { return m_points[index(key)]; }
    std::optional<LatLon> device() const { return m_device; }
    bool aLive() const { return m_aLive; }

    std::optional<Baseline> result() const;

    // Ручная установка точки (щелчок, перетаскивание, ввод, найденное место)
    void set(PointKey key, std::optional<LatLon> ll);
    // Щелчок по карте ставит первую свободную точку; возвращает её ключ (nullopt — обе стоят)
    std::optional<PointKey> click(LatLon ll);
    // Новая позиция устройства; true, если A сдвинулась вместе с ней
    bool deviceMoved(LatLon ll);
    // A снова следует за устройством; true, если позиция устройства известна
    bool attachA();
    // После сохранения или очистки: B снимается, «живая» A возвращается к устройству
    void newMeasurement();
    // Показать сохранённое измерение (A отвязывается от устройства)
    void load(LatLon a, LatLon b);

private:
    static constexpr size_t index(PointKey key) { return key == PointKey::A ? 0 : 1; }

    std::array<std::optional<LatLon>, 2> m_points;
    std::optional<LatLon> m_device;
    bool m_aLive = true;
};
