// Сообщение приёмника (кадр NMEA или UBX), его классификация и текст для консоли.
#pragma once

#include "core/message_types.h"

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>

struct GnssMessage {
    enum class Protocol { Nmea, Ubx };

    Protocol protocol = Protocol::Nmea;
    QByteArray raw;    // кадр целиком, как пришёл из порта
    QString identity;  // «GNGGA», «PUBX00», «NAV-PVT», «ACK-ACK»

    // NMEA: talker («GN», «GP»; «P» — фирменные), тип («GGA») и поля после адреса
    QString talker;
    QString sentence;
    QList<QByteArray> fields;

    // UBX: класс и идентификатор; нагрузка — raw без заголовка и контрольной суммы
    quint8 cls = 0;
    quint8 id = 0;

    // Разобранное содержимое (заполняется при приёме, один раз)
    MessageData data;

    bool isUbx() const { return protocol == Protocol::Ubx; }
    QByteArray payload() const;
    // Разобранное содержимое данного типа или nullptr
    template <class T>
    const T *get() const
    {
        return std::get_if<T>(&data);
    }
};

Q_DECLARE_METATYPE(GnssMessage)

namespace messages {

// UBX — в разобранном виде, NMEA — исходной строкой
QString messageText(const GnssMessage &msg);
// Подробности для консоли «имя — значение»: тип и длина, разобранные (UBX) или поименованные (NMEA)
// поля и кадр целиком (raw; UBX — в hex по 16 байт в строке)
QList<std::pair<QString, QString>> details(const GnssMessage &msg);

}  // namespace messages
