# Roadmap

Planned improvements after 1.0.0, in the order they should be done. Each step lists what it changes, how to tell it is done, and a rough size (S — hours, M — about a day, L — several days). Architectural steps refer to their [decision records](adr/README.md).

Principle: **measure, then optimize.** No performance work starts without a number to beat, and every optimization ends with the same measurement repeated.

## Phase 0 — Housekeeping (before anything else)

| # | Step | Done when | Size |
|---|---|---|---|
| 0.1 | Move `measurements.json` on Windows: on startup, if the file is missing in the new data directory but present in `%APPDATA%\UCenter`, move it ([0016](adr/0016-windows-organization-name.md)) | A test with a temporary `APPDATA` shows the file moved once and never overwritten | S |
| 0.2 | Delete merged branches, locally and on GitHub | `git branch -a` shows only `main` and work in progress | S |
| 0.3 | Update GitHub Actions to versions on Node 24 (`actions/checkout`, `upload-artifact`, `download-artifact`) | No deprecation annotations in a CI run | S |

## Phase 1 — A baseline to measure against

| # | Step | Done when | Size |
|---|---|---|---|
| 1.1 | A reference recording: 10 minutes at 10 Hz with NMEA, NAV-PVT, NAV-SAT and NAV-DOP all enabled, committed under `tests/data/` | The file replays in the app and in `test_session` | S |
| 1.2 | Benchmarks in the core (`QBENCHMARK`): `FrameReader` on the reference recording, `NavigationState::apply`, `SatelliteTracker::handle` and `snapshot` | Numbers are printed by a `bench_core` target, not run in CI by default | M |
| 1.3 | GUI-thread load: replay the reference recording at 1×, 4× and maximum speed and record the busy time of the GUI thread (Instruments on macOS, the Visual Studio profiler on Windows) and the peak memory | A table of the results is added to this file | M |

## Phase 2 — Computation off the GUI thread ([0018](adr/0018-receiver-model-in-reader-thread.md))

| # | Step | Done when | Size |
|---|---|---|---|
| 2.1 | `ReceiverSession` delivers frames as one batch per read instead of one signal per frame | Console and models are fed from batches; tests pass | S |
| 2.2 | Refresh only the visible tab; a tab takes the latest snapshot when shown | No Dashboard or satellite redraws while another tab is open | S |
| 2.3 | `ReceiverModel` in the reader thread publishing `NavData` (≤ 10 Hz, on change) and `SatelliteSnapshot` (1 Hz); `MainWindow` holds no receiver state | 1.3 repeated: GUI-thread busy time at maximum replay speed drops substantially; 0018 becomes Accepted | M |

## Phase 3 — Interface

| # | Step | Done when | Size |
|---|---|---|---|
| 3.1 | Console as a model and view: a ring buffer of `GnssMessage` in a `QAbstractListModel`, a `QListView` with uniform row heights, filters and freeze as a proxy model; text built only for visible rows | Filter switching is instant with a full buffer; the buffer can be raised from 5 000 to 50 000 messages without slowing the UI | M |
| 3.2 | Console details: selecting a row shows the decoded fields of the message | Works for every type in `message_types.h` | M |
| 3.3 | Decode map tiles in a thread pool (`QImage`), convert to `QPixmap` in the GUI thread — **only if** 1.3 shows decoding as a visible cost while panning | Panning over imagery at Retina resolution has no frames above 16 ms in the profiler | M |

## Phase 4 — CI and delivery

| # | Step | Done when | Size |
|---|---|---|---|
| 4.1 | Split the Linux job into parallel jobs: format, sanitizers with tests, clang-tidy, fuzzing | Wall time of a CI run is set by the slowest job (about 4 minutes instead of 10) | S |
| 4.2 | `ccache` for the sanitizer and fuzzer builds | A rebuild without source changes takes under a minute | S |
| 4.3 | Fuzz for 30 s on branches, 120 s on `main` and tags | Configured in the workflow | S |
| 4.4 | macOS: sign with a Developer ID certificate and notarize the DMG | The app opens on a clean Mac without a Gatekeeper warning | M |
| 4.5 | Windows: an installer (CPack with WiX or NSIS) next to the ZIP, with a Start menu entry | Installs and uninstalls cleanly on a clean Windows | M |

## Phase 5 — Data and compliance

| # | Step | Done when | Size |
|---|---|---|---|
| 5.1 | A `version` field in `measurements.json` with a migration path, as foreseen in [0012](adr/0012-storage-compatibility.md); a new decision record | Old files load; new files carry the version | S |
| 5.2 | Resolve the use of Google tiles before wider distribution ([0008](adr/0008-painted-tile-map.md)): remove the source, or put it behind an explicit opt-in | A decision record and the README reflect the choice | S |

## Not planned

- **A QML interface** — see [0017](adr/0017-layers-and-presentation-models.md).
- **Switching the language without reopening the window** — the language changes rarely, and retranslating every widget costs more than reopening ([0010](adr/0010-localization.md), [0015](adr/0015-explicit-language-choice.md)).
