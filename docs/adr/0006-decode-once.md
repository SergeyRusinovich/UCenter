# 0006. Decode every frame once into a `std::variant`

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

One message is used by several consumers: the console, the navigation solution, the satellite tracker and the session (ACK, VALGET). In the first C++ version each of them decoded the raw frame again.

## Decision

Every frame is decoded once, in the reader thread, when it is assembled. The result is stored in `GnssMessage::data`, a `std::variant` of the known message types (`nmea::Gga`, `ubx::NavPvt`, … ; `std::monostate` for messages without a decoder). Consumers read it with `msg.get<T>()`, which returns a pointer or `nullptr`.

## Alternatives considered

- **Decoding in each consumer** — duplicated work and duplicated error handling.
- **A class hierarchy with virtual methods** — heap allocations per message and type checks via `dynamic_cast`; the set of types is closed and small, which is what `std::variant` is for.
- **Decoding lazily on first access** — needs mutable caching inside a value that crosses threads.

## Consequences

- Decoding work leaves the GUI thread.
- Adding a message type means adding a struct to `message_types.h`, an alternative to `MessageData` and a decoder; consumers that do not care are unaffected.
- A message is slightly larger in memory; at receiver data rates this is negligible.
