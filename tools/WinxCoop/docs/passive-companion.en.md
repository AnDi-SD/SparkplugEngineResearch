# WinxCoop: normal launch and passive companion

[Русский](passive-companion.md)

The server launches the installed Winx Club through its normal main menu.
The client waits for the server's ready scene and enters it through the game's
direct-start mechanism. The current build displays a passive remote Bloom
when both games are ready in one location, and automatically follows the
server's subsequent level loads. Both original games keep running in their
own processes. The client retains local movement, while the adapter
selectively suppresses its combat, story, and resource effects.
Full cooperative gameplay and world synchronization are not provided.

## Program package

Use the package for your system: `Win7-x86` for a Windows 7 VM, or
`Windows-x64` for a physical PC running 64-bit Windows. Each contains:

- `WinxCoopLauncher.exe` — the launcher window and lobby;
- `WinxCoopGameHost.exe` — the owner of the launched game process;
- `WinxCoop.NativeAdapter.dll` — the adapter for the original 32-bit game.

GameHost and the adapter remain 32-bit in both packages. Normal operation
does not use XpGameProbe or .NET programs. Game files are not included: a
full installation of the original PC version of Winx Club is required.
A prepared game copy is not needed. When launching from a shared folder,
use a mapped network drive.

## Window and language

| Area | Actions |
| --- | --- |
| General settings at the top | Installed game selection, logs folder, launch without the adapter |
| Server on the left | TCP port, lobby creation and closure, game launch |
| Client on the right | Server address, LAN discovery, discovered lobbies, connection and disconnection |
| Upper-right corner | Program interface language |

One program instance takes one network role at a time. With a server lobby
open, the client connection action in that window is unavailable. While
connected to a server, it cannot create its own lobby. Separate buttons do
not change gameplay capabilities or start a second role in the same process.

The language selector offers **System**, **Русский**, and **English**.
The default follows the Windows display language: Russian for a Russian
display language, and English otherwise. A manual choice is saved locally
and applied to the open window. It does not change the game's language,
controls, saves, or network message formats. Technical Windows messages
and original error details may remain in their source language.

## Launch

1. Open `WinxCoopLauncher.exe` from the appropriate package.
2. Choose **Browse…** in the general settings and select the installed
   `WinxClub.exe`. Its directory appears in the field and is saved locally.
3. In **Server** on the left, choose **Create lobby** and **Start game**.
   Select a new game, save, and level through the game's normal menu. You
   can also launch the game before creating the lobby.
4. In **Client** on the right, use **Find on LAN**, or enter `IPv4:port`
   and choose **Connect**. Do not launch the client game separately.
   It waits until the server loads a location. At the first connection,
   the client game starts directly on the assigned ready level, without
   choosing a new game or save in its own menu. The current entry is `entry0`.
5. Pose exchange and remote model creation begin only after both games are
   actually ready. A network connection or launch request alone does not
   mean that the client has loaded the assigned scene.
6. Perform subsequent loads through the normal server game. The client
   waits for its ready scene, then loads the assigned location in its
   already open game. It does not need menu navigation or reconnection.

The current debugging setup runs both games on one physical PC: server
from `Win7-x86`, client from `Windows-x64`, with client address
`127.0.0.1:27025`. This keeps the two programs' log folders separate.
Both games use local controls and their own cameras. Operation across
computers or in a VM needs a separate network check; localhost does not
establish that result.

If two local games use one installation, they share `Media\Saved`. Keep a
copy before such a run. Do not change settings or save to the same slot
simultaneously. The server's full profile and save are not transferred to
the client on direct start.

**Without adapter** launches the original game separately through its normal
path. This allows starting the game when the adapter is incompatible, but
does not verify mod integration or connection of a second model.

## Companion behavior

The original game controls the server's main Bloom. Remote Bloom is a visual
model without a gameplay controller, combat components, or collision shapes.
Her position and animation do not create a second participant in the server's
simulation. The network exchanges character poses; attack, item collection,
and profile-change commands are not sent through this channel.

On the client, normal input, level physics, movement, jumping, crouching,
and camera controls remain available. The adapter suppresses combat, shield,
target lock, and gameplay interaction commands, while retaining movement
and pause commands. Further restrictions cover known damage, pickup, save,
script-update, and scene-change paths. The original game continues updating
the local character and its animations.

The client scene does not reproduce the server simulation: enemies, items,
HP, inventory, and quests are not synchronized. Their state and behavior can
differ between the windows. Guest restrictions are not installed in the
server game; the main server Bloom owns progression.

The client can open pause and the diary, return to the level, open options,
and exit to the main menu. Other diary pages are currently blocked: their
inventory, magic, and outfit actions need separate restrictions. Exiting
through the main menu and Quit retains the game's normal shutdown order.
The remote model is removed before the scene is freed during level unload.

## Reconnection and waiting

**Close lobby** in the server section, or **Disconnect** in the client
section, disconnects the network session and removes the remote models.
Both original games keep running. To return to the same location, choose
**Connect** on the client again. If the server also closed its lobby,
create it again first. Reconnection retains the already open client game,
the local Bloom's position, and its camera. After a broken connection,
the closed client channel is released automatically; reconnection requires
a new user action.

