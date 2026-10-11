// libFuzzer: произвольные байты «из порта» -> FrameReader -> разбор и текст консоли.
// Ищет падения, чтение за границы и неопределённое поведение (AddressSanitizer + UBSan).
//
// Сборка (clang с libFuzzer):  cmake -S . -B build-fuzz -DUCENTER_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
// Запуск:                      build-fuzz/fuzz_frame_reader -max_total_time=60 fuzz/corpus
#include "core/frame_reader.h"
#include "core/messages.h"
#include "core/navigation.h"
#include "core/replay.h"
#include "core/satellites.h"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0)
        return 0;
    // Первый байт задаёт размер кусков: поток приходит из порта частями произвольной длины
    const qsizetype chunk = 1 + data[0];
    const QByteArray bytes(reinterpret_cast<const char *>(data + 1), qsizetype(size - 1));

    FrameReader reader;
    NavigationState nav;
    SatelliteTracker tracker;
    QList<GnssMessage> all;
    for (qsizetype i = 0; i < bytes.size(); i += chunk) {
        for (const GnssMessage &msg : reader.feed(bytes.mid(i, chunk))) {
            (void)messages::messageText(msg);
            nav.apply(msg, 0.0);
            tracker.handle(msg, 0.0);
            all.append(msg);
        }
    }
    (void)tracker.snapshot(0.0);
    (void)replayEpochs(all);
    return 0;
}
