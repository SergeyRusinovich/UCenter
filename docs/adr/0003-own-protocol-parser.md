# 0003. Own NMEA and UBX parser

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

The receiver sends NMEA sentences and UBX frames interleaved on one stream. The app needs a small, fixed subset: NMEA GGA, GSA, RMC, GSV and UBX NAV-PVT, NAV-SAT, NAV-DOP, ACK-ACK/NAK, CFG-VALGET; it builds CFG-VALSET, CFG-VALGET and CFG-CFG. The Python version used pyubx2, which has no C++ counterpart. Bytes from a serial port are untrusted input: noise, partial frames and frames cut by a reconnect are normal.

## Decision

The app parses both protocols itself (`frame_reader`, `nmea`, `ubx`, `receiver_config`):

- `FrameReader` turns arbitrary byte chunks into frames: NMEA only with a valid `*hh` checksum, UBX only with a valid Fletcher checksum and a payload of at most 8 KB; anything else is skipped byte by byte.
- Decoders read only the fields the app uses; a known sentence with an unparsable field is kept for the console but not used in calculations.
- Configuration keys are numeric IDs from the M9 interface description; a key's value size is derived from the ID itself.
- Correctness is pinned by tests against reference frames generated with pyubx2.

## Alternatives considered

- **A third-party C/C++ library** (e.g. u-blox's or community UBX parsers) — either covers far more than needed, is unmaintained, or is C-style with manual buffers; the needed subset is a few hundred lines.
- **Embedding Python and pyubx2** — defeats the reason for the rewrite ([0002](0002-cpp-and-qt-widgets.md)).

## Consequences

- No external dependency beyond Qt; the parser is small enough to read in one sitting.
- Supporting a new message means adding a decoder and a test with a reference frame.
- The console shows a pyubx2-like text only for decoded messages; others show their name and length.
- The parser is fuzzed with libFuzzer and tested on randomly corrupted streams ([0013](0013-quality-gates.md)).
