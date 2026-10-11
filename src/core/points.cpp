#include "core/points.h"

std::optional<Baseline> MeasurementPoints::result() const
{
    const auto a = m_points[0], b = m_points[1];
    if (!a || !b)
        return std::nullopt;
    return measure(*a, *b);
}

void MeasurementPoints::set(PointKey key, std::optional<LatLon> ll)
{
    if (key == PointKey::A)
        m_aLive = false;
    m_points[index(key)] = ll;
}

std::optional<PointKey> MeasurementPoints::click(LatLon ll)
{
    for (PointKey key : PointKeys) {
        if (!m_points[index(key)]) {
            set(key, ll);
            return key;
        }
    }
    return std::nullopt;
}

bool MeasurementPoints::deviceMoved(LatLon ll)
{
    m_device = ll;
    if (m_aLive)
        m_points[0] = ll;
    return m_aLive;
}

bool MeasurementPoints::attachA()
{
    m_aLive = true;
    if (m_device)
        m_points[0] = m_device;
    return m_device.has_value();
}

void MeasurementPoints::newMeasurement()
{
    m_points[0] = m_aLive ? m_device : std::nullopt;
    m_points[1].reset();
}

void MeasurementPoints::load(LatLon a, LatLon b)
{
    set(PointKey::A, a);
    set(PointKey::B, b);
}
