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

## Repository Layout

- `node/targets/`
  - Executable entry points for the distinct node types.
  - `Client.cpp` owns the playable client node entrypoint, creates the shared `TaskManager`, creates `Game`, and passes the manager into `Run()`.
  - `Simulator.cpp` owns the headless simulator node entrypoint.
  - `Relay.cpp` owns the lightweight relay node entrypoint and currently exercises the runtime-networking/blockchain integration scaffolds.
- `node/runtime/`
  - Runtime node-to-node transport code only.
  - `RuntimeNetClient.*` wraps the current ENet binary-packet scaffold.
- `node/blockchain/`
  - Native-side orchestration-layer / JSON-RPC integration code.
  - `SmartContract.h` is the common base for native contract wrappers; it centralizes shared RPC/address behavior.
  - `Blockchain.*` is the unified native blockchain facade that owns the RPC client, wallet abstraction, and contract wrappers.
  - `BlockchainConfig.*` holds the native-side blockchain interaction configuration: RPC URL, contract addresses, and request timeouts.
  - `BlockchainAbi.*` contains the current lightweight Ethereum ABI call encoding/decoding helpers used by the native wrappers.
  - `Wallet.*` currently abstracts the selected wallet account plus optional runtime signer metadata for the native layer; transaction signing/submission is not implemented yet.
  - `BlockchainRpcClient.*` wraps JSON-RPC calls to the orchestration-layer backend.
  - `GlobalParamsContract.*`, `PlayerRegistryContract.*`, `ChunkClaimsContract.*`, and `MarketplaceContract.*` are the native contract wrapper classes for the current runtime-facing orchestration reads; each wrapper keeps its own related POD read models in its header instead of using a shared blockchain-types header.
- `node/client/Game.*`
  - App shell and frame loop.
  - Owns window/audio init and shutdown.
  - Owns the top-level `World`, `ClientWorld`, and `ColorMenu`.
  - Receives the shared `TaskManager` from the entrypoint instead of owning it.
- `node/client/ClientWorld.*`
  - Client-only composition root.
  - Owns asset/audio caches, GPU upload state, and render orchestration for a `World`.
  - Receives the shared `TaskManager` from `Game` instead of owning worker threads directly.
- `node/client/PlayerController.*`
  - Free-function input layer.
  - Converts mouse/keyboard input into `WorldEvent`s.
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
- `node/client/ChunkMeshBuilder.*`
  - Raw mesh buffer builder.
- `node/client/ChunkMeshJobResult.*`
  - Move-only mesh job result container.
- `node/client/WorldRenderer.*`
  - Free-function world rendering path.
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
- `node/world/PlayerSystem.*`
  - Player state management and movement/look/edit simulation.
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
- `blockchain/`
  - Root for the orchestration-layer work separate from the C++ runtime/client code.
  - Contains a Node-based Solidity workflow (`package.json`) using `solc` + `ganache` + `ethers` + `mocha` for local contract testing.
  - Includes Windows helper batch scripts at the `blockchain/` root for common local flows: `install-deps.bat`, `build-contracts.bat`, `test-blockchain.bat`, `start-ganache-local.bat`, `deploy-local.bat`, and `verify-local.bat`.
  - Intended for Solidity contracts and related specs/scripts/tests for registration, chunk claims, ownership, and marketplace logic.
  - Current key files include `contracts/GlobalParams.sol`, `contracts/PlayerRegistry.sol`, `contracts/ChunkClaims.sol`, `contracts/Marketplace.sol`, `specs/orchestration-layer.md`, and `test/orchestration.test.js`.
  - Blockchain contract/interface/file names intentionally omit the redundant `OpenRealm` prefix; prefer concise names like `PlayerRegistry`, `ChunkClaims`, `Marketplace`, `IPlayerRegistry`, and `IChunkClaims`.
  - `GlobalParams` is the shared on-chain source for orchestration-layer tuning values such as chunk coordinate bounds, `MIN_CHUNK_PRICE`, max fee bps, and minimum auction duration; deployment records include both the contract address and the configured values for later runtime fetching.
  - `PlayerRegistry` now also owns expiring runtime-session authorizations so the upcoming runtime layer can resolve gameplay/session signers back to registered wallet accounts.
  - `ChunkClaims` now exposes `GetChunkRuntimeState(...)`, `CanEditWithRuntimeSigner(...)`, and `EditorEpochOfChunk(...)` as the main runtime-facing permission/query surface.
  - Free chunks are claimed by paying exactly `GlobalParams.MIN_CHUNK_PRICE`; abandoning a chunk refunds that locked minimum purchase price to the owner and returns the chunk to the free pool.
  - `Marketplace` now exposes unified sale-state reads (`GetSaleStateForChunk`, `GetSaleStateForToken`) for runtime/UI integration.
  - Marketplace listings and auction reserve prices must be at least `GlobalParams.MIN_CHUNK_PRICE`; fee bps validation is capped by `GlobalParams.MAX_FEE_BPS`.
  - Build/deploy helpers live under `blockchain/scripts/`; `npm run build` writes JSON artifacts under `blockchain/artifacts/`, and `npm run deploy ...` writes deployment records under `blockchain/deployments/`.
