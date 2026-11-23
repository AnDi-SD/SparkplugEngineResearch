# WinxCoop

[Русский](README.md)

[Release 0.1.0 on GitHub](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/tag/winxcoop-v0.1.0) ·
[Release notes](RELEASE_NOTES_0.1.0.en.md)

A passive companion for the original PC version of Winx Club. The server runs
the normal single-player game, while the client controls a second Bloom in
the same location. The games exchange position, direction, and basic
animations; the client automatically follows the server's level loads.

The companion does not yet take part in combat, item collection, or story
actions. The world, HP, inventory, quests, and saved progress are not shared.

## Quick start

1. Open `WinxCoopLauncher.exe` from the package for your system: `Win7-x86`
   or `Windows-x64`. A full installation of the original Winx Club is required;
   game files are not included.
2. In the common settings at the top, select the installed `WinxClub.exe`.
   For windowed play, set `fullScreen=false` in the game's `winx.ini` beforehand.
3. In the server section on the left, choose **Create lobby** and **Start game**.
   Select a new game or a save through the game's normal menu.
4. On the other computer, use the client section on the right to find the
   server on the LAN, or enter `IPv4:port` and choose **Connect**. The client
   game automatically loads the server's ready location.

For two games on one PC, use the two separate program packages and
`127.0.0.1:27025`. See the [passive companion guide](docs/passive-companion.en.md)
for connection, exit, and limitation details. The ready-to-run folder has its
own [package instructions](PACKAGE_README.en.md).

The language selector is in the upper-right corner. **System** selects Russian
for a Russian Windows display language and English for any other display
language. A manual **Русский** or **English** choice is saved locally and applied
to the open window.

## Components and development

`src/Launcher` contains the launcher window, lobby discovery, and passive
companion coordinator. `src/Adapter` connects character presentation to the
original game. `src/Networking` contains our session and transport, using the
shared reconstructed Packet/MemoryStream codec. This is our integration; it
does not establish a working multiplayer mode in the original engine.

Building requires Windows, Visual Studio C++ Build Tools, the Windows SDK,
and CMake. See [CONTRIBUTING](../../CONTRIBUTING.md) for dependencies and source
rules. Run the contract checks from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/WinxCoop/Build-NetworkChecks.ps1
```

Protocol checks without the game do not replace testing two actual games.
The current gameplay scope is limited to localhost, Gardenia 1/2, and the
scenarios listed in the guide. Other levels, physical LAN operation, and
this version running inside Win7 require separate checks.

[User guide](docs/passive-companion.en.md) ·
[Interface and translations](docs/development.en.md) ·
[Historical P/E prototypes, in Russian](docs/legacy-prototypes.md) ·
[Original network stack boundaries, in Russian](../../docs/engine/architecture/network-session.md)
