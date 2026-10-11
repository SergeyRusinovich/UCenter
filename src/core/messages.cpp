#include "core/messages.h"

#include "core/ubx.h"

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

}  // namespace messages
