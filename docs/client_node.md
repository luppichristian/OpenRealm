# Client node

Target mapping:
- build target id: `openrealm_client`
- compiled binary name: `openrealm-client`

## What it does

The client node is the playable OpenRealm application.

In the current codebase it:
- opens the main game window with raylib
- initializes audio, rendering, input, and background task workers
- shows the client menus for Play, Options, Credits, Pause, and Quit
- lets the player choose a realm, jump node, and explicit world join target from the GUI
- starts a local `World`
- attempts to start a runtime session with role `Client`
- participates in the same local runtime hardening as headless nodes: packet-family validation, suspicious-peer scoring, in-memory quarantine/ban, and topology-hint confirmation before third-party peers become active
- falls back to local-only play if the runtime session cannot start
- renders gameplay, HUD, remote-player state, and menu overlays

This is the binary to use when a human player wants to interact with OpenRealm directly.

## Interface

This is a graphical application, not a CLI tool.
- Opens a resizable window
- Uses keyboard and mouse input
- Shows in-game menus for realm selection and client options
- Saves client-side settings back to `config.json`

Main visible menu surfaces in the current build:
- Main menu: Play, Options, Credits, Quit
- Play menu: Realm, Jump node, Join target X/Y/Z, Play, Back
- Options menu: Master volume, Mouse sensitivity, Invert mouse Y, Show FPS
- Pause menu: Resume, Options, Return to main menu, Quit

Gameplay controls visible in code:
- `Esc` opens the pause menu
- `Tab` toggles the color menu during gameplay

## Command line arguments

The current client entrypoint does not parse command line arguments.
- No `--help`
- No `--config`
- No `--realm-dir`
- Realm and jump-node selection happen through the GUI, not through CLI flags

If you pass extra command line arguments today, the current client entrypoint does not consume them.

## Config fields that matter most

The client reads `config.json` for its local client/runtime settings.
The most important user-facing fields are:
- `masterVolume`
- `mouseSensitivity`
- `invertMouseY`
- `showFps`
- `bindAddress.host`
- `bindAddress.port`
- `enabled`
- `acceptsJoins`
- `areaOfInterest.{radiusX,radiusY,radiusZ}`
- `join.maxCandidates`
- `join.maxHops`
- `limits.maxNodeConnections`
- `limits.maxKnownNodes`
- `periodsMs.neighborRefresh`
- `periodsMs.topologyBroadcast`
- `periodsMs.playerBroadcast`
- `receiveTimeoutMs`

Important behavior details:
- Realm selection is not persisted as a flat runtime launch flag. The player chooses the realm from the GUI.
- Jump-node selection is also a GUI decision.
- The client can continue in local-only mode if runtime startup fails.
- Saving options rewrites `config.json` with the client-facing root settings.

## How to run it

From the repository root:

```bash
bbs build -t openrealm_client
./build/default-windows-x86_64/bin/Debug/openrealm-client.exe
```

You can also use the bbs target runner:

```bash
bbs run -t openrealm_client
```

## Typical user flow

1. Launch `openrealm-client`.
2. Open Play from the main menu.
3. Choose the realm.
4. Choose the jump node used for topology bootstrap.
5. Choose the explicit join target position.
6. Start gameplay.
7. If the runtime resolves a different spawn, the client applies the resolved runtime spawn during play.

## When to use this binary

Use `openrealm-client` when you want:
- the playable graphical experience
- menu-driven realm and jump-node selection
- local rendering, audio, input, and HUD
- to test player-facing UX instead of headless networking

Use the relay/simulator binaries instead when you want background runtime nodes without a window.
