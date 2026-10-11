# 0014. Version from git tags; packages with CPack

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

A build installed on another computer must work without Qt installed, and every build must say which code it was made from — especially builds from uncommitted changes.

## Decision

- **Version**: [cmake/version.cmake](../../cmake/version.cmake) runs `git describe` at configure time. A tag `vX.Y.Z` sets the project version; commits after the tag, the commit hash and `-dirty` form the full version shown in the title bar and in `CFBundleVersion`. Without a tag the base version from the file is used with the hash appended.
- **Packaging**: `install()` puts the app in place, `qt_generate_deploy_app_script` runs `macdeployqt` / `windeployqt` to copy Qt libraries and plugins next to it, and CPack builds a drag-and-drop DMG on macOS and a ZIP on Windows. On macOS the deployed bundle is re-signed ([0011](0011-macos-bundle.md)).

## Alternatives considered

- **A version number edited by hand** — forgotten easily, and does not identify development builds.
- **Static Qt linking** — one file, but requires building Qt statically and complicates LGPL compliance.
- **Platform-specific installers (pkg, MSI) from the start** — more setup for no current need; CPack can produce them later.

## Consequences

- Creating a release means pushing a `vX.Y.Z` tag; CI builds the packages.
- The version is read at configure time: reconfigure the build after creating a tag.
- Packages are about 35 MB because they carry Qt.
