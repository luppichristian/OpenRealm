# Project Structure

## Top-level layout

Current top-level repo surface:
- `project.bbs`
  - primary native build definition
- `config.json`
  - shared root config for client/runtime/service/simulation defaults
- `.clang-format`
  - native formatting baseline
- `assets/`
  - shaders, models, textures, and sounds consumed by the native client
- `node/`
  - native runtime, blockchain wrappers, world simulation, client code, and entrypoints
- `node_launcher/`
  - local multi-process launcher that clones configs and spins up relay/simulator sessions
- `blockchain/`
  - Solidity contracts plus JS build/test/deploy scripts
- `realms/`
  - realm-scoped `realm.json`, `jump_nodes.json`, and thin deployment wrappers
- `.github/workflows/`
  - orchestration CI/deploy workflows
- `.agents/`
  - canonical agent-facing repo documentation
- `build/`
  - generated native build artifacts and launcher session output
- `AGENTS.md`, `CLAUDE.md`
  - root pointer files into `.agents/`
- `LICENSE`
  - MIT license for the repository

Generated subtrees currently worth knowing about:
- `build/default-windows-x86_64/`
  - current native build output root on this machine
- `build/node_launcher/<timestamp>/`
  - launcher-generated configs, logs, and copied realm data
- `blockchain/artifacts/`
  - generated contract artifacts plus `manifest.json`
- `blockchain/deployments/`
  - generated deployment records such as `test.json`, `main.json`, and `local.json`

A human-facing root `README.md` is present again. There is still no current `dist/` tree in the repository.

## Native directory ownership

### `node/targets/`
Executable entrypoints.

Current files:
- `Client.cpp`
  - tiny playable-client entrypoint: static `TaskManager`, static `Game`, then `game.Run(...)`
- `Simulator.cpp`
  - headless world loop + runtime session loop
- `Relay.cpp`
  - headless relay/runtime loop
- `RuntimeNodeConfigBase.h`
  - shared relay/simulator runtime config struct

### `node/client/`
Player-facing native client code.

Key responsibilities proven by `Game.cpp`, `ClientMenu.cpp`, and related files:
- window/audio lifecycle
- menus and options persistence
- realm selection and jump-node selection for the client
- runtime session startup for the playable client
- local-world rendering and mesh upload/application
- HUD, input translation, palette/color tools, and sound playback

Important files:
- `Game.*`
  - top-level app loop; starts local gameplay and optionally overlays a runtime session
- `ClientMenu.*`
  - main/pause/options flow and realm/config selection
- `ClientConfigFiles.*`
  - client config load/save and realm discovery
- `ClientWorld.*`
  - render-side world presentation/cache state
- `WorldMeshSystem.*`
  - background mesh-job scheduling and GPU application
- `ChunkMesher.*`, `ChunkMeshBuilder.*`, `ChunkMeshJobResult.*`
  - chunk mesh generation pipeline
- `PlayerController.*`
  - player input -> world events / HUD helpers
- `AssetManager.*`, `SoundPlayer.*`, `WorldRenderer.*`
  - assets, audio, and draw path helpers

### `node/world/`
Headless gameplay/world simulation.

Important files:
- `World.*`
  - world coordinator and event queue pump
- `VoxelWorld.*`
  - chunk storage, raycast/collision, voxel edits, chunk lookup
- `PlayerSystem.*`
  - spawn/despawn, movement, look, jump, place/remove voxel actions
- `WorldEvent.h`
  - event payloads passed into `World`
- `WorldConfig.h`, `WorldData.h`
  - compile-time limits and POD state

The world is intentionally usable without client render/audio ownership.

### `node/runtime/`
Runtime networking and topology code.

Important files:
- `NodeConfigFiles.*`
  - reads root `config.json` for runtime/service/simulation defaults
- `ProtocolVersion.h`
  - runtime protocol and packet versions
- `RuntimeClient.*`
  - ENet wrapper
