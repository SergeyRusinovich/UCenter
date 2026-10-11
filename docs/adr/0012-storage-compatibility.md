# 0012. Settings and data compatible with the Python version

- **Status:** Accepted; the organization name on Windows is amended by [0016](0016-windows-organization-name.md)
- **Date:** 2026-10-11

## Context

Users of the Python version have saved azimuth measurements and settings. The rewrite must not lose them.

## Decision

- The application name stays `UCenter` and no organization name is set, so `QStandardPaths` and `QSettings` resolve to the same locations as before (e.g. `~/Library/Application Support/UCenter/measurements.json` on macOS).
- `measurements.json` keeps the Python field names (`a_lat`, `azimuth`, `saved_at`, …); `null` azimuths stay `null`. It is written atomically with `QSaveFile`.
- The imagery choice keeps its key (`azimuth/map_source`) and values (tile cache directory names).
- New settings get their own keys under a section per component (`connection/…`, `console/…`, `window/…`, `ui/language`).

## Alternatives considered

- **Setting an organization name or a new data format** — cleaner paths and a versioned schema, but requires a migration step for existing users with no real benefit yet.

## Consequences

- Existing measurements appear in the C++ version without any action.
- A future format change should add a `version` field and a migration, recorded in a new ADR.
