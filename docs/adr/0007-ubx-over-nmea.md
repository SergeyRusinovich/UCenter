# 0007. UBX messages take priority over NMEA

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

The M9 can output the same information as NMEA and as UBX. NMEA is on by default; UBX gives validity flags, more precision and explicit fields (for example, whether a satellite is used in the solution), and allows turning NMEA off to free the USB link. Both may be on at the same time, and either may stop at any moment after a configuration change.

## Decision

- Position, time, fix type, PDOP and satellite count come from `UBX-NAV-PVT` while it arrives; NMEA GGA/GSA/RMC are used only when NAV-PVT has not been seen for 2 s.
- HDOP comes from GGA or `UBX-NAV-DOP` (NAV-PVT has no HDOP).
- Satellites come from `UBX-NAV-SAT` while it arrives: each message replaces the whole list with an explicit "used" flag. Without it, satellites come from GSV and the "used" flag from GSA; SBAS is recognized by PRN ranges.
- Satellites not reported for 5 s disappear.

## Alternatives considered

- **NMEA only** — works out of the box but loses validity flags and needs heuristics (SBAS ranges, multi-signal GSV merging).
- **UBX only** — requires configuring every receiver before it shows anything.
- **Merging both continuously** — two sources with different numbering and timing would fight over the same fields.

## Consequences

- The app works with a factory-configured receiver and gets better when UBX output is enabled on the Configuration tab.
- GLONASS and QZSS numbering differs between sources (NMEA 65–96 / 193–202, NAV-SAT from 1); after switching sources the labels change. This is documented as a limitation.
