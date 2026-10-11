# 0017. Layered architecture with presentation models in the core; no MVVM, no QML

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

The question came up whether UCenter should adopt a layered architecture combined with MVVM, as commonly recommended for Qt applications with a QML interface. The code is already split into a core and a UI ([0004](0004-core-library.md)), and the interface is built with Qt Widgets ([0002](0002-cpp-and-qt-widgets.md)), including a map drawn by hand ([0008](0008-painted-tile-map.md)). MVVM pays off through declarative bindings — the view follows properties of the view model without glue code — and Qt Widgets has no such bindings.

## Decision

The app keeps four layers with dependencies pointing down only:

| Layer | Contents |
|---|---|
| Presentation | `src/ui/`: tabs, `MainWindow`, the map widgets |
| Application | `ReceiverSession`: connection, commands and ACKs, recording |
| Domain | `nmea`, `ubx`, `navigation`, `satellites`, `geo`, `baseline`, `points`, `receiver_config` |
| Infrastructure | `worker`, `source`, `replay`, `frame_reader`, `ports`, `baseline_store`, `host_location`; `tile_loader`, `geocoder`, `network` in `src/ui/` |

- The boundary that matters — nothing below the presentation layer uses widgets or QtGui — is enforced by the build: `ucenter_core` does not link `Qt6::Widgets` or `Qt6::Gui`.
- Infrastructure that produces GUI types stays in `src/ui/`: `TileLoader` returns `QPixmap`, which the core cannot use.
- From MVVM the app takes one idea: **presentation models**. The state a screen shows, and the rules that change it, live in plain core classes — `NavigationState`, `SatelliteTracker`, `MeasurementPoints` — which hand out snapshots (`NavData`, `SatelliteSnapshot`). Widgets display a snapshot and report user actions through signals; they hold no rules of their own.
- There is no view-model layer with property bindings, and the interface stays on Qt Widgets.

## Alternatives considered

- **Full MVVM on Qt Widgets** — every property would need hand-written synchronization between the view model and the widget: the glue code that MVVM exists to remove, plus one more layer.
- **Move the interface to QML to get MVVM** — a rewrite of all six tabs, the theme and the map with its marker dragging (as a `QQuickPaintedItem` or from scratch). The gains — animation, touch, separate work by a designer — are not needed by a desktop tool for a receiver.
- **Split the core into separate domain and infrastructure libraries** — makes the domain layer provably free of I/O, but at about three thousand lines of core code the extra targets cost more than they protect. To revisit if the core grows considerably or a second consumer such as a CLI appears.

## Consequences

- No restructuring is needed; the existing split is the layered architecture, now documented with names.
- New screen logic goes into a presentation model in the core, with a test, before the widget that shows it — as 0004 already asks.
- If a QML interface is ever built, the presentation models become its view models by adding `Q_PROPERTY` declarations and change signals; the domain and infrastructure stay as they are.
- A further step — one model per receiver running in the reader thread — was proposed in [0018](0018-receiver-model-in-reader-thread.md) and rejected after measurement: the models are too cheap for it to matter.
