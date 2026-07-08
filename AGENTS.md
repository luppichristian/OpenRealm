# AGENTS.md

## Maintenance Rule

Treat this file as the agent's working memory for the repository.

- Always keep `AGENTS.md` up to date automatically when you discover, change, or establish project conventions, architecture, directory structure, build rules, or important implementation constraints.
- Update this file as part of the same work when those facts change; do not leave it stale for later.
- Prefer correcting `AGENTS.md` immediately over relying on implicit memory or prior conversation context.

## Project

OpenRealm is a C++20 voxel game prototype aimed at a serverless, decentralized voxel world.

The intended product direction is a multiplayer voxel multiverse with no single authoritative gameplay server: a decentralized runtime layer of nodes in C/C++ plus an orchestration layer on Ethereum-compatible chains for ownership, registration, and market mechanics.

What exists in this repository today is the local engine/client foundation:

- real-time game loop
- local voxel storage and editing
- player simulation and collision
- asynchronous chunk meshing
- raylib-based rendering/audio

Do not describe this repo as if it already contains distributed networking or consensus logic. The decentralized/serverless model is the project direction, not the current implementation state.

## Tech Stack

- Language: C++20
- Build system: `bbs`
- Windowing/render/input/audio: `raylib`
- Profiling dependency: `tracy`
- Runtime node-to-node networking scaffold: ENet binary UDP transport
- Blockchain JSON-RPC integration scaffold: `cpp-httplib` + `nlohmann/json`
- Terminal configuration UI: vendored `term.h` (`node/cli/third_party/term.h`) with a small native C implementation unit linked into the node targets

## Repository Layout

- `node/targets/`
  - Executable entry points and role-specific orchestration for the distinct node types.
  - `Client.cpp` owns the playable client node entrypoint, creates the shared `TaskManager`, creates `Game`, and passes the manager into `Run()`.
  - `Simulator.cpp` owns the headless simulator node entrypoint and real world-update/runtime-session loop.
  - `Relay.cpp` owns the lightweight relay node entrypoint and relay service/smoke verification modes.
  - `RuntimeNodeConfigBase.h` holds the shared relay/simulator runtime config fields so the native node targets do not duplicate wallet/bind/node-id/realm defaults.
  - The target entrypoints should keep heavyweight node roots (`Game`, `World`, shared `TaskManager`) in static storage instead of stack locals because the world/client state is large enough to risk Windows stack overflow in tiny headless launchers.
- `node_launcher/`
  - Root-level utility subproject for multi-node local runtime sessions.
  - `main.cpp` clones the selected base `config.json`, creates a session-local realm copy under `build/node_launcher/<timestamp>/realm`, rewrites jump-node discovery to the launched local relay ports, and spawns multiple `openrealm-relay` / `openrealm-simulator` child processes with `--no-cli`.
  - Generated per-node configs and logs live under `build/node_launcher/<timestamp>/` so launcher sessions stay out of committed source paths.
- `node/runtime/`
  - Runtime node-to-node transport code and node-local runtime configuration.
  - `NodeConfigFiles.*` loads root `config.json` so simulator/relay nodes can source wallet, bind, node-id, runtime-loop, and local node defaults from JSON instead of verbose CLI flags.
  - Root `config.json` should stay target-agnostic for shared runtime settings: common runtime fields belong under shared sections like `runtime`, `runtime.interest`, and `runtime.limits` instead of being split per target type.
  - `ProtocolVersion.h` centralizes the explicit runtime protocol version (`kRuntimeProtocolVersion`) and packet-header version (`kRuntimePacketVersion`).
  - `RuntimeClient.*` wraps the current ENet binary-packet scaffold.
  - `Packet.*` owns the current runtime binary packet header/payload helpers, including handshake and peer-discovery payload encoding/decoding.
  - `ActiveNodeBucket.*` tracks live runtime peers by node id and peer address so duplicate node identities can be rejected.
  - `PacketValidator.*` validates incoming runtime packets, decodes handshake payloads, enforces explicit protocol-version + realm-hash matching, and rejects duplicate/self node ids.
  - `RuntimeRealm.*` builds the runtime realm fingerprint/hash from the explicit runtime protocol version plus blockchain config + fetched global params so peers can confirm they are on the same environment.
  - `RuntimeHash.*` owns the current 64-bit runtime hash helper used for realm fingerprints.
