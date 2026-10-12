# 0020. Windows packages: an MSI installer built by CPack WiX, next to the ZIP

- **Status:** Accepted
- **Date:** 2026-10-12
- **Amends:** the packaging part of [0014](0014-versioning-and-packaging.md)

## Context

On Windows the app shipped only as a ZIP: the user unpacks it somewhere, finds `bin\UCenter.exe` and makes a shortcut by hand, and there is no clean way to update or remove it. [0014](0014-versioning-and-packaging.md) left platform installers for later; CPack can build them from the same installation.

## Decision

- CPack builds both a ZIP and an MSI (WiX generator) on Windows. The MSI installs for all users (`CPACK_WIX_INSTALL_SCOPE perMachine`): into `Program Files\UCenter`, with `UCenter` in the all-users Start menu; it shows the MIT license and uses the app icon.
- `CPACK_WIX_UPGRADE_GUID` is fixed (`C7595876-A5F1-40DA-BF7E-43674F64D6D8`): it is how Windows recognizes a newer MSI as an upgrade of the installed one, so it never changes.
- MSI accepts only a numeric `X.Y.Z` version; [cmake/cpack_project_config.cmake](../../cmake/cpack_project_config.cmake) gives the WiX generator the tag version, while the file name keeps the full version.
- CI checks every Windows build on a clean runner: silent install, the Start menu shortcut, a run of the installed app (it must start with the deployed Qt), silent uninstall, nothing left in `Program Files`.

## Alternatives considered

- **NSIS** — an `.exe` installer, also supported by CPack, but upgrades and uninstall have to be scripted, and it is not what administrators deploy through policies.
- **MSIX** — the modern format, but it requires a signing certificate trusted by the computer even for testing, and packaged apps run with restrictions (file system virtualization) that would affect settings and data paths ([0016](0016-windows-organization-name.md)).
- **ZIP only** — the status quo described above.

## Consequences

- Windows users get a normal install, upgrade and uninstall; the ZIP stays for portable use.
- The MSI is not code-signed: Windows SmartScreen may warn on download until it is signed with a Windows code signing certificate (a separate decision).
- User data (`%APPDATA%\UCenter`) and settings (registry `HKCU\Software\UCenter`) are not removed on uninstall, as usual for Windows apps.
