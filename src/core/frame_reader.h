// Поток байт порта -> кадры UBX и NMEA. Мусор между кадрами и кадры с неверной
// контрольной суммой пропускаются: чтение продолжается со следующего байта.
#pragma once

#include "core/messages.h"

#include <QByteArray>
#include <QList>

class FrameReader
{
public:
    static constexpr int MaxUbxPayload = 8192;
    static constexpr int MaxNmeaLength = 512;

    // Добавляет прочитанные байты; возвращает все кадры, собранные целиком
    QList<GnssMessage> feed(const QByteArray &data);
    void reset() { m_buffer.clear(); }

private:
    QByteArray m_buffer;  // хвост, из которого ещё не собран кадр
};
