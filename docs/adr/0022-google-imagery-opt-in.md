# 0022. Google satellite imagery is shown only after the user accepts its terms

- **Status:** Accepted
- **Date:** 2026-10-12
- **Amends:** the Google part of [0008](0008-painted-tile-map.md)

## Context

The Azimuth tab offers three imagery providers. Esri World Imagery is the default, OpenStreetMap is a street map, and Google satellite tiles are fetched directly from Google's tile servers. The Google Maps Platform terms do not allow apps to load those tiles directly and allow the imagery only for personal, non-commercial use, so shipping it as an ordinary choice in a distributed app is not compliant. At the same time Google is a real fallback: Esri is blocked on some networks (HTTP 403), and then Google may be the only satellite imagery that loads.

## Decision

- Esri stays the default; Google moves to the end of the list as "Satellite — Google (personal use)".
- The first time Google is chosen, a dialog explains the terms and asks for confirmation; Google is shown only after "Yes", and the answer is remembered (`azimuth/google_terms_accepted`). "No" returns to the previous provider.
- A Google choice saved by an earlier version counts as accepted: the user picked it deliberately, and silently switching to Esri could leave the map empty where Esri is blocked.

## Alternatives considered

- **Remove Google** — the cleanest legally, but it takes away the only satellite fallback where Esri does not load.
- **Leave it as it was** and only document the restriction — distributing it as an ordinary option still contradicts the terms.
- **An official Google Maps API key** — the Map Tiles API requires billing and session tokens, and its terms would still restrict caching the tiles on disk, which the tile loader does.

## Consequences

- New users see Esri and OpenStreetMap; Google requires an explicit, informed choice.
- The responsibility for using Google imagery is with the user who accepted the terms; the README says so.
- Removing Google later, or replacing it with a licensed source, needs only the imagery list to change.
