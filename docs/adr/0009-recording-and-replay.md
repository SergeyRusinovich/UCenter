# 0009. Recordings are the raw byte stream; replay is paced by epoch time

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

Developing and debugging without a receiver at hand, reproducing field problems and sharing data between users all need recorded streams. u-center records `.ubx` files that are simply the bytes received from the port.

## Decision

- **Recording** writes every byte read from the port, unchanged, in the reader thread — including bytes that did not form valid frames.
- **Replay** is a `ReceiverSource` ([0005](0005-reader-thread-and-sources.md)) that parses the whole file with the normal `FrameReader` and emits it epoch by epoch. Epoch time is `iTOW` of `UBX-NAV-PVT` when the file contains it, otherwise the UTC time of NMEA GGA/RMC; pauses longer than 2 s or negative (midnight or week rollover) become 100 ms. Without any time the stream is played in chunks of 20 messages every 100 ms.
- During replay the Configuration tab is disabled and sending is refused.

## Alternatives considered

- **A custom format with timestamps** — exact timing, but incompatible with u-center and other tools, and timestamps of arrival add little over the receiver's own epoch time.
- **Replaying at a fixed byte rate** — simple, but depends on the original baud rate and output configuration.

## Consequences

- Recordings from u-center and UCenter are interchangeable.
- Replay timing follows the solution epochs, not the exact arrival times inside an epoch.
- Recordings make good test fixtures: `test_session` records and replays through the real session.
