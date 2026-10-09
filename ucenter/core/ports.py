"""Поиск последовательных портов."""
import glob

from serial.tools import list_ports


def find_ports() -> list[str]:
    """На macOS ищем по шаблону /dev/cu.usbmodem*, затем добавляем остальные порты.

    Номер usbmodem зависит от USB-разъёма, поэтому конкретный порт не зашит:
    приёмник (usbmodem) всегда идёт первым в списке.
    """
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    for p in list_ports.comports():
        if p.device not in ports and not p.device.startswith("/dev/tty."):
            ports.append(p.device)
    return ports
