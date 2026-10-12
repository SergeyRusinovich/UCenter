# Roadmap

Planned improvements after 1.0.0, in the order they should be done. Each step lists what it changes, how to tell it is done, and a rough size (S — hours, M — about a day, L — several days). Architectural steps refer to their [decision records](adr/README.md).

Principle: **measure, then optimize.** No performance work starts without a number to beat, and every optimization ends with the same measurement repeated.

## Phase 0 — Housekeeping (before anything else) ✅ done

| # | Step | Done when | Size |
|---|---|---|---|
| 0.1 | ✅ Move `measurements.json` on Windows: on startup, if the file is missing in the new data directory but present in `%APPDATA%\UCenter`, move it ([0016](adr/0016-windows-organization-name.md)) | `test_core` (`legacyFileIsMovedOnce`) shows the file moved once and never overwritten; if the move fails, the app keeps using the old file | S |
| 0.2 | ✅ Delete merged branches, locally and on GitHub | `git branch -a` shows only `main` and work in progress | S |
| 0.3 | ✅ Update GitHub Actions to versions on Node 24 (`actions/checkout@v7`, `upload-artifact@v7`, `download-artifact@v8`) | No deprecation annotations in a CI run | S |

## Phase 1 — A baseline to measure against ✅ done

| # | Step | Done when | Size |
|---|---|---|---|
| 1.1 | ✅ A reference recording: 10 minutes at 10 Hz with NAV-PVT, NAV-DOP, NAV-SAT and NMEA RMC, GGA, GSA, GSV in every epoch. Generated deterministically by `tests/recording.cpp` instead of a file under `tests/data/` (no receiver at hand, and 9 MB of binary data would stay in the history forever); `make_recording` writes it to a file | `test_core` (`referenceRecordingDecodes`) checks that every frame decodes and the replay splits it into epochs | S |
| 1.2 | ✅ Benchmarks in the core (`QBENCHMARK`): `FrameReader`, `NavigationState::apply`, `SatelliteTracker::handle` and `snapshot`, console text | Numbers are printed by `bench_core`, not run in CI | M |
| 1.3 | ✅ App load: `--replay`, `--speed` and `--quit-at-end` options; per-thread CPU time (`ps -M`), peak memory (`time -l`) and call stacks (`sample`) at 1×, 4× and maximum speed with the Console, Dashboard and Satellites tabs open | The results are below | M |

### Baseline measurements

macOS 26.6, Apple M1 Pro, Release build, 11 October 2026. Windows has not been measured yet.

**Core** (`bench_core`, the whole 10-minute recording — 6 000 epochs, 108 000 messages, 9 MB):

| Work | Whole recording | Per epoch | Thread today |
|---|---|---|---|
| Frame assembly and decoding | 183 ms | 30 µs | reader |
| Navigation solution | 3.5 ms | 0.6 µs | GUI |
| Satellite tracker | 7.2 ms | 1.2 µs | GUI |
| Satellite snapshot | — | 0.9 µs (one per second) | GUI |
| Console text | 194 ms | 32 µs | GUI |

**App** (replay of the reference stream; CPU time of the GUI thread and of all threads, peak memory):

| Open tab | Speed | GUI thread busy | All threads | Peak memory |
|---|---|---|---|---|
| Console | 1× | 14.3 % | 14.5 % | 275 MB |
| Console | 4× | 21.6 % | 22.1 % | 273 MB |
| Dashboard | 1× | 16.1 % | **34.5 %** | 262 MB |
| Dashboard | 4× | 17.7 % | 38.8 % | 269 MB |
| Satellites | 1× | 2.0 % | 2.2 % | 230 MB |
| Satellites | 4× | 2.9 % | 3.4 % | 230 MB |
| any | maximum | the whole 10-minute recording in 0.8 s, 0.7 s of CPU | | 385–424 MB |

**What the call stacks show** (`sample`, 10 s at 1×):

- **Dashboard** — `MapView::drawTile` → `QPainter::drawPixmap` → `QRasterPaintEngine::drawImage`: the map is repainted with every position update (10 Hz) and every tile is scaled while it is drawn. Qt spreads smooth scaling over its thread pool, which is the CPU outside the GUI thread.
- **Console** — `QPlainTextEdit::paintEvent`, text shaping (`QTextEngine::shapeText`, HarfBuzz) and block layout on every flush.
- Message handling, the navigation and satellite models and console text formatting together take 0–1 of about 8 000 samples of the GUI thread. Tile decoding does not appear.

**Conclusions.** The cost is in painting, not in computation. The models are four orders of magnitude below the 100 ms epoch budget, so moving them to the reader thread ([0018](adr/0018-receiver-model-in-reader-thread.md)) would not be noticeable — that record is rejected. Hidden tabs cost nothing measurable (the Satellites row), so refreshing only the visible tab is not needed. Phases 2 and 3 are re-planned around the map and the console.

## Phase 2 — Cheaper painting ✅ done

