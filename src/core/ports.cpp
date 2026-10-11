#include "core/ports.h"

#include <QSerialPortInfo>

namespace ports {

QStringList findPorts()
{
    QStringList modems, others;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
#ifdef Q_OS_WIN
        const QString name = info.portName();  // COM3
#else
        const QString name = info.systemLocation();  // /dev/cu.usbmodem1301
#endif
        if (name.startsWith(QLatin1String("/dev/tty.")))
            continue;
        (name.startsWith(QLatin1String("/dev/cu.usbmodem")) ? modems : others) << name;
    }
    modems.sort();
    return modems + others;
}

}  // namespace ports