- `node/cli/`
  - Shared console configuration UI for non-client nodes.
  - `NodeCli.*` is now a term.h-based full-screen TUI for relay/simulator nodes: arrow-key navigation, inline editors/toggles, realm picker, dirty-state tracking, reload/save/launch actions, and a live realm/launch summary panel.
  - `NodeCliTerm.c` is the tiny C implementation unit that defines `TIMPLEMENTATION`; `node/cli/third_party/term.h` is vendored from the upstream `term.h` repository.
  - Client nodes should not use this CLI because they are expected to get in-game GUI configuration later.
- `node/blockchain/`
  - Native-side orchestration-layer / JSON-RPC integration code.
  - `RealmConfigFiles.*` loads selected realm data from `<realm>/realm.json` and `<realm>/jump_nodes.json`, including blockchain config, realm name, and jump-node defaults.
  - `SmartContract.*` is the common base for native contract wrappers; the shared RPC/address helpers now live in `SmartContract.cpp` instead of being fully inlined in the header.
  - `ProtocolVersion.h` centralizes the explicit blockchain/orchestration protocol version (`kBlockchainProtocolVersion`).
  - `BlockchainConfig.*` holds the native-side blockchain interaction configuration: protocol version, RPC URL, contract addresses, and request timeouts.
  - `BlockchainAbi.*` contains the current lightweight Ethereum ABI call encoding/decoding helpers used by the native wrappers.
  - `Wallet.*` currently abstracts the selected wallet account for the native layer; transaction signing/submission is not implemented yet.
  - `BlockchainRpcClient.*` wraps JSON-RPC calls to the orchestration-layer backend.
  - `GlobalParamsContract.*`, `PlayerRegistryContract.*`, `ChunkClaimsContract.*`, and `MarketplaceContract.*` are the native contract wrapper classes for the current runtime-facing orchestration reads; each wrapper keeps its own related POD read models in its header instead of using a shared blockchain-types header.
- `node/client/Game.*`
  - App shell and frame loop.
  - Owns window/audio init and shutdown.
  - Owns the top-level `World`, `ClientWorld`, `ClientMenu`, and `ColorMenu`.
  - Receives the shared `TaskManager` from the entrypoint instead of owning it.
  - Initializes the window/audio/task-manager shell up front, keeps the client main menu open before gameplay starts, and creates/shuts down the playable world session when entering or leaving play.
- `node/client/ClientMenu.*`
  - In-game GUI menu flow for client nodes.
  - Owns the main menu, play/network selection menu, options menu, credits screen, and pause menu.
  - Persists client-only settings and selected realm/jump node through `config.json`.
  - Menu hitboxes/interaction rows should share the same layout constants as the drawn widgets so clickable regions stay aligned with the visible controls.
  - Long client-menu text should be wrapped or width-fitted to the menu panel instead of being drawn as a single unbounded line.
- `node/client/ClientConfigFiles.*`
  - Client-only config/realm file helpers.
  - Loads/saves the `client` block in root `config.json` and discovers playable realms from `realms/*`.
- `node/client/ClientWorld.*`

  - Owns asset/audio caches, GPU upload state, and render orchestration for a `World`.
  - Receives the shared `TaskManager` from `Game` instead of owning worker threads directly.
- `node/client/PlayerController.*`
  - Free-function input layer.
  - Converts mouse/keyboard input into `WorldEvent`s.
  - Applies client-configured mouse sensitivity and invert-Y options before sending look events.
  - Draws HUD elements.
- `node/client/ColorMenu.*`

- `node/client/AssetManager.*`
  - Lazy asset cache for textures, shaders, shader locations, and sounds.
- `node/client/SoundPlayer.*`
  - Local sound playback helper with pooled alias voices.
