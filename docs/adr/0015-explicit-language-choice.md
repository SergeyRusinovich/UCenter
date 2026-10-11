# 0015. The language is chosen in a drop-down list; the system language is not used

- **Status:** Accepted
- **Date:** 2026-10-11
- **Supersedes:** the language choice part of [0010](0010-localization.md)

## Context

[0010](0010-localization.md) picked the interface language from the system's preferred languages and offered an override in a _Language_ menu with a "System language" item. In practice this is unpredictable: the same computer is used by people who prefer different languages, the system language of a lab or field laptop is often not the operator's, and a menu in the menu bar (on macOS — outside the window) is easy to miss.

## Decision

- The language is chosen only by the user, in a drop-down list in the top-right corner of the tab bar. The list shows each language in itself: English, Русский, Español, 中文（简体）.
- The system language is not used. Until the user chooses, the interface is in English (`language::Default`); the choice is saved in QSettings (`ui/language`) and used at every start.
- Switching still reopens the window in the new language and asks first when a receiver is connected (as in 0010).

## Alternatives considered

- **Keep following the system language with an override** — the behavior this record replaces.
- **Ask for the language on first start** — one more dialog before the user sees anything; the always-visible list makes the choice just as easy.
- **The list in the connection panel or the status bar** — the connection panel is already full, and a control in the status bar is easy to overlook.

## Consequences

- The interface language is predictable and the same on every computer for the same settings.
- First-time users who do not read English have to find the list; the native names make it recognizable.
- `language::codeForUiLanguages()` and the "System language" item are removed.
