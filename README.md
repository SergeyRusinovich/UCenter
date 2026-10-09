# UCenter

A lightweight, cross-platform desktop alternative to u-center for working with u-blox M9 GNSS receivers over a native USB (CDC-ACM) serial port. Built with Python, PyQt6, pyserial and pyubx2.

## Features

- **Connection panel** – auto-detects ports (`/dev/cu.usbmodem*` on macOS), selectable baud rate (9600 / 38400 / 115200), connect/disconnect button, status indicator and session uptime.
- **Console** – read-only live stream of raw NMEA and decoded UBX messages with _Freeze_, _NMEA only_ and _UBX only_ filters and auto-scroll.
- **Dashboard** – latitude, longitude, altitude, UTC time, fix type (No Fix, 2D, 3D, GNSS+DGNSS), PDOP/HDOP, number of satellites used, speed and course; local time with a selectable UTC offset; a compass (vehicle heading when the receiver provides it, course over ground otherwise) and an OpenStreetMap map with the current position.
- **Satellites** – sky plot (azimuth/elevation), per-satellite C/N0 bar chart, per-constellation filters with used/visible counters. Outline colors: blue = tracked but not used in the solution, red = not tracked.
- **Azimuth** – satellite map for measuring a baseline A→B: azimuth and deviation from true north, distance and offsets, with place search, saved measurements and CSV export. See [Azimuth tab](#azimuth-tab).
- **Configuration** – sends `UBX-CFG-VALSET` (M9 Configuration Database) to change the navigation rate (`CFG-RATE-MEAS`: 1 / 5 / 10 Hz), enable/disable GPS, GLONASS, Galileo and BeiDou, and enable `UBX-NAV-PVT` output on USB. Settings are written to the RAM and BBR layers. Each VALSET waits for `UBX-ACK-ACK`/`ACK-NAK`; current values are read back with `UBX-CFG-VALGET` on connect, after every change and on demand.

## Azimuth tab

Measures a baseline from point A to point B on the WGS84 ellipsoid (Vincenty inverse formula).

**Points**
- By default point A is the location of the computer running UCenter (OS location services, not the GNSS receiver) and follows it live; the accuracy circle is drawn around it. A click on the map places point B.
- Without the computer's location the first click places A and the second places B.
- Markers can be dragged and coordinates typed in. Moving or editing A detaches it from the computer's location; the crosshair button (⌖) centers the map on that location and returns A there.
- If the location is unavailable, a dot on the crosshair shows it (red — no access, amber — still locating). Clicking the crosshair then opens a compact pop-up with buttons to open the OS location settings and to retry.

**Result** – coordinates of both points, azimuth A→B and back azimuth B→A from true north, signed deviation from true north (+ east / − west), distance, north/east offsets.

**Place search** (Russian; OpenStreetMap Nominatim)
- Type an address, street, city or landmark and press Enter. Results nearest to the visible map come first; segments of one street are merged into one result.
- The chosen place is highlighted (fill or outline) and marked with a flag and its name. The flag can be dragged to the exact spot.
- **→ A** / **→ B** put point A or B on the flag.

**Map**
- Imagery provider: Esri World Imagery, Google satellite or OpenStreetMap; the choice is remembered.
- The expand button shows the map over the whole window while the saved measurements stay visible below; the result and **Save** move onto the map. Esc or the same button returns to the normal view.

**Saved measurements** – **Save** stores the result and starts the next measurement (B is cleared, A returns to the computer's location if it follows it). Measurements are kept between runs (`measurements.json` in the app data directory). **Show on map** (or a double-click) shows a measurement with the whole A–B segment in view; rows can be deleted and exported to CSV.

## Requirements

- Python 3.10+
- Dependencies from [requirements.txt](requirements.txt): `PyQt6`, `pyserial`, `pyubx2`

## Quick start

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python main.py
```

### Computer location on macOS

macOS gives location only to an app bundle whose `Info.plist` declares why it needs it, so `python main.py` cannot get the computer's location. Build a small wrapper once and start UCenter from it:

```bash
./tools/make_macos_app.sh   # creates UCenter.app (runs main.py from this folder with .venv)
open UCenter.app
```

On first use of the Azimuth tab macOS asks for permission. Location Services must be on: System Settings → Privacy & Security → Location Services. Without a GPS chip a Mac estimates its location from Wi-Fi, typically to tens of meters; the accuracy is shown next to the device location. Rebuild the wrapper after recreating `.venv` or moving the folder. On Windows location comes from Windows location services (Settings → Privacy & security → Location).

Select the port (for example `/dev/cu.usbmodem1301`; the number depends on the USB socket, and the list picks up a newly plugged receiver automatically every 2 seconds), choose the baud rate and press **Connect**. For a native USB port the baud rate is ignored by the receiver, so any value works.

## Tests

Logic without GUI (geodesy, baseline, storage, place parsing, point rules) is covered by unit tests on the standard library `unittest`:

```bash
python -m unittest discover -s tests
```

## Architecture

The code is split into a GUI-independent core and a thin UI layer. Dependencies point one way only: `ui` imports `core`, never the reverse, and there are no import cycles. The UI never talks to the serial port directly: all receiver I/O goes through `ReceiverSession`, and `MainWindow` only assembles the components and connects their signals. `AzimuthTab` does the same for its own widgets.

```
main.py                      Entry point
tools/make_macos_app.sh      Builds UCenter.app (macOS wrapper needed for location access)
tests/test_core.py           Unit tests for the core logic (unittest)
ucenter/
  core/                      No widgets; QtCore (thread, signals, timer) and QtPositioning only
    ports.py                 Serial port discovery
    worker.py                GNSSWorker (QThread): reads the port, parses via UBXReader, emits signals
    session.py               ReceiverSession: worker lifecycle, CFG-VALSET/ACK/VALGET transactions
    receiver_config.py       CFG keys, rate options, VALSET/VALGET building, ACK and VALGET decoding
    navigation.py            NavigationState -> NavData (typed snapshot) from NAV-PVT / GGA / GSA / RMC
    satellites.py            Satellite, SatelliteTracker (GSV/GSA), per-constellation counts
    messages.py              UBX/NMEA classification and console text
    timeutil.py              hh:mm:ss / uptime formatting, UTC offsets
    geo.py                   Web Mercator projection, WGS84 inverse problem (Vincenty)
    baseline.py              Baseline A->B: azimuth, deviation from true north, offsets, formatting
    baseline_store.py        Saved measurements: JSON between runs, CSV export
    points.py                MeasurementPoints: points A/B and the rules for placing them
    places.py                Place search results: Nominatim parsing, merging segments, sorting by distance
    host_location.py         HostLocation: this computer's location (QtPositioning + location permission)
  ui/
    main_window.py           MainWindow: composition and signal wiring only
    connection_panel.py      ConnectionPanel (port list provider is injected)
    console_tab.py           ConsoleTab
    dashboard_tab.py         DashboardTab: formats NavData for display
    compass_view.py          CompassView
    satellites_tab.py        Satellites tab with filters
    satellite_views.py       SkyView, SignalView (custom painting)
    filters.py               Constellation filter checkboxes with counters
    config_tab.py            ConfigTab
    azimuth_tab.py           AzimuthTab: composition and signal wiring of the Azimuth widgets
    measurement_panels.py    PointsEditor, ResultPanel, ResultOverlay
    saved_measurements.py    SavedMeasurementsPanel: table, persistence, delete, CSV export
    place_search_bar.py      PlaceSearchBar: search field, results menu, → A / → B (map bounds injected)
    imagery_picker.py        ImageryPicker: imagery provider choice, remembered in QSettings
    location_popover.py      LocationPopover: why the computer's location is unavailable
    map_view.py              MapView: tiles, pan/zoom, click signal, fit to bounds
    baseline_map_view.py     BaselineMapView: A/B markers, A->B line, found place with a flag, location alert
    map_buttons.py           Round map buttons with theme-colored icons (crosshair, expand)
    tile_loader.py           TileLoader and tile sources (OSM, Esri, Google): memory/disk cache, retry backoff
    geocoder.py              PlaceSearch: Nominatim requests (Russian, 1 request/s)
    network.py               Shared User-Agent and request setup for map services
    painting.py              Shared drawing helpers
    theme.py                 Satellite colors
    style.py                 App look: light/dark palettes following the OS, system accent color, QSS
    assets/                  SVG icons used by the stylesheet (combo arrow, checkmark)
```

### Data flow

```
serial port -> GNSSWorker (QThread) -> ReceiverSession --message(parsed, raw)--> MainWindow
                                         |                                       |-> ConsoleTab.append_message (buffer)
                                         |                                       |-> NavigationState
                                         |                                       `-> SatelliteTracker
                                         |--config_values--> ConfigTab
                                         |--status / error--> status bar
                                         `--connected_changed--> ConnectionPanel, ConfigTab
100 ms timer: NavigationState.data -> DashboardTab, ConsoleTab.flush()
1 s timer:    SatelliteTracker.snapshot() -> SatellitesTab

ConfigTab --valset_requested--> ReceiverSession.apply_config() -> CFG-VALSET, waits for ACK/NAK
ConfigTab --refresh_requested--> ReceiverSession.poll_config() -> CFG-VALGET

Azimuth tab (AzimuthTab wires the widgets; MeasurementPoints holds the state):
HostLocation --position--> MeasurementPoints.device_moved() -> PointsEditor, BaselineMapView
             --status----> BaselineMapView.set_location_alert() (crosshair dot, pop-up)
BaselineMapView --clicked / point_moved / recentered--> MeasurementPoints -> PointsEditor, map, ResultPanel
PointsEditor --point_edited--> MeasurementPoints
PlaceSearchBar --place_changed--> BaselineMapView.set_place()   --point_requested--> MeasurementPoints
ResultPanel / ResultOverlay --save_requested--> SavedMeasurementsPanel.add() -> measurements.json
SavedMeasurementsPanel --show_requested--> MeasurementPoints.load() -> map.fit_bounds()
AzimuthTab --expand_changed--> MainWindow (hides the connection panel)
```

### Coupling and cohesion

- **One job per module.** Each module has a single reason to change: geodesy (`geo`), the measurement (`baseline`), its storage (`baseline_store`), placement rules (`points`), search results (`places`); in the UI each panel owns its widgets and behavior (`saved_measurements` also owns persistence and export).
- **Rules live in `core`.** How points A/B are placed, when A follows the computer's location and what Save/Clear do is in `MeasurementPoints`, without Qt, and is unit-tested. The UI only displays the state.
- **Widgets talk through signals.** Panels do not know each other; tabs (`AzimuthTab`, `MainWindow`) connect them. Widgets offer methods instead of exposing internals — for example `BaselineMapView.set_expanded()`, `stop_following()`, `show_location_help()`.
- **Dependencies are injected.** `ConnectionPanel` gets the port list function and `PlaceSearchBar` gets the visible-map-bounds function, so neither depends on the component that provides them.
- **Shared code has one place.** Network request setup is in `network.py`; azimuth and deviation formatting is in `baseline.py`.
- `AzimuthTab` and `MainWindow` import many modules on purpose: they are composition roots, which keeps every other module narrow.

### Design notes

- Reading happens in a separate `QThread`, so the UI never blocks. Port errors (busy port, device unplugged) are reported through the `error` signal. Disconnect cancels the pending read, so it does not wait for the port timeout.
- The UI uses the Fusion style on every platform plus a stylesheet from `ucenter/ui/style.py`, so it looks the same on macOS and Windows. The light or dark palette follows the system setting and switches live; the accent color is taken from the OS. Widgets do not set inline styles: they use object names (`console`, `statusLed`, `mapButton`) and the dynamic properties `role` and `variant`, which the stylesheet targets.
- Incoming messages only update the models; the Dashboard and the console are redrawn by a 100 ms timer, and the console appends lines in batches (only while its tab is visible).
- Writes are protected by a lock, so they can be issued from the GUI thread while the worker is reading.
- Satellites that stop appearing in GSV for 5 seconds are removed from the view. If a satellite is reported on several signals (NMEA 4.10+ `signalID`), the best C/N0 of the current epoch is shown; a signal with an empty C/N0 counts as not tracked.
- `UBX-NAV-PVT`, when present, is the primary source for position, time, fix type, PDOP and satellite count; NMEA GGA/GSA/RMC are used only when NAV-PVT has not been received for 2 seconds. HDOP always comes from GGA.
- Esri imagery is blocked on some networks (HTTP 403); pick another provider on the Azimuth tab then. Google satellite tiles are fetched directly, which Google Maps Platform terms do not allow — use it for personal/internal work only.
- Place search follows the Nominatim usage policy: a request only on Enter (no autocomplete), at most one per second, with a descriptive User-Agent.
- Map tiles are cached in memory and on disk (OS cache directory, up to 200 MB). A tile that fails to load is retried after 10 s, then with a doubling delay up to 5 minutes, to respect the OpenStreetMap tile usage policy.
- "Used in navigation" is derived from NMEA GSA; SBAS is recognized by PRN ranges 33–64 and 120–158 reported under the GP talker.

## Limitations

- Satellite data and HDOP rely on NMEA output (GSV, GSA, GGA), which M9 enables by default. The dashboard also uses `UBX-NAV-PVT` when it is enabled.
- The _Zoom in/out_ slider currently scales satellite markers on the sky plot only.
- No persistent (Flash) configuration layer.
- The computer's location on a Mac without GPS comes from Wi-Fi and is typically accurate to tens of meters, which limits short baselines measured from the computer's position.