- `node/client/WorldClientData.h`
  - Client-owned per-chunk-section render state.
  - Keeps `Model`, bounds, and queued/uploaded flags out of simulation data.
- `node/client/WorldMeshSystem.*`
  - Client-side mesh job capture, queueing, and GPU upload application.
  - Submits chunk mesh jobs into the shared `TaskManager` and owns only the completed-result queue.
- `node/client/ChunkMesher.*`
  - Actual chunk mesh generation implementation.
  - Greedy-meshing helper state now travels through small local parameter structs (`GreedyQuadParams`, `GreedyPlaneParams`, `FaceSampleQuery`) instead of long helper argument lists.
- `node/client/ChunkMeshBuilder.*`
  - Raw mesh buffer builder.
- `node/client/ChunkMeshJobResult.*`
  - Move-only mesh job result container.
- `node/client/WorldRenderer.*`
  - Free-function world rendering path.
  - Uses `PlayerSystem::CameraVectors` plus small file-local render/culling structs instead of long camera-argument helper signatures.
- `node/Utils.h`
  - Header-only utilities used across world/render/gameplay code.
  - Defines the shared `NonCopyable` base used by resource-owning/client/world coordinator types.
- `node/Base.h`
  - Central low-level include hub for raylib, raymath, and C headers.
- `node/TaskManager.*`
  - Global worker-thread task queue for generic background jobs.
  - Owns thread startup, shutdown, and pending-task dispatch independent of client systems.
- `node/world/World.*`
  - Top-level world coordinator.
  - Headless simulation coordinator.
  - Owns voxel data, player system, and event queue only.
- `node/world/VoxelWorld.*`
  - Authoritative local voxel/chunk storage and collision/raycast logic.
  - Axis sweep collision queries now travel through `VoxelWorld::MovementSweep` instead of passing raw bounds/axis/delta scalars around together.
- `node/world/PlayerSystem.*`
  - Player state management and movement/look/edit simulation.
  - Builds camera basis data through `PlayerSystem::CameraVectors BuildCameraVectors(...)` instead of multi-pointer output arguments.
- `node/world/WorldEvent.h`
  - World event enum and payload struct.
- `node/world/WorldConfig.h`
  - Global compile-time constants.
  - Chunk X/Y coordinates are clamped to the inclusive range `[-30000, 30000]` via `MIN_CHUNK_COORD` and `MAX_CHUNK_COORD`.
- `node/world/WorldData.h`
  - Shared POD data structures for chunks, players, ray hits, and movement results.
- `assets/`
  - Runtime shaders and sounds.
- `build/`
  - Local build artifacts.
- `dist/`
  - Output/distribution artifacts.
- `realms/`
  - Root folder for runtime realm-data directories.
  - `realms/main/` stores the production/main realm config (`realm.json`, `jump_nodes.json`) plus the realm-specific deployment entrypoints (`deploy.js`) used for real deployments.
  - `realms/*/realm.json` now explicitly carries the orchestration/blockchain `protocolVersion` for that realm alongside RPC/contract settings.
  - `realms/test/` stores the local/test runtime environment plus its Ganache/test-realm deployment wrappers (`start-ganache-local.js`, `deploy-local.js`, `deploy.js`).
- `config.json`
  - Root runtime node configuration file.
  - Stores the selected default realm plus target-agnostic runtime settings, service/simulation controls, client menu settings, and wallet account identity.
  - Now also stores a `client` object for client-node menu settings (`realm`, `jumpNodeIndex`, `masterVolume`, `mouseSensitivity`, `invertMouseY`, `showFps`).