Before creating new models, the adapter waits for the old ones to be removed
and for both scenes to be actually ready. Disconnection also works during
pause: the model is removed and the game remains paused. Reconnection during
pause waits for both games to return to gameplay, then creates the models again.

## Following the server

When the server scene is actually unloaded, only its presentation generation
ends. Remote models are removed, old pose exchange stops, and the TCP
connection remains open. The server game continues its normal operation.
While the server is in a menu or loading a location, the client stays in
its previous scene and waits for an assigned ready level.

After the server finishes loading, the client requests a normal level
change in the same game process. Old models must be removed, and commands
must be completed or confirmed cancelled at scene retirement. If the client
is paused, the request waits for its return to gameplay. A new exchange
generation and new models begin after the old scene is actually unloaded
and the new client scene is ready. Reloading the same server location also
requires a new generation; matching level numbers cannot reuse old poses.

Following transfers a level number and uses its default `entry0` entrance.
The server Bloom's exact spawn point, profile, save, inventory, and script
state are not transferred. The client's spawn point and world can therefore
differ from the server's. Exiting the client to its main menu or Quit remains
a normal session exit; following does not reconnect after a broken connection.

## Presentation smoothing

Remote Bloom's position and rotation are smoothed between received poses.
Repeating the same pose does not restart movement towards it or postpone
interpolation completion. The interpolation interval adapts within limits
to the local pose arrival rate; the computers' clocks are not a shared game timer.

After a long gap or a large movement, a new pose can be applied immediately,
without dragging the model through the level. Smoothing does not predict
the next movement or modify the locally controlled Bloom, her normal
physics, or the camera.

## Settings, compatibility, and stopping

Normal launch does not copy the game or change its INI files or registry
settings. On the client, the adapter sets `startLevel` only in process
memory after settings are read; the normal game code performs the subsequent
entry. Server launch does not change that value. The default duration is
zero: there is no general game shutdown timer. Explicitly selected
diagnostic modes can have their own limits.

The game EXE's full SHA256 is not a compatibility requirement. The adapter
checks required functions, hook points, and image structure; an unsupported
mechanism produces a specific error in the log. The current native path's
image-format and ASCII-path restrictions remain. **Without adapter** uses
a separate launch path.

The current passive path uses gameplay network protocol V4 and adapter
contract IPC5. Both computers must use matching packages from the same
build; keep the launcher, GameHost, and adapter DLL together. The previous
V3 protocol is incompatible with scene-generation exchange. V3 references
in historical P/E guides do not describe the current package.

The adapter checks that the selected resources match the original game's
actual resource root. With multiple installations, the system `MediaPath`
can point to another folder even when the correct EXE is selected. This
mismatch is not solved by weakening the check: the installed game and its
resources must first be selected correctly. The launcher does not rewrite
game registration itself.

Exit through the game's normal menu. Closing the launcher requests
termination of its owned processes and waits for cleanup. An adapter error
or loss of the owner can also trigger a protective game shutdown.

## Logs

**Open logs** opens the program's local directory:

- `%LOCALAPPDATA%\WinxCoop\Win7-x86` — the 32-bit launcher;
- `%LOCALAPPDATA%\WinxCoop\Windows-x64` — the 64-bit launcher.

It contains `launcher.log`, the selected game directory in
`launcher-settings.ini`, and a `Sessions` directory with per-run logs,
including `host-report.json` and `host-report.json.events.jsonl`. Normal
launch does not require writing to the shared program folder. To investigate
an error, keep the launcher log and the relevant run folder from `Sessions`.
The language preference is saved in the same `launcher-settings.ini`:
section `[Interface]`, key `language`, with value `system`, `ru`, or `en`.

## Verified scope and limitations

The local character retains normal local input, physics, and camera controls.
The remote model receives position, direction, and idle, walk, or jump
animation. A separate running animation is not transmitted yet; remote
Bloom's body and hair use the fixed original jeans outfit. A separate
remote crouching animation is not transmitted either.

Two actual games on localhost have verified the first Gardenia location,
automatic client entry, two visible Blooms, local movement, jumping, camera,
client command suppression, pause and resume, options, main-menu exit, and
normal Quit with model cleanup. Additional checks covered reconnection
without restarting the games, model removal during pause, waiting for both
games to return before recreating models, control after connection, and
cleanup of a finished scene generation.

Automatic following has been checked by reloading the first Gardenia on the
server through the normal menu and save: both games kept their processes
and connection, the client actually unloaded and reloaded the scene, the
models were recreated, and client movement and jumping worked. The normal
server transition from Gardenia 1 to Gardenia 2 has also been checked: the
client followed automatically in the same process, and both models appeared
again. Client movement, jumping, diary, pause, and resume were checked after
that transition. The client uses the default `entry0`; an exact match with
the server's spawn point is not established.

These checks cover only the listed locations and transition. Having
restriction points in the adapter does not mean that all variants of
combat, damage, pickups, saves, and scripts have been checked in the game.
Shared resources and the game world are not synchronized. A new scene
assignment and actual level readiness are confirmed separately from delivery
of a transition request; a waiting message alone does not prove a completed load.

A successful build and protocol tests do not replace two-game validation.
The main debugging setup uses localhost; moving to a VM and a physical LAN
needs its own network check. Historical P and E results describe separate
paths and do not define this package.
