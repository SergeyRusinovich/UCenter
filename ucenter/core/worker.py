"""Поток чтения GNSS-приёмника: порт + pyubx2, наружу только Qt-сигналы."""
import threading

import serial
from PyQt6.QtCore import QThread, pyqtSignal
from pyubx2 import UBXReader, NMEA_PROTOCOL, UBX_PROTOCOL


class GNSSWorker(QThread):
    """Читает поток порта в отдельном потоке и отдаёт сообщения в GUI сигналами."""

    message = pyqtSignal(object, bytes)  # (распарсенное сообщение, сырые байты)
    error = pyqtSignal(str)              # ошибка соединения: порт занят, отключён и т.д.
    connected = pyqtSignal()             # порт успешно открыт

    def __init__(self, port: str, baudrate: int, parent=None):
        super().__init__(parent)
        self._port = port
        self._baudrate = baudrate
        self._running = True
        self._serial = None
        self._write_lock = threading.Lock()

    def run(self):
        # Открываем порт; при ошибке сообщаем в GUI и выходим
        try:
            self._serial = serial.Serial(self._port, self._baudrate, timeout=1)
        except (serial.SerialException, OSError) as exc:
            self.error.emit(f"Не удалось открыть {self._port}: {exc}")
            return
        self.connected.emit()

        # UBXReader сам читает из потока побайтно и ищет границы кадров
        reader = UBXReader(
            self._serial,
            protfilter=NMEA_PROTOCOL | UBX_PROTOCOL,
            quitonerror=0,  # ошибки разбора не прерывают чтение
        )
        bad_frames = 0
        try:
            while self._running:
                try:
                    raw, parsed = reader.read()
                except Exception as exc:  # битый кадр — пропускаем
                    if not self._running:
                        break
                    if isinstance(exc, (serial.SerialException, OSError)):
                        raise
                    # Повторяющиеся ошибки подряд не должны крутить цикл вхолостую
                    bad_frames += 1
                    if bad_frames >= 5:
                        self.msleep(50)
                    continue
                bad_frames = 0
                if raw is None or parsed is None:  # таймаут чтения или нераспознанный кадр
                    continue
                self.message.emit(parsed, raw)
        except (serial.SerialException, OSError) as exc:
            if self._running:
                self.error.emit(f"Соединение потеряно: {exc}")
        finally:
            with self._write_lock:
                if self._serial and self._serial.is_open:
                    self._serial.close()

    def send(self, data: bytes) -> str | None:
        """Потокобезопасная отправка байт в приёмник; возвращает текст ошибки или None."""
        with self._write_lock:
            if not self._serial or not self._serial.is_open:
                return "порт не открыт"
            try:
                self._serial.write(data)
                self._serial.flush()
                return None
            except (serial.SerialException, OSError) as exc:
                return f"ошибка записи: {exc}"

    def stop(self) -> bool:
        """Просит поток завершиться; True, если он успел остановиться."""
        self._running = False
        try:  # прерываем ожидающий read(), чтобы не ждать таймаут порта
            if self._serial and self._serial.is_open:
                self._serial.cancel_read()
        except (AttributeError, NotImplementedError, OSError, serial.SerialException):
            pass  # cancel_read есть не на всех платформах
        return self.wait(3000)