| # | Step | Done when | Size |
|---|---|---|---|
| 2.1 | ✅ Map: tiles kept at the device pixel size in a cache (`MapView::readyTile`) and the map origin snapped to device pixels, so painting is a plain copy; the parent tile is no longer drawn under four loaded children | Dashboard at 1×: the thread pool is idle and all threads together stay under 10 % — **met, 9.9 %** | M |
| 2.2 | ✅ Map: repaint only when the marker moves by at least one device pixel or the view changes | Dashboard GUI thread at 1× under 5 % — **not met, 7.7 %** (see below) | S |
| 2.3 | ✅ Console: refresh the text at most 4 times a second, and not while the window is minimized | Console GUI thread at 1× under 7 % — **not met, 8.6 %**; the rest is text layout in `QPlainTextEdit`, which 3.1 replaces | S |

Two causes that were not in the plan turned up while measuring, both making every map update repaint the whole window:

- `MapView` was not marked opaque, so each of its repaints also repainted every parent widget under it, up to the main window. It now sets `Qt::WA_OpaquePaintEvent`.
- Changing the text of a `QLabel` asks the layout for a new size, and the request travels up to `QMainWindow`, which then repainted the whole window — 16 times a second, about 820 000 pixels each time. Values that change several times a second (Dashboard, the azimuth result) are now a `ValueLabel`, whose size does not depend on its text. With that, the map repaints 2.7 times a second instead of 10.7, i.e. only when the marker actually moves.

### Phase 2 results

Steady state: CPU time between the 20th and the 80th second of a replay of the reference stream, so start-up is excluded. Same machine as the baseline; "before" is `main` before phase 2, measured the same way. The phase 1 table above averaged over the whole run including start-up, which is why its numbers differ slightly.

| Open tab | Speed | Before: GUI thread / all threads | After: GUI thread / all threads |
|---|---|---|---|
| Dashboard | 1× | 15.8 % / 35.2 % | **7.7 % / 9.9 %** |
| Dashboard | 4× | 17.8 % / 41.4 % | 12.9 % / 17.8 % |
| Console | 1× | 17.0 % / 17.1 % | **8.6 % / 8.8 %** |
| Console | 4× | 22.9 % / 23.0 % | 13.2 % / 13.5 % |
| Satellites | 1× | 2.4 % / 2.5 % | 2.3 % / 2.5 % |

The map also looks better: with the origin on device pixels, the thin seams between tiles that were visible on Retina screens are gone.

## Phase 3 — Interface ✅ done on macOS; Windows painting not measured

| # | Step | Done when | Size |
|---|---|---|---|
| 3.1 | ✅ Console as a model and view: `MessageLog` in the core (ring buffer of 50 000 messages, NMEA/UBX filter over the whole history), `ConsoleModel` + `QListView` with uniform rows, text built only for visible rows; copy, full line as a tooltip; a selected line stays in place while the history grows and is kept in the details when it is evicted; Esc follows new lines again | Filter switching with a full history: **0.22 ms** (`bench_core`, `consoleFilterSwitch`) — **met**. History raised from 5 000 to 50 000 with **no measurable memory growth** (291 MB vs 293 MB after two minutes at 4×) — **met**. Console at 1× no slower than after 2.3: **8.7 % vs 8.6 %** — **met** | M |
| 3.2 | ✅ Console details: selecting a row shows the fields of the message — named NMEA fields, decoded UBX fields, the frame in hex (`messages::details`) | `test_core` (`messageDetailsForEveryType`) covers every type in `message_types.h` — **met** | M |
| 3.3 | ◐ Measure on Windows. `--measure 20,80 --measure-out load.txt` (with `--tab N`) reports the GUI-thread and process load and whether the window was visible, on every platform; the **Measure** workflow runs it on GitHub's Windows and macOS runners | On the Windows runner the app replays the stream and processing costs **0.0–0.1 %**, but the runner has no screen, so the window is not painted and painting cannot be measured there (the macOS runner likewise shows Dashboard at 2 % instead of 7.5 %). **A run on a real Windows computer is still needed** for the table | S |
| 3.4 | ✅ Compass repaints only when the heading or course changes by at least 0.5° | Dashboard GUI thread at 1× under 5 % — **not met: 7.5 %** (7.7 % before). The rest is the value labels and the map, not the compass | S |

### Phase 3 results

macOS 26.6, Apple M1 Pro, built-in `--measure 20,80`, the window visible for the whole measurement (`visible=100%`). "Before" is the phase 2 result above.

| Open tab | Speed | Before: GUI thread / all threads | After: GUI thread / all threads |
|---|---|---|---|
| Console | 1× | 8.6 % / 8.8 % | 8.7 % / 8.9 % |
| Console | 4× | 13.2 % / 13.5 % | **9.1 % / 9.5 %** |
| Dashboard | 1× | 7.7 % / 9.9 % | 7.5 % / 9.2 % |
| Dashboard | 4× | 12.9 % / 17.8 % | 13.3 % / 18.1 % |
| Satellites | 1× | 2.3 % / 2.5 % | 2.3 % / 2.5 % |

The console no longer gets more expensive with the data rate: at 4× it costs about the same as at 1×, because only the visible rows are laid out.

Two lessons from measuring, now built into `--measure`:

- macOS neither paints nor runs the timers of a covered window at full rate, so a covered window looks cheap. `--measure` raises the window and reports `visible=…%`; runs below 100 % are discarded.
- Qt 6 closes the windows on quit, which used to save the window size and tab of every measured run into the user's settings. Runs with `--tab` or `--measure` no longer save them.

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
