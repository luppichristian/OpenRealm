# Build Targets

## Build frontend

The primary native build frontend is `bbs` from the repo root.

Useful baseline commands:
- `bbs info project`
- `bbs update --init-toolchain`
- `bbs build`
- `bbs build -t <target>`
- `bbs run -t <target>`

Recent direct verification:
- `bbs info project` successfully enumerates the targets in `project.bbs`
- `bbs build -t openrealm_node_launcher` succeeds and produces `build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe` on this machine

## External dependencies declared in `project.bbs`

Package-backed targets:
- `raylib`
  - repo: `https://github.com/raysan5/raylib.git`
  - tag: `6.0`
  - CMake target: `raylib`
- `tracy`
  - repo: `https://github.com/wolfpld/tracy.git`
  - tag: `v0.13.1`
  - CMake target: `TracyClient`
- `httplib`
  - repo: `https://github.com/yhirose/cpp-httplib.git`
  - tag: `v0.20.1`
  - CMake target: `httplib::httplib`
- `nlohmann_json`
  - repo: `https://github.com/nlohmann/json.git`
  - tag: `v3.11.3`
  - CMake target: `nlohmann_json::nlohmann_json`
- `enet`
  - repo: `https://github.com/lsalzman/enet.git`
  - tag: `v1.3.18`
  - CMake target: `enet`
- `termh`
  - repo: `https://github.com/luppichristian/term.h.git`
  - commit: `8e20a9f1e33a6d54de9dab4cc81db3ef9ada9d62`
  - CMake target: `termh_header`
  - include dir: `$TARGET(package.resolved_dir)`

## Native executable targets

### `openrealm_client`
- output: `openrealm-client`
- language: `cpp`
- standard: `c++20`
- dependencies: `raylib`, `tracy`, `httplib`, `nlohmann_json`, `enet`
- units:
  - `node/TaskManager.cpp`
  - `node/runtime/*.cpp`
  - `node/blockchain/*.cpp`
  - `node/world/*.cpp`
  - `node/client/*.cpp`
  - `node/targets/Client.cpp`

Use when touching the playable windowed client, menus, rendering, local world UX, or the client-side runtime join path.

### `openrealm_simulator`
- output: `openrealm-simulator`
- language: `cpp`
- standard: `c++20`
- dependencies: `raylib`, `tracy`, `httplib`, `nlohmann_json`, `enet`
- units:
  - `node/TaskManager.cpp`
  - `node/runtime/*.cpp`
  - `node/blockchain/*.cpp`
  - `node/world/*.cpp`
  - `node/targets/Simulator.cpp`

Use when touching headless world simulation or simulator-side runtime behavior.

### `openrealm_relay`
- output: `openrealm-relay`
- language: `cpp`
- standard: `c++20`
- dependencies: `raylib`, `tracy`, `httplib`, `nlohmann_json`, `enet`
- units:
  - `node/runtime/*.cpp`
  - `node/blockchain/*.cpp`
  - `node/world/*.cpp`
  - `node/targets/Relay.cpp`

Use when touching bootstrap/join/topology relay behavior.

### `openrealm_node_launcher`
- output: `openrealm-node-launcher`
- language: `cpp`
- standard: `c++20`
- dependency: `nlohmann_json`
- units:
  - `node_launcher/*.cpp`
  - `node_launcher/App.cpp`

Notes:
- the wildcard already covers `App.cpp`; the explicit extra listing is present in `project.bbs` today and should be preserved unless the build file is intentionally cleaned up
- the launcher binary exposes the `--realm`, `--relays`, `--simulators`, `--config`, `--relay-base-port`, `--sim-base-port`, `--relay-base-node-id`, `--sim-base-node-id`, `--relay-ticks`, `--sim-frames`, `--sim-sleep-ms`, `--sim-frame-time`, `--launch-delay-ms`, `--run-seconds`, and `--emit-place-event` flags

## Important build conventions

- `project.bbs` is the source of truth for what actually compiles.
- New source directories under `node/` do not compile automatically; update `project.bbs` if a new subtree should be built.
- `node/cli/` exists in the repository but is not included in any current target.
- Keep executable entrypoints in `node/targets/` and `node_launcher/`.
- Relay/simulator/client targets rely on the ENet include path from `$DEP(enet.package.resolved_dir)/include`.
- Do not hardcode machine-local package cache paths into `project.bbs`.

## User-facing target docs

The repository now keeps human-readable binary usage docs under `docs/`:
- `docs/client_node.md`
- `docs/relay_node.md`
- `docs/simulator_node.md`
- `docs/node_launcher.md`

When a target's purpose, interface style, command-line arguments, generated outputs, or expected run behavior changes, agents should update these docs in the same change.

## Build/run examples

Build all native targets:
```bash
bbs build
```

Build one target:
```bash
bbs build -t openrealm_client
bbs build -t openrealm_simulator
bbs build -t openrealm_relay
bbs build -t openrealm_node_launcher
```

Run the launcher help:
```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --help
```

Launch a short local session:
```bash
bbs run -t openrealm_node_launcher -a --realm test --relays 1 --simulators 2 --run-seconds 5
```

## Which target to choose

- client menus, rendering, input, world presentation, client runtime-join UX
  - verify against `openrealm_client`
- headless runtime topology/bootstrap behavior
  - verify against `openrealm_relay`
- headless world update loop or simulator runtime behavior
  - verify against `openrealm_simulator`
- generated child configs, local relay/simulator orchestration, launcher CLI/status flow
  - verify against `openrealm_node_launcher`