- `blockchain/`
  - Root for the orchestration-layer work separate from the C++ runtime/client code.

  - The local Solidity helper at `blockchain/test/helpers/compileContracts.js` should pin `evmVersion: 'shanghai'` so bytecode stays compatible with the Ganache version used by this repo's local verification flow.
  - The blockchain helper surface is JavaScript-only: build/test/deploy helpers live in `package.json`, `blockchain/scripts/*.js`, and the realm-local wrappers under `realms/test/`.
  - Generic realm-aware deployment entrypoints live under `blockchain/scripts/`, especially `deploy.js`, `deployRealm.js`, and `realmConfig.js`; test-only Ganache launch/deploy wrappers live under `realms/test/`, while real-deployment wrappers live under `realms/main/`.
  - Intended for Solidity contracts and related specs/scripts/tests for registration, chunk claims, ownership, and marketplace logic.

  - Current key files include `contracts/GlobalParams.sol`, `contracts/PlayerRegistry.sol`, `contracts/ChunkClaims.sol`, `contracts/Marketplace.sol`, `specs/orchestration-layer.md`, and `test/orchestration.test.js`.
  - Blockchain contract/interface/file names intentionally omit the redundant `OpenRealm` prefix; prefer concise names like `PlayerRegistry`, `ChunkClaims`, `Marketplace`, `IPlayerRegistry`, and `IChunkClaims`.

  - `GlobalParams` is the shared on-chain source for orchestration-layer tuning values such as chunk coordinate bounds, `MIN_CHUNK_PRICE`, max fee bps, and minimum auction duration; deployment records include both the contract address and the configured values for later runtime fetching.
  - `PlayerRegistry` now also owns expiring runtime-session authorizations so the upcoming runtime layer can resolve gameplay/session signers back to registered wallet accounts.
  - `ChunkClaims` now exposes `GetChunkRuntimeState(...)` and `EditorEpochOfChunk(...)` as the main runtime-facing permission/query surface.
  - Free chunks are claimed by paying exactly `GlobalParams.MIN_CHUNK_PRICE`; abandoning a chunk refunds that locked minimum purchase price to the owner and returns the chunk to the free pool.
  - `Marketplace` now exposes unified sale-state reads (`GetSaleStateForChunk`, `GetSaleStateForToken`) for runtime/UI integration.
  - Marketplace listings and auction reserve prices must be at least `GlobalParams.MIN_CHUNK_PRICE`; fee bps validation is capped by `GlobalParams.MAX_FEE_BPS`.
  - Build/deploy helpers live under `blockchain/scripts/`; `npm run build` writes JSON artifacts under `blockchain/artifacts/`, and the generic `npm run deploy ...` path writes deployment records under `blockchain/deployments/`.
- `.github/workflows/`
  - `orchestration-test.yml` is the main orchestration CI workflow: on pushes and pull requests that touch orchestration-layer files or `realms/test/**` it runs one job that builds the Solidity artifacts, runs the blockchain tests, then smoke-deploys the **test realm** to an ephemeral local Ganache instance through `realms/test/deploy.js`.
  - `orchestration-deploy.yml` is the manual deployment workflow: on `workflow_dispatch` it first runs the same local test-realm smoke deploy, then deploys the **main realm** through `realms/main/deploy.js` when `ORCHESTRATION_RPC_URL` and `ORCHESTRATION_DEPLOY_PRIVATE_KEY` are available.

## Architecture Notes

- The `project.bbs` native targets are split by node type:
  - `openrealm_client` builds `openrealm-client` from the client/world/task-manager folders plus `node/targets/Client.cpp`.
  - `openrealm_simulator` builds `openrealm-simulator` from `node/TaskManager.cpp`, `node/runtime/*.cpp`, `node/cli/*.cpp`, `node/blockchain/*.cpp`, `node/world/*.cpp`, and `node/targets/Simulator.cpp`.
  - `openrealm_relay` builds `openrealm-relay` from `node/runtime/*.cpp`, `node/cli/*.cpp`, `node/blockchain/*.cpp`, and `node/targets/Relay.cpp`.
  - `openrealm_node_launcher` builds `openrealm-node-launcher` from `node_launcher/*.cpp` and is responsible for assembling disposable per-session configs/logs and spawning multiple headless relay/simulator node processes for a chosen realm.
  - The term.h TUI is linked through a dedicated `termh_impl` C static library (`node/cli/NodeCliTerm.c`) instead of forcing the C implementation directly through the C++ targets.
  - The simulator/relay targets no longer hardcode `ws2_32` / `winmm`; ENet's package target supplies the required platform linkage transitively, which keeps `project.bbs` itself platform-agnostic.

