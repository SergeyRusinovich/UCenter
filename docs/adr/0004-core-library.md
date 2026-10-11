# 0004. GUI-independent core library

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

Most of the logic — protocol parsing, the navigation solution, satellite tracking, geodesy, the rules for placing points A/B, storage — does not depend on widgets. Tests, the fuzzer and future tools (a CLI, a replay analyzer) need that logic without a GUI.

## Decision

The code is split into `src/core` and `src/ui`:

- `core` is a static library (`ucenter_core`) that uses only QtCore, QtSerialPort and QtPositioning — no widgets, no QtGui;
- `ui` includes `core`, never the reverse;
- rules live in `core` as plain classes (e.g. `MeasurementPoints`, `NavigationState`, `SatelliteTracker`), the UI only displays their state;
- widgets talk through signals; `MainWindow` and `AzimuthTab` are the composition roots that create and connect them; dependencies such as the port list or the visible map bounds are injected as functions.

## Alternatives considered

- **One executable target with all sources** — tests and the fuzzer would have to link widgets and create a `QApplication`.
- **Logic inside widgets** — quicker to write, but untestable without a display and duplicated between views.

## Consequences

- `test_core`, `test_session` and `fuzz_frame_reader` link only `ucenter_core` and run headless.
- New logic goes to `core` first, with a test; a widget that needs a rule asks the core class instead of implementing it.
- `core` must not include widget headers; the CMake target enforces it by not linking `Qt6::Widgets`.
