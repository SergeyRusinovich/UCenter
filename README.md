# UCenter

A lightweight, cross-platform desktop alternative to u-center for working with u-blox M9 GNSS receivers over a native USB (CDC-ACM) serial port. Built with Python, PyQt6, pyserial and pyubx2.

## Features

- **Connection panel** – auto-detects ports (`/dev/cu.usbmodem*` on macOS), selectable baud rate (9600 / 38400 / 115200), connect/disconnect button, status indicator and session uptime.
- **Console** – read-only live stream of raw NMEA and decoded UBX messages with _Freeze_, _NMEA only_ and _UBX only_ filters and auto-scroll.
- **Dashboard** – latitude, longitude, altitude, UTC time, fix type (No Fix, 2D, 3D, GNSS+DGNSS), PDOP/HDOP, number of satellites used, speed and course; local time with a selectable UTC offset; a compass (vehicle heading when the receiver provides it, course over ground otherwise) and an OpenStreetMap map with the current position.
- **Satellites** – sky plot (azimuth/elevation), per-satellite C/N0 bar chart, per-constellation filters with used/visible counters. Outline colors: blue = tracked but not used in the solution, red = not tracked.
- **Configuration** – sends `UBX-CFG-VALSET` (M9 Configuration Database) to change the navigation rate (`CFG-RATE-MEAS`: 1 / 5 / 10 Hz), enable/disable GPS, GLONASS, Galileo and BeiDou, and enable `UBX-NAV-PVT` output on USB. Settings are written to the RAM and BBR layers. Each VALSET waits for `UBX-ACK-ACK`/`ACK-NAK`; current values are read back with `UBX-CFG-VALGET` on connect, after every change and on demand.

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

Select the port (for example `/dev/cu.usbmodem1301`; the number depends on the USB socket, and the list picks up a newly plugged receiver automatically every 2 seconds), choose the baud rate and press **Connect**. For a native USB port the baud rate is ignored by the receiver, so any value works.

## Architecture

The code is split into a GUI-independent core and a thin UI layer. Dependencies point one way only: `ui` imports `core`, never the reverse. The UI never talks to the serial port directly: all receiver I/O goes through `ReceiverSession`, and `MainWindow` only assembles the components and connects their signals.

```
main.py                      Entry point
ucenter/
  core/                      No widgets; only QtCore is used (thread, signals, timer)
    ports.py                 Serial port discovery
    worker.py                GNSSWorker (QThread): reads the port, parses via UBXReader, emits signals
    session.py               ReceiverSession: worker lifecycle, CFG-VALSET/ACK/VALGET transactions
    receiver_config.py       CFG keys, rate options, VALSET/VALGET building, ACK and VALGET decoding
    navigation.py            NavigationState -> NavData (typed snapshot) from NAV-PVT / GGA / GSA / RMC
    satellites.py            Satellite, SatelliteTracker (GSV/GSA), per-constellation counts
    messages.py              UBX/NMEA classification and console text
    timeutil.py              hh:mm:ss / uptime formatting, UTC offsets
    geo.py                   Web Mercator projection
  ui/
    main_window.py           MainWindow: composition and signal wiring only
    connection_panel.py      ConnectionPanel (port list provider is injected)
    console_tab.py           ConsoleTab
    dashboard_tab.py         DashboardTab: formats NavData for display
    compass_view.py          CompassView
    map_view.py              MapView: map painting and mouse/zoom handling
    tile_loader.py           TileLoader: OSM tile download, memory/disk cache, retry backoff
    satellites_tab.py        Satellites tab with filters
    satellite_views.py       SkyView, SignalView (custom painting)
    filters.py               Constellation filter checkboxes with counters
    painting.py              Shared drawing helpers
    theme.py                 Satellite colors
    style.py                 App look: light/dark palettes following the OS, system accent color, QSS
    assets/                  SVG icons used by the stylesheet (combo arrow, checkmark)
    config_tab.py            ConfigTab
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
```

### Design notes

- Reading happens in a separate `QThread`, so the UI never blocks. Port errors (busy port, device unplugged) are reported through the `error` signal. Disconnect cancels the pending read, so it does not wait for the port timeout.
- The UI uses the Fusion style on every platform plus a stylesheet from `ucenter/ui/style.py`, so it looks the same on macOS and Windows. The light or dark palette follows the system setting and switches live; the accent color is taken from the OS. Widgets do not set inline styles: they use object names (`console`, `statusLed`, `mapButton`) and the dynamic properties `role` and `variant`, which the stylesheet targets.
- Incoming messages only update the models; the Dashboard and the console are redrawn by a 100 ms timer, and the console appends lines in batches (only while its tab is visible).
- Writes are protected by a lock, so they can be issued from the GUI thread while the worker is reading.
- Satellites that stop appearing in GSV for 5 seconds are removed from the view. If a satellite is reported on several signals (NMEA 4.10+ `signalID`), the best C/N0 of the current epoch is shown; a signal with an empty C/N0 counts as not tracked.
- `UBX-NAV-PVT`, when present, is the primary source for position, time, fix type, PDOP and satellite count; NMEA GGA/GSA/RMC are used only when NAV-PVT has not been received for 2 seconds. HDOP always comes from GGA.
- Map tiles are cached in memory and on disk (OS cache directory, up to 200 MB). A tile that fails to load is retried after 10 s, then with a doubling delay up to 5 minutes, to respect the OpenStreetMap tile usage policy.
- "Used in navigation" is derived from NMEA GSA; SBAS is recognized by PRN ranges 33–64 and 120–158 reported under the GP talker.

## Limitations

- Satellite data and HDOP rely on NMEA output (GSV, GSA, GGA), which M9 enables by default. The dashboard also uses `UBX-NAV-PVT` when it is enabled.
- The _Zoom in/out_ slider currently scales satellite markers on the sky plot only.
- No persistent (Flash) configuration layer.