- The codebase is mostly split into two layers:
  - client/app shell in `node/client/`
  - world/simulation systems in `node/world/`

- The long-term product architecture is expected to become three concerns that should stay conceptually separate:
  - orchestration layer: blockchain/smart-contract systems for chunk claims, user registration/unregistration, wallet-linked identity, and chunk marketplace/business logic
  - runtime layer: decentralized node network in C/C++ that simulates world state and exchanges world events peer-to-peer
  - local client layer: rendering, input, audio, and UX on top of the runtime/world simulation

- The `blockchain/` directory is the repository root for orchestration-layer implementation and documentation; it should stay focused on low-frequency ownership/economic concerns and not absorb real-time simulation responsibilities.
- Smart-contract usage is intended for low-frequency, ownership/economic operations rather than moment-to-moment gameplay synchronization.
- Chunk ownership/claiming is expected to be blockchain-backed; chunks are intended to be purchasable/tradable, with marketplace transactions taking a project-controlled fee.
- Wallet connection is expected to be part of user identity/authentication because registration and ownership operations are tied to Ethereum-based smart contracts.
- The world is intended to be runtime-resident rather than disk-persistent by default: the shared world/network exists only while some nodes continue running it.
- If all simulation-bearing nodes responsible for a region go offline, voxel chunk contents for that region are expected to disappear; ownership metadata remains preserved through the orchestration/blockchain layer.
- World continuity/fidelity is expected to scale with active node coverage: fewer nodes means thinner replication and lower continuity, while more nodes means stronger continuity and resilience.
- Non-client nodes are expected to stay cheap/lightweight to run so world continuity can emerge from many active participants rather than a few heavy hosts.
- Runtime nodes are expected to join the wider network through one or more known "jump nodes" that act as discoverable entry points; project-hosted jump nodes are acceptable, but player-hosted jump nodes should also remain possible.
- The current native runtime targets source realm-scoped blockchain/jump-node data from JSON files: `realms/<name>/realm.json` for blockchain config and `realms/<name>/jump_nodes.json` for known entry nodes, with root `config.json` selecting the default realm path (for example `realms/test`) and providing node-local wallet/bind settings plus shared target-agnostic runtime defaults.
- A jump node is a public entry/discovery role layered on top of a known node address; it should provide first contact, peer-list sharing, handshake/compatibility checks, and optional relay fallback, but should not imply gameplay authority.

- Peer neighborhood design should prefer locality-aware connectivity: nodes responsible for nearby world regions should connect more directly, while distant regions do not need dense direct links.
- The network topology should be locality-biased rather than a full mesh: nearby nodes cluster more densely, distant communication normally traverses graph hops, and sparse long-range links/relay assistance preserve resilience.
- Region-of-interest should be a first-class runtime concept and may be centered around the local player, claimed/hosted chunks, settlements, or manually configured sim areas.
- Current node-role vocabulary:
  - sim node: simulates world state without rendering
  - relay node: forwards/relays network traffic to help connectivity without simulating gameplay locally
  - client node: simulates, renders, and communicates as a player-facing node
  - jump node: discoverable entrypoint label for a known node; conceptually a public-facing relay/discovery role rather than a gameplay authority
