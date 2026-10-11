# 0001. Record architecture decisions

- **Status:** Accepted
- **Date:** 2026-10-11

## Context

UCenter was rewritten from Python to C++ and then extended (recording, replay, UBX-only output, localization, packaging). Many choices — an own protocol parser, a hand-drawn map, the way the language is switched — look arbitrary without the reasons behind them, and the reasons are not visible in the code.

## Decision

Architecturally significant decisions are recorded as short Markdown files in `docs/adr/`, one decision per file, numbered in order, in the format of [template.md](template.md). Records stay in the repository next to the code they explain and change in the same commits.

## Alternatives considered

- **Only the README** — it describes how the code is organized now, not why, and grows unreadable if every reason is added to it.
- **A wiki or an external document** — drifts away from the code; reviewers do not see it in pull requests.

## Consequences

- A pull request that changes an architectural decision adds or supersedes a record.
- Records are not rewritten: a changed decision gets a new record, the old one is marked `Superseded by NNNN`.
