# 0002. C++20 and Qt 6 Widgets instead of Python and PyQt6

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

The first version was written in Python with PyQt6, pyserial and pyubx2. It worked, but:

- shipping it meant shipping a Python interpreter and packages; on macOS location access even required a hand-made wrapper bundle around the interpreter (`tools/make_macos_app.sh`);
- runtime type errors showed up only when a code path was reached, which is costly for code that reacts to receiver data;
- the app is a desktop tool for macOS and Windows with custom-painted views (sky plot, signal chart, map) — exactly what Qt Widgets is built for.

## Decision

The app is written in C++20 with Qt 6 Widgets and built with CMake. Qt modules: Core, Gui, Widgets, Network, SerialPort, Positioning, Svg; Test and LinguistTools for development. The Fusion style plus a stylesheet gives the same look on every platform.

## Alternatives considered

- **Keep Python, package with PyInstaller/Briefcase** — keeps the dynamic-typing and startup-size problems; the location-permission bundle remains a workaround.
- **Qt Quick (QML)** — better for touch and animation, but the app is a dense desktop instrument panel with custom painting where Widgets is simpler and more mature.
- **Electron or a web UI** — much larger runtime, and serial ports and OS location need native bridges anyway.

## Consequences

- One native binary per platform; `macdeployqt`/`windeployqt` produce self-contained packages ([0014](0014-versioning-and-packaging.md)).
- The compiler, warnings as errors and clang-tidy catch whole classes of mistakes before running ([0013](0013-quality-gates.md)).
- pyubx2 is no longer available, so the protocols are parsed by the app ([0003](0003-own-protocol-parser.md)).
- Contributors need a C++ toolchain and Qt 6.7+.