- Client nodes are inherently simulation-capable for the regions they participate in; dedicated sim nodes should stay headless, and relay nodes should remain relay-only/lightweight by default.
- Chunk liveness currently assumes no mandatory minimum replication factor for MVP: a single active sim-bearing node can keep a chunk alive, nearby nodes may inherit responsibility, and relay-only nodes do not keep chunks alive.
- Runtime authority for chunks should be local and dynamic: nearby active simulation-capable nodes are the intended basis for authority, and ownership should not automatically equal simulation authority.
- Runtime edit propagation should flow from client action to relevant sim-bearing authority, which validates against ownership/permission rules and then rebroadcasts accepted edits to neighboring peers.
- Early conflict resolution should prefer authority-local deterministic ordering / first-valid-edit-seen semantics instead of timestamp-trusting or CRDT-heavy designs.
- Chunk ownership is expected to grant at least build rights, allow/deny/delegate permission control, and transfer/sale rights; claims should not expire by default, voluntary abandonment should remain possible, and chunk subdivision is deferred.
- Runtime permission enforcement is expected to cache ownership/permission state from the orchestration layer, reject unauthorized edits, and bind important actions to runtime identities linked to wallet-backed ownership where relevant.
- Runtime identity delegation is currently expected to use wallet-authorized expiring runtime-session keys recorded in `PlayerRegistry`; runtime code should resolve incoming signers through the registry before applying ownership/permission rules.
- Current runtime handshake validation is intentionally environment-aware: nodes advertise a realm hash derived from blockchain config plus fetched `GlobalParams`, and handshake acceptance also rejects duplicate node ids coming from different peer addresses.
- Peer discovery packets currently advertise the requester's node id plus a filtered list of known same-realm peers from `ActiveNodeBucket`; the relay smoke target exercises discovery by excluding the requester and returning the remaining compatible peers.
- Runtime chunk-interest packets currently advertise a node id plus center chunk/radius subscription data; the relay keeps the latest interest per node and uses it to prefer locality-aware forwarding of runtime world-event packets, with a same-realm peer fallback when no explicit interest match is available yet.
- The current runtime sim-node verification path uses per-process config files with unique simulator `nodeId` and bind-port values; with two simulator nodes on the same relay, one simulator can publish a configured place event and another interested simulator can receive and apply that forwarded runtime world-event into its live world state.
- Wallet identity currently assumes one wallet maps to one registered player identity at a time; guest/local-only play may exist, but guests should not exercise ownership-derived rights.
- Important early runtime security concerns are fake edits, stale-state replay, false peer advertisements, spam/flooding, and eclipse/isolation attempts; early mitigations should include authentication hooks for important actions, rate limits, replay protection, permission checks, peer sanity checks, and handshake version checks.
- Early marketplace scope should stay narrow: wallet connection, registration, chunk claims, and later simple buy/sell/transfer flows with percentage-fee marketplace mediation; NFT compatibility is acceptable, but gameplay needs take priority over NFT-first framing.
- The current concrete orchestration-layer implementation is NFT-backed: `PlayerRegistry` manages one active player identity per wallet + unique handles, `ChunkClaims` mints one ERC721-like ownership token per claimed chunk and resets delegated-editor state on transfer, and `Marketplace` supports fixed-price listings plus English auctions with protocol-fee retention.
- The current runtime-facing blockchain read model is: resolve signer/session via `PlayerRegistry`, read chunk ownership/permission state via `ChunkClaims.GetChunkRuntimeState(...)`, and read active sale state via `Marketplace.GetSaleStateForChunk(...)`.
- Claimed chunks are expected to be economically meaningful even when voxel state is volatile; chunks nearer world origin `(0, 0)` are expected to be more valuable because they are more likely to stay alive due to denser node activity.
- Suggested implementation order is: strengthen headless/runtime separation, define network protocol + identity basics, add peer discovery/jump-node flow, implement region-of-interest topology + relay behavior, implement chunk responsibility/authority, then synchronized edit propagation, then wallet/contracts, then marketplace logic.
- `World` is the main composition root for world-side systems.
- World event buffering uses `std::queue<WorldEvent>` semantics with an explicit `MAX_WORLD_EVENTS` cap enforced by `World::SendEvent()`.
- `ClientWorld` is the main composition root for client-only systems that consume `World`.
- The code favors direct ownership and explicit orchestration over abstract interfaces.
- Background task execution is centralized in `TaskManager`, owned outside both `Game` and `World` at the entrypoint layer; client mesh jobs submit directly into it from `WorldMeshSystem`, and other systems can reuse the same manager.
- Client node menu flow is GUI-driven inside the raylib window: the app boots into a main menu, the play submenu persists the selected realm/jump node, and gameplay can be suspended with `ESC` into a pause menu that can resume, open options, or return to the main menu.
- Voxel data is chunked and sectioned. Meshing is asynchronous and client-owned, while gameplay/world mutation remains local and immediate.
- Chunk storage/lookup ignores any chunk coordinates outside the inclusive X/Y bounds `[-30000, 30000]`.
- Rendering is a mix of:
  - shader-driven fullscreen floor rendering
  - uploaded chunk meshes rendered in 3D
