# 0018. Navigation and satellite models run in the reader thread and publish snapshots

- **Status:** Rejected after measurement (see Outcome)
- **Date:** 2026-10-11

## Context

Frames are decoded in the reader thread ([0005](0005-reader-thread-and-sources.md), [0006](0006-decode-once.md)), but everything after decoding happens in the GUI thread. `ReceiverSession` receives a batch per read and re-emits it as one `message` signal per frame; `MainWindow::onMessage` appends each frame to the console and applies it to `NavigationState` and `SatelliteTracker`. Two timers then copy the state to the screens: Dashboard and the console every 100 ms, satellites every second — whether or not those tabs are visible.

At 10 Hz with NMEA and UBX both enabled this is hundreds of signal calls and model updates per second in the GUI thread, and replay at more than real-time speed (`ReplayWorker` already takes a `speed` factor) multiplies it. `MainWindow`, meant to be only the composition root ([0004](0004-core-library.md)), also owns the receiver state.

## Decision

- A new core class, `ReceiverModel`, owns `NavigationState` and `SatelliteTracker` and lives in the reader thread next to the `ReceiverSource`. It applies every batch as it is decoded.
- The model publishes snapshots to the GUI thread instead of messages: `NavData` at most every 100 ms and only when it changed, `SatelliteSnapshot` every second. Consumers receive them through queued signals of `ReceiverSession`.
- The console keeps receiving frames, as one batch per read (`messages(QList<GnssMessage>)`) instead of one signal per frame.
- Session-level handling of ACK/NAK and `CFG-VALGET` stays in `ReceiverSession`.
- A screen refreshes only while it is visible; a hidden tab takes the latest snapshot when it is shown.
- `MainWindow` connects the session to the tabs and holds no receiver state.

## Alternatives considered

- **Keep the models in the GUI thread, only batch the signals** — removes the per-frame signal overhead but leaves all model work on the GUI thread; a reasonable first step if this record turns out to be too large.
- **A separate computation thread between the reader and the GUI** — one more hop and one more queue with no benefit: decoding and model updates are cheap compared with the cost of a context switch per batch.
- **Leave it as it is** — no measured problem yet; the cost of the current design grows with output rate and replay speed, and the ownership of state by `MainWindow` contradicts 0004.

## Consequences

- The GUI thread only paints; output rate and replay speed no longer affect UI responsiveness.
- `ReceiverModel` is tested headless, like the other core classes; `test_session` checks that snapshots arrive and are throttled.
- The models are touched from one thread only, the reader thread; snapshots are copied across threads, so `NavData` and `SatelliteSnapshot` must stay plain value types.
- Before acceptance, the GUI-thread load is measured on a 10 Hz recording, so the change can be verified rather than assumed (see the [roadmap](../roadmap.md)).

## Outcome

Rejected on 2026-10-11, after the baseline measurements of the [roadmap](../roadmap.md#baseline-measurements). On the reference 10 Hz stream with every message enabled, the navigation and satellite models take about 2 µs per epoch together, and message handling plus console text formatting do not show up in the profile of the GUI thread at all; the whole 10-minute recording is processed in 0.7 s of CPU at maximum replay speed. The GUI thread is busy painting: the map on Dashboard and the text of the console. Moving the models would add a thread boundary and change ownership for no measurable gain, so the work goes into painting instead (roadmap phase 2). `MainWindow` keeps the models; the concern about it owning state is noted but not worth a restructuring on its own.
