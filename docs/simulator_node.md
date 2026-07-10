# Simulator node

Target mapping:
- build target id: `openrealm_simulator`
- compiled binary name: `openrealm-simulator`

## What it does

The simulator node is a headless world-simulation process for OpenRealm.

In the current codebase it:
- loads the shared flat `config.json`
- loads the selected realm from `realms/<name>/`
- starts a runtime session with role `Simulator`
- initializes a headless `World`
- advances the world in a fixed-step loop
- ticks the runtime session alongside the world simulation
- applies local runtime hardening: packet-family validation, suspicious-peer scoring, in-memory quarantine/ban, and topology-hint confirmation before third-party peers become active
- prints periodic status lines, including whether join resolution succeeded

This is the binary to use when you want a non-graphical simulator that participates in the runtime network and actually owns a running world loop.

## Interface

This is a CLI/headless binary.
- No graphical window
- No interactive shell
- It runs until you stop it, unless the loaded config sets a positive `frames` limit
- Status appears on stdout/log output

## Command line arguments

The simulator currently parses only these arguments:
- `--config <path>`
  - Path to the node config file
  - Default: `config.json`
- `--realm-dir <path>`
  - Realm directory containing at least `realm.json` and `jump_nodes.json`
  - Default: `realms/test`
- `--jump-node-index <index>`
  - Zero-based index into the realm's `jump_nodes.json`
  - Default: `0`

Notes:
- There is no built-in `--help` output in the current simulator binary.
- Unknown flags are ignored by the current boot-config parser instead of producing a usage error.

## Config fields that matter most

The simulator reads the same shared flat root config as the relay, plus simulation loop controls.
The most relevant fields are:
- `bindAddress.host`
- `bindAddress.port`
- `enabled`
- `acceptsJoins`
- `position.{x,y,z}`
- `areaOfInterest.{radiusX,radiusY,radiusZ}`
- `join.maxCandidates`
- `join.maxHops`
- `limits.maxNodeConnections`
- `limits.maxKnownNodes`
- `periodsMs.neighborRefresh`
- `periodsMs.topologyBroadcast`
- `periodsMs.playerBroadcast`
- `receiveTimeoutMs`
- `frames`
- `frameTime`
- `sleepMs`

Behavior notes:
- `enabled: true` is what makes the runtime session actually start.
- `frames: 0` means run forever.
- A positive `frames` value makes the simulator exit after that many world updates.
- `frameTime` controls the simulation timestep.
- `sleepMs` throttles the loop between frames.

## How to run it

From the repository root:

```bash
bbs build -t openrealm_simulator
./build/default-windows-x86_64/bin/Debug/openrealm-simulator.exe --config config.json --realm-dir realms/test --jump-node-index 0
```

You can also use the bbs target runner:

```bash
bbs run -t openrealm_simulator -a --config config.json --realm-dir realms/test --jump-node-index 0
```

## Example use cases

Run against the default test realm:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-simulator.exe --config config.json --realm-dir realms/test
```

Run a finite simulation by pointing at a config whose `frames` field is positive:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-simulator.exe --config config.json --realm-dir realms/test
```

Use a different bootstrap entry from `jump_nodes.json`:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-simulator.exe --config config.json --realm-dir realms/test --jump-node-index 1
```

## What you should expect when it starts

A healthy start prints repeating status lines similar to:
- `INFO: sim bind=127.0.0.1:46010 known=0 connected=0 resolved=1 pos=(0.0, 0.0, 0.0)`

The `resolved=` field tells you whether the runtime join/bootstrap path has been resolved.

If startup fails, the binary logs an error and exits with code `1`, typically because:
- the config file could not be opened or parsed
- the realm directory is missing or incomplete
- the runtime session could not bind or initialize

## When to use this binary instead of the launcher

Use `openrealm-simulator` directly when you want:
- one simulator process only
- manual control over the config file and realm path
- to test simulation/runtime behavior in isolation

Use `openrealm-node-launcher` instead when you want it to generate per-node configs and coordinate one or more simulators with one or more relays.
