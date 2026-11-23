# WinxCoop 0.1.0

[Русский](RELEASE_NOTES_0.1.0.md) · [Quick start](README.en.md) · [User guide](docs/passive-companion.en.md)

The first public release of WinxCoop, dated November 23, 2026. In the original PC version of Winx Club, a second player can control their own Bloom alongside the host's Bloom. Each player controls their own character and camera, while the host leads the playthrough.

## Downloads

- [Windows x64](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/download/winxcoop-v0.1.0/WinxCoop-0.1.0-Windows-x64.zip)
- [Windows 7 x86](https://github.com/AnDi-SD/SparkplugEngineResearch/releases/download/winxcoop-v0.1.0/WinxCoop-0.1.0-Win7-x86.zip)

Extract the entire archive for your system and open `WinxCoopLauncher.exe`. You need a full installation of Winx Club. Select its `WinxClub.exe` in the launcher's general settings. Both archives include instructions in English and Russian.

## Features

- The server creates a lobby and starts the game through its normal menu. The client finds a lobby on the local network or connects using a manually entered address and port.
- The client enters the location automatically once the server has loaded it. On the next transition, the client follows in its already open game. The games exchange Bloom's position, direction, and basic animations.
- General settings are at the top, server actions on the left, and client actions on the right. The language selector is in the upper-right corner. **System** uses Russian when Windows displays in Russian and English otherwise. A manual choice is saved and applied immediately.
- Each package keeps local logs. Choose **Open logs** to open the folder with `launcher.log` and `Sessions`.

## Release scope

The client's Bloom is a passive companion. Combat, item collection, and story actions belong to the host. Each game keeps its own HP, inventory, quests, and world state. The client loads the location's default entry. Remote Bloom uses a fixed outfit and basic animations.

Developer gameplay checks cover two instances on one PC over localhost, a reload of Gardenia 1, and the transition to Gardenia 2. Checks of the updated launcher cover both architectures, switching and saving the display language, lobby discovery, connecting, and disconnecting on one PC. Other levels, connections between separate computers, and running inside Windows 7 remain outside the verified scope.