- Input and some rendering helpers are implemented as free functions rather than class methods.
- `World` should stay usable as a headless node runtime:
  - no asset ownership
  - no audio playback
  - no GPU/model lifetime management
  - no direct frame-time polling from raylib; frame time is passed in by the caller
- Client render state lives outside `VoxelWorld`; `WorldData.h` should not regain `Model`, uploaded, or bounds fields.

## Style And Conventions

These are not generic C++ preferences. They reflect the code that is already in this repository.

### Formatting

- Follow `.clang-format`.
- 2-space indentation.
- No tabs.
- Allman braces.
- No practical line-length limit; `ColumnLimit: 0`.
- Pointer alignment is `Type* name`.
- Includes are not auto-sorted.
- `.clang-format` is for the native C/C++ codebase; do not treat it as the Solidity formatter. If Solidity formatting is added later, use a Solidity-aware formatter such as `forge fmt` or Prettier with a Solidity plugin.

### Naming

- Classes, structs, enums, functions, and methods use `PascalCase`.
- For the blockchain/Solidity workspace too, prefer `PascalCase` for public/external function names so contract APIs stay aligned with the native C++ naming style.
- When Solidity code inherits standardized APIs (for example ERC721-style methods like `tokenURI`, `balanceOf`, or `transferFrom`), keep the standard spellings for compatibility instead of force-renaming them.
- Constants use `kPascalCase` when they are scoped constants in classes or files.
- Regular variables and member fields use plain `camelCase`.
- Member fields do not use `m_`, `_`, or `s_` prefixes.
- Global compile-time configuration values in `WorldConfig.h` use `UPPER_SNAKE_CASE`-style identifiers such as `CHUNK_SIZE_XZ`, `MOVE_SPEED`, and `WORLD_UP`.

### Namespaces

- Do not introduce named namespaces by default.
- The current codebase is intentionally flat at the symbol level.
- Do not wrap the project in `namespace OpenRealm` unless the user explicitly asks for that refactor.
- For new file-local helpers in `.cpp` files, prefer `static` free functions over introducing extra namespace blocks.

### Data Modeling

- This codebase is comfortable with POD-style structs that expose public fields directly.
- `WorldData.h`, `WorldEvent.h`, and `ChunkMeshJob` are the model for this style.
- Use simple structs with defaults for state carriers instead of over-encapsulating them.
- Reserve `*State` structs for plain data carriers and read models returned from systems/contracts; do not make callers build or toggle internal lifecycle flags for owning classes through external `*State` structs.
- Designated initializers are already used and are acceptable where supported by the current compiler setup.

### Class Design

- Classes tend to be concrete and stateful.
- Ownership is direct by value where practical.
- Keep connection/lifecycle flags owned by the class itself; expose explicit methods such as connect/disconnect instead of requiring external code to assemble an internal state blob.
- Do not introduce smart pointers by default.
- Prefer direct ownership, stack allocation, plain members, or explicit raw-pointer/non-owning pointer usage consistent with the existing codebase.
- If a heap allocation is genuinely necessary, justify it with a concrete ownership/lifetime need instead of reaching for `std::unique_ptr` or `std::shared_ptr` automatically.
- Copy is often explicitly deleted for resource-owning types (use NonCopyable).
- Destructors commonly call `Shutdown()` or `Reset()` to centralize cleanup.
- Avoid adding inheritance-heavy abstractions unless there is a strong repo-local reason.

### Functions

- Free functions are a normal pattern here, especially for:
  - rendering entrypoints
  - input translation
  - small helpers
- Prefer straightforward imperative code over callback-heavy or template-heavy designs.
- Early returns are common and should remain the default for guard conditions.

### Header Patterns

- Use `#pragma once`.
- Headers commonly contain:
  - type definitions
  - inline constants
  - inline utility functions
