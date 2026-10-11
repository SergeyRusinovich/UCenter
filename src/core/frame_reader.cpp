#include "core/frame_reader.h"

#include "core/nmea.h"
#include "core/ubx.h"

#include <QtEndian>

QList<GnssMessage> FrameReader::feed(const QByteArray &data)
{
    m_buffer += data;
    QList<GnssMessage> out;
    const QByteArray &buf = m_buffer;
    qsizetype i = 0;
    while (i < buf.size()) {
        const qsizetype left = buf.size() - i;
        const auto c = quint8(buf[i]);
        if (c == ubx::Sync1) {
            if (left < 2)
                break;  // ждём второй байт синхронизации
            if (quint8(buf[i + 1]) != ubx::Sync2) {
                ++i;
                continue;
            }
            if (left < ubx::HeaderSize)
                break;
            const int length = qFromLittleEndian<quint16>(buf.constData() + i + 4);
            if (length > MaxUbxPayload) {
                ++i;
                continue;
            }
            const qsizetype total = ubx::FrameOverhead + length;
            if (left < total)
                break;
            const QByteArray raw = buf.mid(i, total);
            const auto [a, b] = ubx::checksum(raw.mid(2, total - 4));
            if (quint8(raw[total - 2]) != a || quint8(raw[total - 1]) != b) {
                ++i;
                continue;
            }
            out.append(ubx::message(raw));
            i += total;
        } else if (c == '$') {
            const qsizetype end = buf.indexOf('\n', i);
            if (end < 0) {
                if (left > MaxNmeaLength) {
                    ++i;
                    continue;
                }
                break;  // строка ещё не пришла целиком
            }
            if (end - i > MaxNmeaLength) {
                ++i;
                continue;
            }
            if (auto msg = nmea::parse(buf.mid(i, end - i + 1))) {
                out.append(*msg);
                i = end + 1;
            } else {
                ++i;
            }
        } else {
            ++i;
        }
    }
    m_buffer.remove(0, i);
    return out;
}