- `.github/workflows/`
  - `orchestration-test.yml` runs the blockchain workspace build/tests on pushes and pull requests that touch orchestration-layer files.
  - `orchestration-deploy.yml` provides a manual workflow that first smoke-deploys the contracts to ephemeral Ganache, then can deploy to a configured external JSON-RPC network when `ORCHESTRATION_RPC_URL` and `ORCHESTRATION_DEPLOY_PRIVATE_KEY` are available.

## Architecture Notes

- The `project.bbs` native targets are split by node type:
  - `openrealm_client` builds `openrealm-client` from the client/world/task-manager folders plus `node/targets/Client.cpp`.
  - `openrealm_simulator` builds `openrealm-simulator` from the world/task-manager folders plus `node/targets/Simulator.cpp`.
  - `openrealm_relay` builds `openrealm-relay` from the runtime/blockchain folders plus `node/targets/Relay.cpp`.
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
- A jump node is a public entry/discovery role layered on top of a known node address; it should provide first contact, peer-list sharing, handshake/compatibility checks, and optional relay fallback, but should not imply gameplay authority.
- The decentralized node graph should preserve reachability across the same world/network even when peers do not all connect directly.
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
- Designated initializers are already used and are acceptable where supported by the current compiler setup.

### Class Design

- Classes tend to be concrete and stateful.
- Ownership is direct by value where practical.
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
  - `openrealm_simulator`: `node/TaskManager.cpp`, `node/world/*.cpp`, `node/targets/Simulator.cpp`
  - `openrealm_relay`: `node/runtime/*.cpp`, `node/blockchain/*.cpp`, `node/targets/Relay.cpp`
- Add new executable node entrypoints as explicit `units(...)` under their corresponding `project.bbs` console target in `node/targets/` rather than globbing every target entrypoint together.
- New subdirectories under `node/` are not automatically part of the build unless `project.bbs` is updated.
- If you add assets, put them under `assets/` with stable folder naming that matches the current `BuildAssetPath()` convention.
- For the blockchain workspace, `npm run build` compiles Solidity into `artifacts/`, `npm test` runs the Ganache-backed contract tests, and `npm run deploy` / `npm run deploy:local` deploy the registry + claims + marketplace stack and write `deployments/<network>.json`.
- Root `.gitignore` should ignore native build outputs (`build/`, `dist/`, `gen/`), machine-local `bbs` files (`config.bbs`, `toolchain.bbs`), and generated blockchain workspace directories such as `blockchain/node_modules/`, `blockchain/artifacts/`, and `blockchain/deployments/`.

## Behavior To Preserve

- Local playability comes first.
- Voxel edits should remain immediate from the local player perspective.
- Chunk meshing should remain asynchronous and budgeted.
- World state and rendering should stay simple enough to iterate on quickly.

When extending the codebase toward decentralization, build on top of these local systems instead of prematurely replacing them with a traditional client/server architecture.
