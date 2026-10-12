# 0008. Tile map drawn with `QPainter`

- **Status:** Accepted; Google imagery made opt-in by [0022](0022-google-imagery-opt-in.md)
- **Date:** 2026-10-11

## Context

The Dashboard shows the current position, and the Azimuth tab needs a satellite map with draggable markers, a line A→B, place highlighting and a flag. The map has to work with several tile providers (Esri, Google, OpenStreetMap) and respect their usage policies (User-Agent, caching, no hammering on errors).

## Decision

The map is a plain `QWidget` (`MapView`) that projects Web Mercator itself and draws raster tiles with `QPainter`; `BaselineMapView` adds the measurement overlay. `TileLoader` fetches tiles with `QNetworkAccessManager`:

- memory cache (600 tiles) and disk cache (200 MB, OS cache directory);
- tiles are requested from the center outwards; requests for tiles that left the view are cancelled;
- while a tile loads, the nearest cached ancestor is drawn stretched in its place;
- a failed tile is retried after 10 s, then with a doubling delay up to 5 minutes;
- on Retina screens imagery sources are drawn from the next zoom level at full resolution (not OSM, whose labels would shrink).

## Alternatives considered

- **Qt Location (`QtLocation` / Map QML item)** — requires Qt Quick in a Widgets app, its tile plugins do not cover Esri imagery, and custom drag interactions are harder.
- **Qt WebEngine with Leaflet** — a Chromium runtime of ~100 MB for one view, and every interaction has to cross a JavaScript bridge.

## Consequences

- Full control over interaction (drag of markers and the flag, zoom around the cursor, trackpad handling) and over request policy.
- The map is small and dependency-free, but features like vector tiles or rotation would have to be written by hand.
- Google tiles are fetched directly, which their terms do not allow for public use; since [0022](0022-google-imagery-opt-in.md) they are shown only after the user accepts those terms.
