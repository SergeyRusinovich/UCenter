# 0010. Localization with Qt Linguist; language switch recreates the window

- **Status:** Accepted; the choice of language is superseded by [0015](0015-explicit-language-choice.md)
- **Date:** 2026-10-11

## Context

The Python version mixed Russian and English in one interface. Users of u-blox receivers are international; the app should support English, Russian, Spanish and Chinese and let the user pick a language other than the system one.

## Decision

- **Source strings are English**, wrapped in `tr()` (or `QT_TRANSLATE_NOOP` in static tables and translated at display time).
- **Translations** are Qt Linguist files `translations/ucenter_<code>.ts` for `ru`, `es`, `zh_CN` (Simplified Chinese). `qt_add_translations` compiles them to `.qm` and embeds them in the app, together with Qt's own `qtbase_<code>.qm` for standard dialogs, so nothing depends on the Qt installation.
- **Choosing the language** (`core/language`): the user's choice from the _Language_ menu, saved in QSettings; otherwise the first supported language from the system's preferred languages; otherwise English. Every Chinese variant maps to Simplified Chinese.
- **Switching at runtime** opens a new main window in the new language and closes the old one (after saving its state); widgets set their texts once at construction and have no re-translation code. If a receiver is connected the user is asked first, because the connection is closed.
- **Completeness is a test**: `translations_complete` fails if any `.ts` file has an untranslated string.

## Alternatives considered

- **Re-translating widgets on `QEvent::LanguageChange`** — every widget would need a `retranslateUi()` that repeats its text setup; a lot of code to keep in sync for a rarely used action.
- **Applying the language after a restart** — simpler, but restarting a bundle reliably differs per platform and loses the session anyway.
- **Separate Simplified and Traditional Chinese** — doubles the translation work; Traditional readers read Simplified far better than English. Can be added later as `zh_TW`.

## Consequences

- Adding a language: add its code to `UCENTER_LANGUAGES` in `CMakeLists.txt` and to `language::available()`, run `update_translations`, translate the new `.ts` file.
- After changing user-visible strings: run `update_translations` and translate the new entries, or the test fails.
- Layouts must survive longer translations: labels that can grow use word wrap or eliding, and the connection panel uses short labels with tooltips.
