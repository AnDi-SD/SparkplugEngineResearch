# WinxCoop interface development

[Русский](development.md)

The passive companion window is a native Win32 application. General settings
are at the top, server functions on the left, and client functions on the
right. One program takes one network role at a time; separate controls do
not create another session. Language changes do not alter gameplay or the
network contract.

## Interface strings

All interface labels and status messages are collected in the separate
[`LaunchText.h`](../src/Launcher/LaunchText.h) file. Each table row stores
a stable key beside its English and Russian strings. `TextId` selects the
entry, `UiLanguage` selects the language, and `Text` returns the translation.
`UiStatus` stores a key and message detail; `Render` replaces `{0}` with that
detail. Switching languages renders the current status again without
repeating its operation.

Original error details, filenames, addresses, and paths remain data; they
should not be translated as keys. Status messages in the local log are
written in English independently of the window's selected language.

[`LaunchMain.cpp`](../src/Launcher/LaunchMain.cpp) uses
`GetUserDefaultUILanguage` to choose Russian for a Russian Windows display
language and English otherwise. A manual preference is saved in the local
`launcher-settings.ini`, section `[Interface]`, key `language`:

| Value | Behavior |
| --- | --- |
| `system` | Follow the Windows display language |
| `ru` | Russian |
| `en` | English |

A missing or unknown value uses `system`. The selector does not change the
game's locale, numeric network fields, or save formats.

## Updating translations

Add a key and both translations in one table row. Retain `{0}` in both
translations of a message with detail; word order can differ. Do not
translate filenames or program identifiers.

After an edit, check both languages, system selection, persistence of a
manual choice after restart, rerendering of the current status, and long
labels when resizing the window. Update the Russian and English guides
together. A ready-to-run package receives `PACKAGE_README.md` as `README.md`,
`PACKAGE_README.en.md` as `README.en.md`, and both `passive-companion`
guides in its own `docs` directory.

[Home](../README.en.md) · [User guide](passive-companion.en.md) ·
[Shared contribution rules](../../../CONTRIBUTING.md)
