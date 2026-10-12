# 0019. macOS packages are signed with a Developer ID and notarized; ad hoc signing stays the default

- **Status:** Accepted
- **Date:** 2026-10-12
- **Amends:** the signing part of [0011](0011-macos-bundle.md)

## Context

[0011](0011-macos-bundle.md) signs the bundle ad hoc: enough for macOS to grant and remember location access, but Gatekeeper blocks an ad hoc signed app downloaded on another Mac, and every build has a different signature, so the system can forget the location permission after an update. The maintainer has an Apple Developer ID Application certificate (team `D966UR5B6V`). A self-signed certificate was considered first; it would give a stable signature on the maintainer's Mac, but Gatekeeper rejects it elsewhere and it cannot be notarized.

## Decision

- CMake option `UCENTER_CODESIGN_IDENTITY`: `-` (default) signs ad hoc as before; a `Developer ID Application: …` identity signs with that certificate.
- With a Developer ID, the build tree bundle is signed with the hardened runtime after linking (no timestamp: it is only for the developer). Installation (`cmake --install`, CPack) signs everything inside the bundle, then the bundle itself, with the hardened runtime, an Apple timestamp and [cmake/UCenter.entitlements](../../cmake/UCenter.entitlements), and verifies the result. CPack signs the `.dmg` as well.
- The only entitlement is `com.apple.security.personal-information.location`: under the hardened runtime macOS gives location only to apps that declare it.
- Apple's timestamp server rejects a request now and then, so each signing command with a timestamp is tried up to three times.
- Notarization: [tools/notarize.sh](../../tools/notarize.sh) submits the `.dmg`, waits, staples the ticket and checks Gatekeeper; credentials come from a `notarytool` keychain profile. In CI, a `v*` tag notarizes when the repository has the secrets `MACOS_CERT_P12`, `MACOS_CERT_PASSWORD`, `NOTARY_APPLE_ID`, `NOTARY_PASSWORD` and `NOTARY_TEAM_ID`; without them CI keeps signing ad hoc.

## Alternatives considered

- **A self-signed certificate** — free and stable on one Mac, but other Macs still warn and notarization is impossible.
- **Signing everything with `--deep` in one step** — simpler, but it would put the entitlements on every framework and plugin; Apple advises against it. The nested code is signed first and the app is re-signed with entitlements on top.
- **Making Developer ID the default** — every developer would need the maintainer's certificate to build.

## Consequences

- A notarized `.dmg` opens on any Mac without a Gatekeeper warning; the location permission survives updates because the signature identity no longer changes.
- Before notarization a Developer ID build is "rejected: Unnotarized Developer ID" by Gatekeeper — expected.
- The hardened runtime loads only code signed by the same team: Qt frameworks and plugins are re-signed during installation, so a plugin added to the bundle later must be signed too.
- The certificate expires on 2027-02-01. Signatures made before that keep working (they carry a timestamp), but new releases need a renewed certificate, and the CI secret must then be replaced.
