#include "core/messages.h"

#include "core/nmea.h"
#include "core/ubx.h"

#include <QStringList>

QByteArray GnssMessage::payload() const
{
    if (!isUbx() || raw.size() < ubx::FrameOverhead)
        return {};
    return raw.mid(ubx::HeaderSize, raw.size() - ubx::FrameOverhead);
}

namespace messages {

QString messageText(const GnssMessage &msg)
{
    return msg.isUbx() ? ubx::describe(msg) : QString::fromLatin1(msg.raw).trimmed();
}

QList<std::pair<QString, QString>> details(const GnssMessage &msg)
{
    QList<std::pair<QString, QString>> out{{QStringLiteral("message"), msg.identity},
                                           {QStringLiteral("length"), QString::number(msg.raw.size())}};
    if (msg.isUbx()) {
        out += ubx::fields(msg);
        QStringList lines;
        for (qsizetype at = 0; at < msg.raw.size(); at += 16)
            lines << QString::fromLatin1(msg.raw.mid(at, 16).toHex(' ').toUpper());
        out.append({QStringLiteral("raw"), lines.join(u'\n')});
    } else {
        for (qsizetype i = 0; i < msg.fields.size(); ++i)
            out.append({nmea::fieldName(msg, i), QString::fromLatin1(msg.fields[i])});
        out.append({QStringLiteral("raw"), QString::fromLatin1(msg.raw).trimmed()});
    }
    return out;
}

}  // namespace messages
