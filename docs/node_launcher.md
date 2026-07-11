# Node launcher

Target mapping:
- build target id: `openrealm_node_launcher`
- compiled binary name: `openrealm-node-launcher`

## What it does

The node launcher is a CLI orchestration tool for local OpenRealm runtime sessions.

In the current codebase it:
- validates the selected realm directory
- loads a base `config.json`
- normalizes the config schema before generating per-node configs
- creates a timestamped session directory under `build/node_launcher/`
- clones the selected realm into a session-local realm directory
- rewrites `jump_nodes.json` to point at the launcher-created local relay ports
- generates one config file per relay, simulator, and client
- launches `openrealm-relay`, `openrealm-simulator`, and `openrealm-client` child processes
- prints launch lines, periodic status, final status, and the session log directory
- stops all launched children on Ctrl+C or when `--run-seconds` expires

This is the most convenient binary for local multi-node testing.

## Interface

This is a CLI/headless utility.
- No graphical window
- Human-readable console output
- Spawns and supervises child node processes
- Writes generated configs and logs to a timestamped session directory

Unlike the relay/simulator binaries, the launcher does have built-in help text.

## Command line arguments

Required:
- `--realm <name-or-path>`
  - Realm name under `realms/` or a direct path to a realm directory

Optional:
- `--relays <count>`
  - Relay nodes to launch
  - Default: `1`
- `--simulators <count>`
  - Simulator nodes to launch
  - Default: `0`
- `--config <path>`
  - Base config file to clone before per-node edits
  - Default: `config.json`
- `--clients <count>`
  - Client nodes to launch
  - Default: `0`
- `--relay-base-port <port>`
  - First relay UDP port
  - Default: `46001`
- `--sim-base-port <port>`
  - First simulator UDP port
  - Default: `46101`
- `--client-base-port <port>`
  - First client UDP port
  - Default: `46201`
- `--relay-ticks <count>`
  - Relay tick budget for generated relay configs
  - `0` means infinite
  - Default: `0`
- `--sim-frames <count>`
  - Simulator frame budget for generated simulator configs
  - `0` means infinite
  - Default: `0`
- `--sim-sleep-ms <ms>`
  - Simulator sleep between frames
  - Default: `16`
- `--sim-frame-time <seconds>`
  - Simulator timestep
  - Default: `0.0166667`
- `--launch-delay-ms <ms>`
  - Delay between child launches
  - Default: `250`
- `--run-seconds <seconds>`
  - Auto-stop after N seconds
  - `0` means keep running until Ctrl+C
- `--emit-place-event`
  - Enables the startup test place event on the first simulator only
- `--help`
  - Prints usage text

Validation rules currently enforced by the launcher:
- `--realm` is required
- at least one of `--relays`, `--simulators`, or `--clients` must be greater than zero
- node counts cannot be negative
- base ports must be between `1` and `65535`
- launch delay and runtime cannot be negative

## How realm resolution works

`--realm` accepts either:
- a direct absolute path
- a direct path relative to the repo root
- a realm name resolved as `realms/<name>`

A valid realm directory must contain:
- `realm.json`
- `jump_nodes.json`

## Generated output

Each launcher run creates a new timestamped session directory under:

```text
build/node_launcher/<timestamp>/
```

Inside it, the launcher writes:
- `relay-<n>.json`
- `simulator-<n>.json`
- `client-<n>.json`
- `relay-<n>.log`
- `simulator-<n>.log`
- `client-<n>.log`
- `realm/realm.json`
- `realm/jump_nodes.json`

The rewritten session-local `jump_nodes.json` points at the relay ports launched for that session.

## How to run it

From the repository root:

```bash
bbs build -t openrealm_node_launcher
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --realm test --relays 1 --simulators 1 --run-seconds 5
```

You can also use the bbs target runner:

```bash
bbs run -t openrealm_node_launcher -a --realm test --relays 1 --simulators 1 --run-seconds 5
```

Show help:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --help
```

## Typical examples

Launch one relay only and keep it running until Ctrl+C:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --realm test --relays 1 --simulators 0
```

Launch one relay and two clients for a short smoke run:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --realm test --relays 1 --clients 2 --run-seconds 5
```

Launch one relay and two simulators for a short smoke run:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --realm test --relays 1 --simulators 2 --run-seconds 5
```

Launch multiple relays on a custom base port range:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --realm test --relays 2 --relay-base-port 47001 --run-seconds 10
```

## What you should expect when it starts

A normal run prints:
- one launch line per child process
- a session summary showing repo root, base config, source realm, session directory, and requested node counts
- periodic status lines while children are alive
- final status lines and the session log directory when shutting down

A real example from verification on this machine:
- the launcher successfully accepted `--realm test --relays 1 --simulators 1 --run-seconds 2`
- it created a session under `build/node_launcher/20260710-002418/`
- it launched both child processes and then stopped after the requested auto-stop interval

## When to use this binary instead of the others

Use `openrealm-node-launcher` when you want:
- a local multi-process test session
- automatically generated per-node configs
- session-local logs and realm copies
- one command to start and stop several runtime nodes together

Pair it with `openrealm-realm-tester` when you want a packet-level runtime verification pass against the launched realm.

Use the relay/simulator binaries directly when you want to manage each node yourself.
