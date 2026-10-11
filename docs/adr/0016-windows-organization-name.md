# 0016. On Windows the app sets an organization name for QSettings

- **Status:** Accepted
- **Date:** 2026-10-11
- **Amends:** the "no organization name" rule of [0012](0012-storage-compatibility.md), on Windows only

## Context

[0012](0012-storage-compatibility.md) keeps the organization name empty, so settings and data stay where the Python version left them. On Windows that breaks `QSettings`: without an organization, a default-constructed `QSettings` reads and writes nothing (its status is `AccessError`). Every setting was lost on Windows — the language, the port and baud rate, the time zone, the imagery source, the window geometry. On macOS Qt falls back to a generated domain and everything works, which is why the problem went unnoticed until `test_core` ran on Windows in CI.

## Decision

- On Windows `main()` sets the organization name `UCenter`. Settings live in the registry under `HKCU\Software\UCenter\UCenter`; `QStandardPaths` puts data in `%APPDATA%\UCenter\UCenter` and the tile cache in `%LOCALAPPDATA%\UCenter\UCenter\cache`.
- On macOS nothing changes: no organization name, the paths from 0012 stay.
- The tests set their own organization (`UCenter-tests`), so they never touch the settings of the installed app.

## Alternatives considered

- **An organization name on every platform** — one rule instead of two, but it moves the macOS data and settings, and existing users would need a migration.
- **Pass the organization and application explicitly at every `QSettings` call site** — the same result with a dozen edits, and a new call site that forgets it fails silently on Windows only.
- **An INI file in a fixed location instead of the registry** — not the Windows convention, and a second storage code path to maintain.

## Consequences

- Settings are saved on Windows.
- The Windows data directory moved: `measurements.json` written by an earlier Windows build (the Python version, or a C++ build before 1.0.0) is in `%APPDATA%\UCenter`, and the app no longer finds it there. The app moves it once on startup when the new directory has no such file, and never overwrites an existing one; if the move fails, it keeps using the old file (roadmap step 0.1).
- The rule "no organization name" in 0012 now holds for macOS only; code that builds storage paths must not assume the paths are the same on both platforms.
