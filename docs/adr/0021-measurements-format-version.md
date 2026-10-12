# 0021. measurements.json carries a format version; unreadable files are set aside, not overwritten

- **Status:** Accepted
- **Date:** 2026-10-12
- **Amends:** the file format part of [0012](0012-storage-compatibility.md)

## Context

`measurements.json` has been a bare JSON list of measurements with the field names of the Python version ([0012](0012-storage-compatibility.md)). Nothing in the file says which format it is, so a future change could not tell old files from new ones, and an older UCenter opening a newer file would misread it. 0012 foresaw a `version` field and a migration for the first format change.

There was also a data-loss path: when the file could not be read (corrupt, or from a newer version), the app warned, started with an empty list, and the next saved measurement overwrote the file.

## Decision

- **Format 2** wraps the list: `{"format": "ucenter-measurements", "version": 2, "measurements": [...]}`; the measurement fields are unchanged.
- **Reading**: a bare list is format 1 and is read as before; an object is read if its `version` is at most the supported one. A file from a newer version is refused with a message that says so.
- **Writing** always uses the current format, so a format 1 file becomes format 2 on the next save — the migration is the save itself.
- **Unreadable files are set aside**: the file is renamed to `measurements.json.unreadable-<date-time>` next to it, the user is told where it went, and new measurements go to a fresh file.
- Future changes raise `baseline_store::FormatVersion`, keep reading every older version, and add a test with a file of each version.

## Alternatives considered

- **A `version` field in every measurement** — keeps the file a list, but a reader still has to look inside to know what it got, and file-level metadata has no place to go.
- **Keep writing format 1 until a field actually changes** — leaves today's files indistinguishable from tomorrow's; the wrapper costs nothing now.
- **Refuse to save after a failed read** — no data loss, but the app becomes unusable for measurements until the user fixes a file they may not know how to fix.

## Consequences

- The Python version cannot read files saved by UCenter 1.2 or later (it expects a list). Going back to it means using the `.unreadable-…` copy, a CSV export, or editing the file to its `measurements` list.
- A corrupt or newer file is never lost: it stays next to the new one under a dated name.
- `storageFormatVersions` in `test_core` covers reading format 1, writing format 2, refusing a newer format and setting a file aside.
