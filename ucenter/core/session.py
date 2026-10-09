"""Сеанс работы с приёмником: жизненный цикл GNSSWorker и транзакции конфигурации."""
import time
from collections import deque

from PyQt6.QtCore import QObject, QTimer, pyqtSignal

from ucenter.core import receiver_config as cfg
from ucenter.core.worker import GNSSWorker

ACK_TIMEOUT = 2.0  # секунд ожидания ACK на CFG-VALSET


class ReceiverSession(QObject):
    """Единственная точка общения с приёмником: открывает/закрывает порт,
    отправляет CFG-VALSET/VALGET и разбирает ответы на них."""

    connected_changed = pyqtSignal(bool)
    message = pyqtSignal(object, bytes)  # (распарсенное сообщение, сырые байты)
    config_values = pyqtSignal(dict)     # текущие значения ключей из CFG-VALGET
    status = pyqtSignal(str)             # краткие сообщения для строки статуса
    error = pyqtSignal(str)              # ошибки соединения

    def __init__(self, parent=None):
        super().__init__(parent)
        self._worker: GNSSWorker | None = None
        self._valset_sent: deque[float] = deque()  # время отправки VALSET, ждущих ACK
        self._ack_timer = QTimer(self)
        self._ack_timer.timeout.connect(self._check_ack_timeout)

    @property
    def is_open(self) -> bool:
        return self._worker is not None

    # ---------- Соединение ----------
    def open(self, port: str, baudrate: int):
        if self._worker:
            return
        self._worker = GNSSWorker(port, baudrate, self)
        self._worker.message.connect(self._on_message)
        self._worker.error.connect(self.error)
        self._worker.connected.connect(self._on_connected)
        self._worker.finished.connect(self._on_worker_finished)
        self._worker.start()

    def close(self):
        if self._worker:
            w, self._worker = self._worker, None
            w.message.disconnect()
            w.error.disconnect()
            if w.stop():
                w.deleteLater()
            else:  # поток ещё в read() — удаляем объект только после его завершения
                w.finished.connect(w.deleteLater)
        self._reset()

    def _on_connected(self):
        self.connected_changed.emit(True)
        self.poll_config()

    def _on_worker_finished(self):
        # Поток завершился сам (ошибка открытия/обрыв) — сбрасываем состояние
        if self._worker and not self._worker.isRunning():
            self._worker.deleteLater()
            self._worker = None
            self._reset()

    def _reset(self):
        self._valset_sent.clear()
        self._ack_timer.stop()
        self.connected_changed.emit(False)

    # ---------- Конфигурация ----------
    def apply_config(self, cfg_data: list[tuple[str, int]]):
        """Отправляет CFG-VALSET и ждёт подтверждения."""
        try:
            data = cfg.build_valset(cfg_data)
        except Exception as exc:  # неизвестный ключ или значение вне диапазона
            self.status.emit(f"Ошибка конфигурации: {exc}")
            return
        if self._send(data, "CFG-VALSET"):
            self._valset_sent.append(time.monotonic())
            self._ack_timer.start(500)
            self.status.emit("CFG-VALSET отправлен, ожидание подтверждения…")

    def poll_config(self):
        """Запрашивает текущие значения ключей конфигурации (CFG-VALGET)."""
        self._send(cfg.build_valget(cfg.POLL_KEYS), "CFG-VALGET")

    def _send(self, data: bytes, what: str) -> bool:
        err = self._worker.send(data) if self._worker else "нет подключения"
        if err:
            self.status.emit(f"Не удалось отправить {what}: {err}")
        return err is None

    def _on_message(self, parsed, raw: bytes):
        self.message.emit(parsed, raw)
        ack = cfg.valset_ack(parsed)
        if ack is not None:
            self._on_valset_ack(ack)
        values = cfg.valget_values(parsed)
        if values:
            self.config_values.emit(values)

    def _on_valset_ack(self, ok: bool):
        if not self._valset_sent:
            return  # ответ на чужой или уже просроченный запрос
        self._valset_sent.popleft()
        self.status.emit("CFG-VALSET применён (ACK)" if ok else "Приёмник отклонил CFG-VALSET (NAK)")
        if not self._valset_sent:
            self._ack_timer.stop()
            self.poll_config()  # показываем фактические значения после изменения

    def _check_ack_timeout(self):
        if self._valset_sent and time.monotonic() - self._valset_sent[0] > ACK_TIMEOUT:
            self._valset_sent.clear()
            self._ack_timer.stop()
            self.status.emit("Приёмник не подтвердил CFG-VALSET")
            self.poll_config()