- `Utils.h` and `WorldConfig.h` are intentionally header-driven; do not force everything into `.cpp` files just to satisfy a generic rule.

### Memory And Containers

- The code mixes STL containers and manual allocation depending on the problem:
  - `std::array`, `std::unordered_map`, `std::deque`, `std::vector`
  - `malloc`, `realloc`, `free`, `memset` in mesh-building paths
- Do not "modernize" raw allocation paths automatically if it would obscure hot-path behavior or ownership transfer.
- If touching mesh memory ownership, preserve the current release/reset semantics carefully.

## Implementation Guidance

- Keep gameplay/world logic in `node/world/` unless it is clearly client glue.
- Keep app startup, window lifecycle, rendering, meshing, audio, and high-level input flow in `node/client/`.
- Keep executable `main()` functions under `node/targets/`; do not put target entrypoints back at the root of `node/`.
- The repository root `README.md` is the main onboarding document and should describe both the native voxel engine/client foundation and the separate `blockchain/` orchestration workspace.
- The root `README.md` may use Mermaid diagrams for architecture explanations; keep them aligned with the current repo state and avoid depicting the future decentralized runtime as already implemented.
- When adding source files, check which node target should own them; the current `project.bbs` console targets select source folders directly rather than routing through repo-local static libraries.
- Current target folder mapping is:
  - `openrealm_client`: `node/TaskManager.cpp`, `node/world/*.cpp`, `node/client/*.cpp`, `node/targets/Client.cpp`
  - `openrealm_simulator`: `node/TaskManager.cpp`, `node/runtime/*.cpp`, `node/cli/*.cpp`, `node/blockchain/*.cpp`, `node/world/*.cpp`, `node/targets/Simulator.cpp`
  - `openrealm_relay`: `node/runtime/*.cpp`, `node/cli/*.cpp`, `node/blockchain/*.cpp`, `node/targets/Relay.cpp`
  - `openrealm_node_launcher`: `node_launcher/*.cpp`
- Add new executable node entrypoints as explicit `units(...)` under their corresponding `project.bbs` console target in `node/targets/` rather than globbing every target entrypoint together.
- New subdirectories under `node/` are not automatically part of the build unless `project.bbs` is updated.
- In `project.bbs`, do not hardcode machine-local package cache paths (for example `C:/Users/.../packages/...`); package include/link data must flow from declared `dependencies(...)` or from repo-relative/generated paths derived at build time.
- In `project.bbs`, do not add raw MSVC-only flags such as `/FS` through `additional_compile_args(...)`; prefer dedicated `bbs` fields or toolchain-agnostic/clang-style arguments that `bbs` can translate for MSVC.
- `openrealm_relay` now uses the `bbs` target-specific expansion token `$DEP(enet.package.resolved_dir)` to include ENet headers directly from the resolved package root (`.../include`) instead of using a repo-local header-sync workaround or hardcoded package cache path.
- If you add assets, put them under `assets/` with stable folder naming that matches the current `BuildAssetPath()` convention.
- For the blockchain workspace, `npm run build` compiles Solidity into `artifacts/`, `npm test` runs the Ganache-backed contract tests, `npm run deploy` stays the low-level generic network deploy path, and `npm run deploy:realm -- --realm <name-or-path>` applies realm defaults from `realms/<name>/realm.json`. The script surface is intentionally JavaScript-only: use `npm run verify`, `npm run ganache:test`, `npm run deploy:test:local`, or the realm-local `node .../*.js` wrappers rather than Python shims.
- Root `.gitignore` should ignore native build outputs (`build/`, `dist/`, `gen/`), machine-local `bbs` files (`config.bbs`, `toolchain.bbs`), and generated blockchain workspace directories such as `blockchain/node_modules/`, `blockchain/artifacts/`, and `blockchain/deployments/`.

## Behavior To Preserve

- Local playability comes first.
- Voxel edits should remain immediate from the local player perspective.
- Chunk meshing should remain asynchronous and budgeted.
- World state and rendering should stay simple enough to iterate on quickly.

When extending the codebase toward decentralization, build on top of these local systems instead of prematurely replacing them with a traditional client/server architecture.
