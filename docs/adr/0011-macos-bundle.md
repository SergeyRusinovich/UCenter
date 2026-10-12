# 0011. A signed app bundle on macOS for location access

- **Status:** Accepted; signing with a Developer ID and notarization added by [0019](0019-developer-id-signing.md)
- **Date:** 2026-10-11

## Context

Point A of an azimuth measurement is the computer's own location (OS location services). macOS grants location only to an application bundle whose `Info.plist` contains `NSLocationUsageDescription`, and remembers the user's answer only for a signed bundle. The Python version needed a separate script that wrapped the interpreter in such a bundle.

## Decision

On macOS the executable is always built as `UCenter.app` with an `Info.plist` from [cmake/Info.plist.in](../../cmake/Info.plist.in) (usage descriptions, icon, version, supported localizations). The build signs the bundle ad hoc after linking; installation re-signs it after `macdeployqt` changes it. Location permission is requested through `QLocationPermission` only when the Azimuth tab is first shown.

## Alternatives considered

- **A plain executable** — macOS would deny location silently.
- **A Developer ID signature and notarization** — needed to distribute outside the team without Gatekeeper warnings; requires a paid account and secrets in CI. Can be added on top later.

## Consequences

- Run the app with `open build/UCenter.app` (or from Finder): when started from a terminal or a debugger, macOS may attribute the permission request to that process instead. To debug location features, start the bundle with `open` and attach the debugger to the running process.
- Ad-hoc signed packages show a Gatekeeper warning on other Macs until the app is signed with a Developer ID.
