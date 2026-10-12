# Architecture Decision Records

Each record describes one architectural decision of UCenter: the context it was made in, what was decided, which alternatives were considered and what follows from it. Together they explain *why* the code is shaped the way [the README](../../README.md#architecture) describes.

| #    | Decision                                                                     | Status   |
|------|------------------------------------------------------------------------------|----------|
| 0001 | [Record architecture decisions](0001-record-architecture-decisions.md)        | Accepted |
| 0002 | [C++20 and Qt 6 Widgets instead of Python and PyQt6](0002-cpp-and-qt-widgets.md) | Accepted |
| 0003 | [Own NMEA and UBX parser](0003-own-protocol-parser.md)                        | Accepted |
| 0004 | [GUI-independent core library](0004-core-library.md)                          | Accepted |
| 0005 | [Receiver I/O in a reader thread behind `ReceiverSource`](0005-reader-thread-and-sources.md) | Accepted |
| 0006 | [Decode every frame once into a `std::variant`](0006-decode-once.md)          | Accepted |
| 0007 | [UBX messages take priority over NMEA](0007-ubx-over-nmea.md)                 | Accepted |
| 0008 | [Tile map drawn with `QPainter`](0008-painted-tile-map.md)                    | Accepted |
| 0009 | [Recordings are the raw byte stream; replay is paced by epoch time](0009-recording-and-replay.md) | Accepted |
| 0010 | [Localization with Qt Linguist; language switch recreates the window](0010-localization.md) | Accepted, partly superseded by 0015 |
| 0011 | [A signed app bundle on macOS for location access](0011-macos-bundle.md)       | Accepted, extended by 0019 |
| 0012 | [Settings and data compatible with the Python version](0012-storage-compatibility.md) | Accepted, amended by 0016 and 0021 |
| 0013 | [Quality gates: tests without hardware, sanitizers, fuzzing, CI](0013-quality-gates.md) | Accepted |
| 0014 | [Version from git tags; packages with CPack](0014-versioning-and-packaging.md) | Accepted, extended by 0020 |
| 0015 | [The language is chosen in a drop-down list; the system language is not used](0015-explicit-language-choice.md) | Accepted |
| 0016 | [On Windows the app sets an organization name for QSettings](0016-windows-organization-name.md) | Accepted |
| 0017 | [Layered architecture with presentation models in the core; no MVVM, no QML](0017-layers-and-presentation-models.md) | Accepted |
| 0018 | [Navigation and satellite models run in the reader thread and publish snapshots](0018-receiver-model-in-reader-thread.md) | Rejected after measurement |
| 0019 | [macOS packages are signed with a Developer ID and notarized; ad hoc signing stays the default](0019-developer-id-signing.md) | Accepted |
| 0020 | [Windows packages: an MSI installer built by CPack WiX, next to the ZIP](0020-windows-msi-installer.md) | Accepted |
| 0021 | [measurements.json carries a format version; unreadable files are set aside, not overwritten](0021-measurements-format-version.md) | Accepted |

## Writing a new record

1. Copy [template.md](template.md) to `NNNN-short-title.md` with the next number.
2. Fill in the context, the decision, the alternatives and the consequences. Keep it short: one decision per record.
3. Add a line to the table above.
4. A decision is never edited away. When it changes, write a new record and set the old one's status to `Superseded by NNNN`.
