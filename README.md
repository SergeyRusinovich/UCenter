# UCenter

A lightweight, cross-platform desktop alternative to u-center for working with u-blox M9 GNSS receivers over a native USB (CDC-ACM) serial port. Built with C++20 and Qt 6 (Widgets, Network, SerialPort, Positioning); the NMEA and UBX protocols are parsed by the app itself, without third-party libraries. The interface is available in English, Russian, Spanish and Simplified Chinese.

## Features

- **Connection panel** – auto-detects ports (`/dev/cu.usbmodem*` on macOS), selectable baud rate (9600 / 38400 / 115200), connect/disconnect button, status indicator and session uptime. The last port and baud rate are remembered.
- **Recording and replay** – **● Record…** saves the raw stream from the receiver to a `.ubx` file (every byte as received). **▶ From file…** plays a recording back instead of a receiver, at the original epoch pace, so every tab works without hardware. See [Recording and replay](#recording-and-replay).
- **Console** – read-only live stream of raw NMEA and decoded UBX messages with _Freeze_, _NMEA only_ and _UBX only_ filters and auto-scroll.
- **Dashboard** – latitude, longitude, altitude, UTC time, fix type (No Fix, 2D, 3D, GNSS+DGNSS), PDOP/HDOP, number of satellites used, speed and course; local time with a selectable UTC offset; a compass (vehicle heading when the receiver provides it, course over ground otherwise) and an OpenStreetMap map with the current position.
- **Satellites** – sky plot (azimuth/elevation), per-satellite C/N0 bar chart, per-constellation filters with used/visible counters. Outline colors: blue = tracked but not used in the solution, red = not tracked. Data comes from `UBX-NAV-SAT` when it is enabled, otherwise from NMEA GSV/GSA.
- **Azimuth** – satellite map for measuring a baseline A→B: azimuth and deviation from true north, distance and offsets, with place search, saved measurements and CSV export. See [Azimuth tab](#azimuth-tab).
- **Configuration** – sends `UBX-CFG-VALSET` (M9 Configuration Database) to change the navigation rate (`CFG-RATE-MEAS`: 1 / 5 / 10 Hz), enable/disable GPS, GLONASS, Galileo and BeiDou, and choose the USB output: `UBX-NAV-PVT`, `UBX-NAV-SAT`, `UBX-NAV-DOP` and NMEA. Settings are written to the RAM and BBR layers, and to Flash when _Also save changes to Flash_ is on. **Revert to factory defaults…** clears the saved configuration with `UBX-CFG-CFG` (after a confirmation). Every command waits for `UBX-ACK-ACK`/`ACK-NAK`; current values are read back with `UBX-CFG-VALGET` on connect, after every change and on demand.

- **Language** – the drop-down list in the top-right corner of the tab bar switches between English, Русский, Español and 中文（简体）. The system language is not used: the interface is in English until another language is chosen, and the choice is remembered. The window reopens in the new language right away; if a receiver is connected, the app asks first because the connection is closed.

## Azimuth tab

Measures a baseline from point A to point B on the WGS84 ellipsoid (Vincenty inverse formula).

**Points**
- By default point A is the location of the computer running UCenter (OS location services, not the GNSS receiver) and follows it live; the accuracy circle is drawn around it. A click on the map places point B.
- Without the computer's location the first click places A and the second places B.
- Markers can be dragged and coordinates typed in. Moving or editing A detaches it from the computer's location; the crosshair button (⌖) centers the map on that location and returns A there.
- If the location is unavailable, a dot on the crosshair shows it (red — no access, amber — still locating). Clicking the crosshair then opens a compact pop-up with buttons to open the OS location settings and to retry.

**Result** – coordinates of both points, azimuth A→B and back azimuth B→A from true north, signed deviation from true north (+ east / − west), distance, north/east offsets.

**Place search** (results in Russian; OpenStreetMap Nominatim)
- Type an address, street, city or landmark and press Enter. Results nearest to the visible map come first; segments of one street are merged into one result.
- The chosen place is highlighted (fill or outline) and marked with a flag and its name. The flag can be dragged to the exact spot.
- **→ A** / **→ B** put point A or B on the flag.

**Map**
- Imagery provider: Esri World Imagery, Google satellite or OpenStreetMap; the choice is remembered, as are the map position, zoom and the split between the map and the table.
- The mouse wheel and the trackpad zoom around the cursor; horizontal scrolling does not change the zoom. On Retina screens satellite imagery is drawn from the next zoom level, at full screen resolution.
- The expand button shows the map over the whole window while the saved measurements stay visible below; the result and **Save** move onto the map. Esc or the same button returns to the normal view.

**Saved measurements** – **Save** stores the result and starts the next measurement (B is cleared, A returns to the computer's location if it follows it). Measurements are kept between runs (`measurements.json` in the app data directory). **Show on map** (or a double-click) shows a measurement with the whole A–B segment in view; rows can be deleted and exported to CSV.

## Recording and replay

- A recording is the raw byte stream from the port, as u-center saves it (`.ubx`), so recordings from u-center can be replayed too.
- Replay paces epochs by the time inside the recording: `iTOW` of `UBX-NAV-PVT` when the recording has it, otherwise the UTC time of NMEA GGA/RMC. Gaps longer than 2 seconds are skipped. Without any time the stream is played in chunks of 20 messages every 100 ms.
- During replay the Configuration tab is disabled — there is no receiver to send commands to. When the recording ends the session closes by itself.

## Requirements

- CMake 3.21+ and a C++20 compiler (Apple Clang / Xcode Command Line Tools, MSVC 2019+, GCC 11+, Clang 15+)
- Qt 6.7+ with the modules Core, Gui, Widgets, Network, SerialPort, Positioning, Svg, LinguistTools (and Test for the tests)

On macOS Qt can be installed with Homebrew (`brew install qt cmake`) or the Qt Online Installer; on Windows — with the Qt Online Installer.

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/macos   # prefix: where Qt is installed
cmake --build build -j
open build/UCenter.app             # macOS
build\Release\UCenter.exe          # Windows (multi-config generators)
```

`CMAKE_PREFIX_PATH` is not needed when Qt is found by CMake on its own (for example Homebrew Qt).

### Packages for other computers

`cpack` installs the app together with Qt (`macdeployqt` / `windeployqt`) and packs it: a drag-and-drop DMG on macOS, a ZIP on Windows.

```bash
cpack --config build/CPackConfig.cmake -B build/package
```

The version comes from git: a tag `v1.2.3` gives `1.2.3`, commits after the tag and uncommitted changes are added to the full version shown in the title bar (`1.2.3-4-gabc1234-dirty`). Without a tag the base version from [cmake/version.cmake](cmake/version.cmake) is used. The version is read when CMake configures the build, so reconfigure after creating a tag.

### Computer location on macOS

macOS gives location only to an app bundle whose `Info.plist` declares why it needs it. The build produces such a bundle (`build/UCenter.app`, with `NSLocationUsageDescription` from [cmake/Info.plist.in](cmake/Info.plist.in)) and signs it ad hoc, so macOS can remember the permission.

On first use of the Azimuth tab macOS asks for permission. Location Services must be on: System Settings → Privacy & Security → Location Services. Without a GPS chip a Mac estimates its location from Wi-Fi, typically to tens of meters; the accuracy is shown next to the device location. On Windows location comes from Windows location services (Settings → Privacy & security → Location).

Select the port (for example `/dev/cu.usbmodem1301`; the number depends on the USB socket, and the list picks up a newly plugged receiver automatically every 2 seconds), choose the baud rate and press **Connect**. For a native USB port the baud rate is ignored by the receiver, so any value works.

## Development

### Tests

```bash
ctest --test-dir build --output-on-failure
```

- `test_core` — logic without GUI: geodesy, baseline, storage, place parsing, point rules, NMEA/UBX parsing, navigation solution, satellite tracking, replay pacing, and a parser robustness check on randomly corrupted streams. UBX frames are checked against reference frames produced by pyubx2, which the earlier Python version used.
- `test_session` (macOS and Linux) — end to end without a receiver: a pseudo-terminal (`openpty`) stands in for the USB port, and the real `ReceiverSession`, reader thread and `QSerialPort` are tested on it: receiving frames, VALSET → ACK, a missing ACK, recording, replay, a port that cannot be opened, reconnecting.

### Sanitizers and fuzzing

```bash
# Tests under AddressSanitizer and UBSan
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j && ctest --test-dir build-asan

# libFuzzer for the frame reader (needs clang with libFuzzer, e.g. Homebrew LLVM on macOS)
cmake -S . -B build-fuzz -DUCENTER_FUZZ=ON -DCMAKE_CXX_COMPILER=$(brew --prefix llvm)/bin/clang++ \
      -DUCENTER_FUZZ_SANITIZERS=undefined
cmake --build build-fuzz --target fuzz_frame_reader
build-fuzz/fuzz_frame_reader -max_total_time=120 new-corpus fuzz/corpus
```

On macOS 26 the AddressSanitizer runtime of LLVM 19 hangs at start-up, so the fuzzer is built there with `UCENTER_FUZZ_SANITIZERS=undefined`; CI fuzzes on Linux with ASan and UBSan.

### Code style and static analysis

- `tools/format.sh` formats the code with clang-format ([.clang-format](.clang-format)); `tools/format.sh --check` only checks.
- `tools/clang_tidy.sh [build dir]` runs clang-tidy ([.clang-tidy](.clang-tidy)) with the flags from `compile_commands.json`.
- `-DUCENTER_WERROR=ON` turns compiler warnings (`-Wall -Wextra -Wpedantic -Wshadow`, MSVC `/W4`) into errors.

### Translations

Source strings in the code are English; translations are Qt Linguist files in [translations/](translations): `ucenter_ru.ts`, `ucenter_es.ts`, `ucenter_zh_CN.ts`. After changing user-visible strings, update the files and translate the new entries (Qt Linguist or a text editor):

```bash
cmake --build build --target update_translations
```

The test `translations_complete` fails while any string is untranslated. The `.qm` files are compiled during the build and embedded in the app, together with Qt's translations of standard dialogs.

To add a language: add its code to `UCENTER_LANGUAGES` in [CMakeLists.txt](CMakeLists.txt) and to `language::available()` in [src/core/language.cpp](src/core/language.cpp), run `update_translations` and translate the new `.ts` file.

### Icon

The app icon is drawn in [resources/icons/ucenter.svg](resources/icons/ucenter.svg); `tools/render_icons/render.sh` renders `ucenter.png`, `ucenter.ico` and (on macOS) `ucenter.icns` from it.

### Continuous integration

[.github/workflows/ci.yml](.github/workflows/ci.yml): on macOS and Windows — build with warnings as errors (MSVC without), tests and a package (DMG / ZIP) as an artifact; on Linux — clang-format, tests under ASan and UBSan, clang-tidy and two minutes of fuzzing.

## Architecture

The code is split into a GUI-independent core and a thin UI layer. Dependencies point one way only: `ui` includes `core`, never the reverse. The core is a static library (`ucenter_core`) linked by the app, the tests and the fuzzer. The UI never talks to the serial port directly: all receiver I/O goes through `ReceiverSession`, and `MainWindow` only assembles the components and connects their signals. `AzimuthTab` does the same for its own widgets.

```
CMakeLists.txt               Build: ucenter_core, UCenter (app bundle), tests, fuzzer, install/CPack
cmake/                       Info.plist template, version from git
resources/                   App icon (SVG source, png, ico, icns) and the Windows resource file
translations/                ucenter_<language>.ts — translations (ru, es, zh_CN)
tests/                       test_core (core logic), test_session (port via a pseudo-terminal)
fuzz/                        libFuzzer target for the frame reader and its seed corpus
tools/                       format.sh, clang_tidy.sh, render_icons/
src/
  main.cpp                   Entry point: language, icon, theme; reopens the window when the language changes
  core/                      No widgets; QtCore (thread, signals, timer), QtSerialPort and QtPositioning only
    ports                    Serial port discovery
    message_types            Decoded message contents: GGA/GSA/RMC/GSV, NAV-PVT/SAT/DOP, ACK, CFG-VALGET
    messages                 GnssMessage: one NMEA or UBX frame, decoded once on arrival; console text
    nmea                     NMEA: checksum, sentence split, decoding
    ubx                      UBX: frames, checksum, message names, decoding, VALSET/VALGET/CFG-CFG building
    frame_reader             Byte stream -> NMEA and UBX frames (skips garbage and bad checksums)
    source                   ReceiverSource: interface of a message source living in the reader thread
    worker                   GnssWorker: QSerialPort source, non-blocking writes, raw stream recording
    replay                   ReplayWorker: recording source paced by epoch time; replayEpochs()
    session                  ReceiverSession: source lifecycle, configuration commands and their ACKs
    receiver_config          CFG keys, rate and output options, command building, ACK and VALGET decoding
    navigation               NavigationState -> NavData (typed snapshot) from NAV-PVT / NAV-DOP / GGA / GSA / RMC
    satellites               Satellite, SatelliteTracker (NAV-SAT, or GSV/GSA), per-constellation counts
    timeutil                 hh:mm:ss / uptime formatting, UTC offsets, monotonic clock
    format                   Number formatting for display (locale-independent)
    geo                      Web Mercator projection, WGS84 inverse problem (Vincenty)
    baseline                 Baseline A->B: azimuth, deviation from true north, offsets, formatting
    baseline_store           Saved measurements: JSON between runs, CSV export
    points                   MeasurementPoints: points A/B and the rules for placing them
    places                   Place search results: Nominatim parsing, merging segments, sorting by distance
    host_location            HostLocation: this computer's location (QtPositioning + location permission)
    language                 Supported languages, the saved choice (English by default), installing translators
  ui/
    main_window              MainWindow: composition and signal wiring, window state, language list
    connection_panel         ConnectionPanel: port, record, replay (port list provider is injected)
    console_tab              ConsoleTab
    dashboard_tab            DashboardTab: formats NavData for display
    compass_view             CompassView
    satellites_tab           Satellites tab with filters
    satellite_views          SkyView, SignalView (custom painting)
    filters                  Constellation filter checkboxes with counters
    config_tab               ConfigTab
    azimuth_tab              AzimuthTab: composition and signal wiring of the Azimuth widgets
    measurement_panels       PointsEditor, ResultPanel, ResultOverlay
    saved_measurements       SavedMeasurementsPanel: table, persistence, delete, CSV export
    place_search_bar         PlaceSearchBar: search field, results menu, → A / → B (map bounds injected)
    imagery_picker           ImageryPicker: imagery provider choice, remembered in QSettings
    location_popover         LocationPopover: why the computer's location is unavailable
    map_view                 MapView: tiles, pan/zoom, click signal, fit to bounds
    baseline_map_view        BaselineMapView: A/B markers, A->B line, found place with a flag, location alert
    map_buttons              Round map buttons with theme-colored icons (crosshair, expand)
    tile_loader              TileLoader and tile sources (OSM, Esri, Google): caches, retry backoff, cancelling
    geocoder                 PlaceSearch: Nominatim requests (Russian, 1 request/s)
    network                  Shared User-Agent and request setup for map services
    painting                 Shared drawing helpers
    theme                    Satellite colors
    style                    App look: light/dark palettes following the OS, system accent color, QSS
    assets/                  SVG icons used by the stylesheet (combo arrow, checkmark), compiled into resources
```

Each module is a `.h`/`.cpp` pair (`format` and `message_types` are header-only).

### Data flow

```
serial port -> GnssWorker ─┐  (reader QThread: QSerialPort + FrameReader, decodes every frame once)
.ubx file  -> ReplayWorker ┴─messages(batch)─> ReceiverSession --message(GnssMessage)--> MainWindow
                                                 |                                         |-> ConsoleTab::appendMessage (buffer)
                                                 |                                         |-> NavigationState
                                                 |                                         `-> SatelliteTracker
                                                 |--configValues--> ConfigTab
                                                 |--status / error--> status bar
                                                 |--recordingChanged--> ConnectionPanel
                                                 `--connectedChanged--> ConnectionPanel, ConfigTab
100 ms timer: NavigationState::data() -> DashboardTab, ConsoleTab::flush()
1 s timer:    SatelliteTracker::snapshot() -> SatellitesTab

ConfigTab --valsetRequested--> ReceiverSession::applyConfig() -> CFG-VALSET, waits for ACK/NAK
ConfigTab --factoryResetRequested--> ReceiverSession::factoryReset() -> CFG-CFG, waits for ACK/NAK
ConfigTab --refreshRequested--> ReceiverSession::pollConfig() -> CFG-VALGET
ConnectionPanel --recordRequested / stopRecordRequested--> ReceiverSession -> GnssWorker (file in the reader thread)
ConnectionPanel --replayRequested--> ReceiverSession::openReplay()

Azimuth tab (AzimuthTab wires the widgets; MeasurementPoints holds the state):
HostLocation --position--> MeasurementPoints::deviceMoved() -> PointsEditor, BaselineMapView
             --status----> BaselineMapView::setLocationAlert() (crosshair dot, pop-up)
BaselineMapView --clicked / pointMoved / recentered--> MeasurementPoints -> PointsEditor, map, ResultPanel
PointsEditor --pointEdited--> MeasurementPoints
PlaceSearchBar --placeChanged--> BaselineMapView::setPlace()   --pointRequested--> MeasurementPoints
ResultPanel / ResultOverlay --saveRequested--> SavedMeasurementsPanel::add() -> measurements.json
SavedMeasurementsPanel --showRequested--> MeasurementPoints::load() -> map.fitBounds()
AzimuthTab --expandChanged--> MainWindow (hides the connection panel)
```

### Coupling and cohesion

- **One job per module.** Each module has a single reason to change: geodesy (`geo`), the measurement (`baseline`), its storage (`baseline_store`), placement rules (`points`), search results (`places`); in the UI each panel owns its widgets and behavior (`saved_measurements` also owns persistence and export).
- **Rules live in `core`.** How points A/B are placed, when A follows the computer's location and what Save/Clear do is in `MeasurementPoints`, without Qt objects, and is unit-tested. The UI only displays the state.
- **Widgets talk through signals.** Panels do not know each other; tabs (`AzimuthTab`, `MainWindow`) connect them. Widgets offer methods instead of exposing internals — for example `BaselineMapView::setExpanded()`, `stopFollowing()`, `showLocationHelp()`.
- **Dependencies are injected.** `ConnectionPanel` gets the port list function and `PlaceSearchBar` gets the visible-map-bounds function, so neither depends on the component that provides them.
- **Sources are interchangeable.** `ReceiverSession` works with any `ReceiverSource`; the port and a recording differ only in the class created in `open()` / `openReplay()`.
- **Shared code has one place.** Network request setup is in `network`; azimuth and deviation formatting is in `baseline`, number formatting in `format`.
- `AzimuthTab` and `MainWindow` include many modules on purpose: they are composition roots, which keeps every other module narrow.

### Design notes

- Reading happens in a separate `QThread` that owns the source (`QSerialPort` or a recording), so the UI never blocks. The source emits all frames read at once as one batch, so the queue between threads holds one event per read instead of one per message. Port errors (busy port, device unplugged) are reported through the `error` signal.
- Every frame is decoded once, in the reader thread, into `GnssMessage::data` (`std::variant`); the console, navigation, satellites and the session only read the result.
- Writes to the receiver are queued to the reader thread without waiting, so a stuck USB device cannot freeze the UI; a write the receiver does not take within 2 seconds is reported and dropped. On disconnect the reader thread gets 3 seconds to stop; a thread stuck in the port driver is detached instead of being destroyed while running.
- The frame reader accepts NMEA sentences only with a valid `*hh` checksum and UBX frames only with a valid Fletcher checksum; anything else is skipped byte by byte until the next frame starts.
- The UI uses the Fusion style on every platform plus a stylesheet from `src/ui/style.cpp`, so it looks the same on macOS and Windows. The light or dark palette follows the system setting and switches live; the accent color is taken from the OS. Widgets do not set inline styles: they use object names (`console`, `statusLed`, `mapButton`) and the dynamic properties `role` and `variant`, which the stylesheet targets.
- Incoming messages only update the models; the Dashboard and the console are redrawn by a 100 ms timer. The console keeps the last 5000 messages and turns them into text only when its tab is visible, in one batch.
- Settings kept between runs (QSettings): interface language, window size and tab, port and baud rate, UTC offset, console filters, imagery provider, the Azimuth map view and splitter, the recording folder.
- `UBX-NAV-PVT`, when present, is the primary source for position, time, fix type, PDOP and satellite count; NMEA GGA/GSA/RMC are used only when NAV-PVT has not been received for 2 seconds. HDOP comes from GGA or `UBX-NAV-DOP`.
- `UBX-NAV-SAT`, when present, is the source for satellites: each message is the full list of the epoch with an explicit "used" flag. Without it satellites come from NMEA: "used in navigation" from GSA, SBAS recognized by PRN ranges 33–64 and 120–158 under the GP talker. Satellites that stop appearing for 5 seconds are removed. If a satellite is reported on several signals (NMEA 4.10+ `signalID`), the best C/N0 of the current epoch is shown; a signal with an empty C/N0 counts as not tracked.
- Map tiles are requested from the center of the view outwards; requests for tiles that left the view are cancelled. While a tile loads, the nearest cached lower-zoom tile is drawn stretched in its place. Tiles are cached in memory and on disk (OS cache directory, up to 200 MB). A tile that fails to load is retried after 10 s, then with a doubling delay up to 5 minutes, to respect the OpenStreetMap tile usage policy.
- Esri imagery is blocked on some networks (HTTP 403); pick another provider on the Azimuth tab then. Google satellite tiles are fetched directly, which Google Maps Platform terms do not allow — use it for personal/internal work only.
- Place search follows the Nominatim usage policy: a request only on Enter (no autocomplete), at most one per second, with a descriptive User-Agent.

## Limitations

- GLONASS and QZSS satellites are numbered as the source reports them: NMEA uses 65–96 and 193–202, `UBX-NAV-SAT` uses slot/PRN numbers from 1.
- The _Marker size_ slider scales satellite markers on the sky plot only.
- The computer's location on a Mac without GPS comes from Wi-Fi and is typically accurate to tens of meters, which limits short baselines measured from the computer's position.
- `test_session` uses pseudo-terminals and is not built on Windows.
