// Поиск последовательных портов.
#pragma once

#include <QStringList>

namespace ports {

// Приёмник (/dev/cu.usbmodem* на macOS) всегда идёт первым в списке; номер usbmodem
// зависит от USB-разъёма, поэтому конкретный порт не зашит. Дубли /dev/tty.* не показываются.
QStringList findPorts();

}  // namespace ports
