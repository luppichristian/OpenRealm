# Relay node

Target mapping:
- build target id: `openrealm_relay`
- compiled binary name: `openrealm-relay`

## What it does

The relay node is a headless runtime/bootstrap process for OpenRealm.

In the current codebase it:
- loads the shared flat `config.json`
- loads the selected realm from `realms/<name>/`
- starts a runtime session with role `Relay`
- optionally uses a jump node from `jump_nodes.json` for bootstrap/discovery
- runs the runtime tick loop without owning a gameplay `World`
- prints periodic status lines such as bind address, known nodes, connected nodes, and local position

This is the right binary when you want a non-graphical runtime node that other nodes can discover or join through.

## Interface

This is a CLI/headless binary.
- No graphical window
- No in-process interactive shell
- It runs until you stop it, unless the loaded config sets a positive `ticks` limit
- Status appears on stdout/log output

## Command line arguments

The relay currently parses only these arguments:
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
- There is no built-in `--help` output in the current relay binary.
- Unknown flags are ignored by the current boot-config parser instead of producing a usage error.

## Config fields that matter most

The relay reads the shared flat root config through `NodeConfigFiles`.
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
- `ticks`

Behavior notes:
- `enabled: true` is what makes the runtime session actually start.
- `ticks: 0` means run forever.
- A positive `ticks` value makes the relay exit on its own after that many runtime ticks.

## How to run it

From the repository root:

```bash
bbs build -t openrealm_relay
./build/default-windows-x86_64/bin/Debug/openrealm-relay.exe --config config.json --realm-dir realms/test --jump-node-index 0
```

You can also use the bbs target runner:

```bash
bbs run -t openrealm_relay -a --config config.json --realm-dir realms/test --jump-node-index 0
```

## Example use cases

Run against the default test realm:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-relay.exe --config config.json --realm-dir realms/test
```

Run against a custom realm directory:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-relay.exe --config config.json --realm-dir realms/main
```

Use a different bootstrap entry from `jump_nodes.json`:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-relay.exe --config config.json --realm-dir realms/test --jump-node-index 1
```

## What you should expect when it starts

A healthy start prints repeating status lines similar to:
- `INFO: relay bind=127.0.0.1:46010 known=0 connected=0 pos=(0.0, 0.0, 0.0)`

If startup fails, the binary logs an error and exits with code `1`, typically because:
- the config file could not be opened or parsed
- the realm directory is missing or incomplete
- the runtime session could not bind or initialize

## When to use this binary instead of the launcher

Use `openrealm-relay` directly when you want:
- one relay process only
- explicit manual control over the config file and realm path
- to supervise the process yourself from a shell, script, or service manager

Use `openrealm-node-launcher` instead when you want it to generate per-node configs and launch multiple relay/simulator processes together.
