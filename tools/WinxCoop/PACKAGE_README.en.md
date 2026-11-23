# WinxCoop — passive companion

[Русский](README.md)

A second player controls their own Bloom alongside the server's Bloom. The
server plays the normal single-player game; the client follows its level
loads. Combat, items, HP, quests, and saved progress are not yet shared.

## Launch

1. Keep the entire package folder together. `WinxCoopGameHost.exe` and
   `WinxCoop.NativeAdapter.dll` must stay next to `WinxCoopLauncher.exe`.
   Use `Win7-x86` for Windows 7 or `Windows-x64` for Windows x64. GameHost
   and the adapter remain 32-bit in both packages.
2. Open `WinxCoopLauncher.exe`. In the common settings at the top, choose
   **Browse…** and select `WinxClub.exe` from a full installation of the
   original game. Game files are not included; a prepared game copy is not needed.
3. Before launch, check `fullScreen=false` in the game's `winx.ini` if you
   want windowed mode. The launcher does not rewrite game settings.
4. On the server, use the left side of the window: **Create lobby** →
   **Start game** → the game's normal menu and new game or save selection.
5. On the client, use the right side: **Find on LAN**, or enter
   `IPv4:port` → **Connect**. Do not launch the client game separately:
   it loads automatically once the server's location is ready.

The default port is `27025`. For two games on one PC, open the two separate
packages and connect to `127.0.0.1:27025`. If both games use one installation,
they share its save directory; do not save to the same slot simultaneously.
Use a mapped network drive when launching from a shared network folder.

## Window and language

Common settings and the logs button are at the top; server functions are on
the left and client functions on the right. One program instance takes one
network role at a time. Actions unavailable in the current state are disabled.

The language selector is in the upper-right corner. **System** selects Russian
for a Russian Windows display language, and English otherwise. **Русский** and
**English** select a language manually. Your choice is saved locally; switching
languages does not restart the game or change its language.

## Exit and logs

Exit the game through its normal menu and Quit. To disconnect without closing
the game, choose **Close lobby** on the server or **Disconnect** on the client.
Closing the launcher waits for its owned processes to finish. There is no
general automatic game shutdown timer.

**Open logs** opens the local `launcher.log` and `Sessions` directory:

- `%LOCALAPPDATA%\WinxCoop\Win7-x86` — the x86 package;
- `%LOCALAPPDATA%\WinxCoop\Windows-x64` — the x64 package.

If an error occurs, keep `launcher.log` and the relevant run folder from
`Sessions`. The program does not need to write logs to the shared package folder.

## Limitations

The client uses the location's default entry. The server's exact spawn point,
profile, and world state are not transferred. Remote Bloom uses a fixed outfit
and basic animations. Gameplay checks are limited to two games on localhost,
Gardenia 1/2, and the documented scenarios. This version has not been verified
across all levels or on a physical LAN.

[Full guide and limitations](docs/passive-companion.en.md)
