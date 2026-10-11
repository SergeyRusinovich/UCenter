# 0005. Receiver I/O in a reader thread behind `ReceiverSource`

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

A USB serial port can block: a receiver that hangs, a driver that stalls on close, a cable pulled during a write. At 10 Hz the receiver sends tens of messages per epoch. The UI must never freeze, and the same pipeline should also play back recordings ([0009](0009-recording-and-replay.md)).

## Decision

- All receiver I/O happens in a dedicated `QThread`. The object living there is a `ReceiverSource`: `GnssWorker` (a `QSerialPort`) or `ReplayWorker` (a recording). `ReceiverSession` in the GUI thread owns the thread and is the only class the UI talks to.
- The source emits all frames from one read as one batch (`messages(QList<GnssMessage>)`), so the queue between threads holds one event per read, not per message.
- Writes are queued to the reader thread without waiting; a write the receiver does not take within 2 s is reported (`sendFailed`) and dropped.
- Configuration commands wait for ACK/NAK with a 2 s timeout; ACKs are matched to pending commands by command ID.
- On disconnect the thread gets 3 s to stop; a thread stuck in the driver is detached and deletes itself when it finishes, instead of being destroyed while running.

## Alternatives considered

- **`QSerialPort` in the GUI thread** — asynchronous and simpler, but a stalled driver call or a burst of parsing blocks the UI.
- **Blocking reads with a polling loop (as in the Python version)** — wastes a thread on timeouts and complicates cancelling.
- **Synchronous (blocking queued) writes from the GUI** — simple to reason about, but a stuck device freezes the UI; this was the first C++ version and was replaced.

## Consequences

- The UI never touches the port; every receiver interaction goes through `ReceiverSession` signals and slots.
- Port and replay are interchangeable for everything above `ReceiverSession`.
- The behavior is tested end to end on a pseudo-terminal (`test_session`), including ACK, a missing ACK and reconnecting.