- `Packet.*`
  - runtime packet framing/serialization
- `PacketValidator.*`
  - compatibility/sanity validation
- `RuntimeSession.*`
  - join/topology/player-replication coordinator
- `ActiveNodeBucket.*`, `ChunkInterestBucket.*`
  - live peer/interest tracking
- `RuntimeRealm.*`, `RuntimeHash.*`
  - realm hash construction

### `node/blockchain/`
Native wrappers over the orchestration layer.

Important files:
- `RealmConfigFiles.*`
  - loads `<realm>/realm.json` and `<realm>/jump_nodes.json`
- `BlockchainConfig.*`
  - native-side blockchain config state
- `BlockchainRpcClient.*`
  - JSON-RPC client wrapper
- `Wallet.*`
  - selected wallet/account wrapper
- `Blockchain.*`
  - facade combining RPC + contract wrappers
- `GlobalParamsContract.*`
- `PlayerRegistryContract.*`
- `ChunkClaimsContract.*`
- `MarketplaceContract.*`
  - native read/write wrappers around the Solidity contracts

### `node/cli/`
Terminal UI/config editor code exists here (`NodeCli.cpp`, `NodeCli.h`, `NodeCliTerm.c`).

Important caveat:
- the current `project.bbs` target lists do not compile `node/cli/*`
- the launcher still passes `--no-cli` to child node processes
- treat this directory as present repo surface, but not as a currently wired native build target

### `node_launcher/`
Local relay/simulator session orchestrator.

Current split:
- `Main.cpp`
  - top-level shim
- `App.*`
  - launch orchestration, status loop, stop handling
- `Args.*`
  - CLI parsing / help
- `Config.*`
  - config normalization, session-realm generation, generated child configs
- `Process.*`
  - child-process argument assembly and status printing
- `Platform.*`
  - OS-specific child-process behavior
- `LauncherUtilities.*`
  - path/timestamp/interest helper utilities
- `LauncherTypes.h`
  - shared structs

Observed behavior from code:
- base session root is `build/node_launcher/<timestamp>/`
- child realm copy lives under `build/node_launcher/<timestamp>/realm`
- relay and simulator configs/logs are written into that same session directory
- launched child args include `--config <generated.json> --realm-dir <session realm> --no-cli`

## Config and realm structure

### Root `config.json`
Current canonical root shape:
- `realm`
- `wallet.accountAddress`
- `client.{realm,jumpNodeIndex,joinTargetPosition,masterVolume,mouseSensitivity,invertMouseY,showFps}`
- `runtime.{enabled,acceptsJoins,bindAddress,jumpNodeIndex,nodeId,position,areaOfInterest,join,limits,periodsMs,receiveTimeoutMs}`
- `service.ticks`
- `simulation.{frames,frameTime,sleepMs}`

Important distinction:
- client-facing spawn/join defaults live under `client.joinTargetPosition`
- shared runtime transport defaults live under `runtime.*`

### `realms/`
Current realm directories:
- `realms/test/`
- `realms/main/`

Each realm carries:
- `realm.json`
  - realm name + blockchain/orchestration configuration
- `jump_nodes.json`
  - bootstrap/discovery node list

Thin JS wrappers also live there:
- `realms/test/start-ganache-local.js`
- `realms/test/deploy-local.js`
- `realms/test/deploy.js`
- `realms/main/deploy.js`

## Placement rules for new work

- Put gameplay/world logic in `node/world/` unless it is clearly client-only glue.
- Put rendering, menus, audio, and input orchestration in `node/client/`.
- Keep executable entrypoints in `node/targets/`.
- Put runtime transport/topology changes in `node/runtime/`.
- Put native contract/RPC integration in `node/blockchain/`.
- Update `project.bbs` when adding a new source directory that should actually compile.
- Do not assume every directory under `node/` is currently wired into a target; `node/cli/` is the main counterexample today.
