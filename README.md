# OpenRealm

OpenRealm is a C++20 voxel game prototype aimed at a future **serverless, decentralized voxel world**.

Today, this repository primarily contains the **local engine/client foundation**:
- real-time game loop
- local voxel storage and editing
- player simulation and collision
- asynchronous chunk meshing
- raylib-based rendering and audio

It also now contains a separate `blockchain/` workspace for the early **orchestration layer**: wallet-linked player registration, NFT-backed chunk claims, and marketplace logic.

> The decentralized multiplayer runtime is the project direction, not the current implementation state. This repo does **not** yet contain a full peer-to-peer runtime or consensus layer.

## High-level architecture

```mermaid
flowchart LR
  Player[Player] --> Client[Local client / renderer]
  Client --> Runtime[Runtime simulation layer\nfuture decentralized node graph]
  Runtime --> WorldState[Live voxel state\nvolatile while sim nodes are online]
  Client --> Contracts[Blockchain orchestration layer]
  Contracts --> Ownership[Chunk ownership / player identity / marketplace]

  classDef current fill:#1f2937,stroke:#60a5fa,color:#fff;
  classDef future fill:#3f3f46,stroke:#a78bfa,color:#fff;
  class Client,Contracts current;
  class Runtime,WorldState future;
```

### Separation of concerns

```mermaid
flowchart TD
  A[Local client layer] --> A1[Rendering]
  A --> A2[Input]
  A --> A3[Audio]
  A --> A4[Chunk mesh uploads]

  B[Runtime layer - planned] --> B1[Peer discovery]
  B --> B2[Region-of-interest simulation]
  B --> B3[Chunk authority / replication]
  B --> B4[Edit propagation]

  C[Orchestration layer] --> C1[Player registration]
  C --> C2[Chunk ownership]
  C --> C3[Delegated edit rights]
  C --> C4[Marketplace / protocol fees]
```

## Current code architecture

### Native client/runtime foundation

```mermaid
flowchart TD
  ClientTarget[node/targets/Client.cpp] --> TaskManager[TaskManager]
  ClientTarget --> Game[client/Game]
  Game --> ClientWorld[client/ClientWorld]
  Game --> World[world/World]
  Game --> ColorMenu[client/ColorMenu]

  ClientWorld --> AssetManager[client/AssetManager]
  ClientWorld --> MeshSystem[client/WorldMeshSystem]
  ClientWorld --> Renderer[client/WorldRenderer]
  ClientWorld --> SoundPlayer[client/SoundPlayer]

  World --> VoxelWorld[world/VoxelWorld]
  World --> PlayerSystem[world/PlayerSystem]
  World --> WorldEvents[world/WorldEvent]

  MeshSystem --> ChunkMesher[client/ChunkMesher]
  MeshSystem --> ChunkMeshBuilder[client/ChunkMeshBuilder]
  MeshSystem --> TaskManager
```

### Blockchain workspace

```mermaid
flowchart LR
  Params[GlobalParams]
  Registry[PlayerRegistry]
  Claims[ChunkClaims\nERC721-like chunk ownership]
  Market[Marketplace]

  Params --> Claims
  Params --> Market
  Registry --> Claims
  Claims --> Market
  Market --> Fees[Protocol fee retention]
  Claims --> Editors[Delegated editors]
  Claims --> Tokens[Chunk ownership tokens]
```

## Repository layout

### Root node/client code
- `node/targets/` — executable entry points for client, simulator, and relay node types
- `node/targets/Client.cpp` — client node entrypoint; creates the shared `TaskManager` and starts `Game`
- `node/targets/Simulator.cpp` — simulator node entrypoint for headless world simulation work
- `node/targets/Relay.cpp` — relay node entrypoint plus the current runtime-networking/blockchain integration smoke test
- `node/client/` — app shell, rendering, input glue, asset/audio caches, meshing, HUD/UI
- `node/world/` — headless simulation-side world systems, voxel data, player system, world events
- `node/runtime/` — node-to-node runtime transport code
- `node/cli/` — console configuration UI for relay and simulator nodes
- `node/blockchain/` — orchestration-layer / JSON-RPC integration code used by native nodes
- `node/TaskManager.*` — generic background worker queue
- `assets/` — shaders and sounds used at runtime

### Blockchain orchestration workspace
- `blockchain/contracts/` — Solidity contracts for global params, registry, chunk claims, and marketplace
- `blockchain/test/` — Mocha + Ganache contract tests
- `blockchain/scripts/` — artifact generation and deployment scripts
- `blockchain/specs/` — orchestration-layer notes/specs
- `blockchain/README.md` — detailed workflow for the blockchain subproject

## Design principles

- **Local playability first** — the current code should stay easy to run and iterate on locally
- **World logic stays headless-friendly** — `node/world/` should remain usable without graphics/audio ownership
- **Client state stays client-side** — render/upload state should not leak back into world simulation POD data
- **Asynchronous chunk meshing stays intact** — world edits are immediate; mesh generation remains backgrounded
- **Blockchain stays low-frequency** — ownership, registration, and market operations belong there; real-time gameplay does not

## Build and run

## Native app (`bbs`)

This repo uses [`bbs`](https://github.com/luppichristian/bbs) as the primary build frontend.

From the repository root:

```bash
bbs build
bbs run -t openrealm_client
```

Useful commands:

```bash
bbs info project
bbs update --init-toolchain
bbs build -t openrealm_client
bbs build -t openrealm_simulator
bbs build -t openrealm_relay
```

### Native dependencies
- `raylib`
- `tracy`
- `cpp-httplib`
- `nlohmann/json`

The current relay scaffold uses:
- ENet for binary node-to-node packets
- `cpp-httplib` + `nlohmann/json` only for blockchain JSON-RPC

These are declared in `project.bbs`.

## Blockchain workspace

From `blockchain/`:

```bash
npm install
npm run build
npm test
```

For a local deployment against Ganache:

```bash
npx ganache --wallet.mnemonic 'test test test test test test test test test test test junk' --wallet.totalAccounts 10 --chain.chainId 31337 --server.port 8545
node realms/test/deploy.js --private-key <private-key> --owner <owner-address>
```

For a main/real deployment using the realm-specific wrapper:

```bash
node realms/main/deploy.js --rpc <production-rpc-url> --private-key <private-key> --owner <owner-address>
```

See [`blockchain/README.md`](blockchain/README.md) for the full contract workflow.

## Formatting

### C++
The native codebase follows the repository's `.clang-format`:
- 2-space indentation
- Allman braces
- no include sorting
- pointer style `Type* name`

### Solidity
**No, `clang-format` is not the right formatter for Solidity.**

For Solidity we should use a Solidity-aware formatter instead, such as:
- `forge fmt`
- `prettier` with `prettier-plugin-solidity`

I have **not** wired a Solidity formatter into the repo in this change, but that would be the right next step if you want one.

## Long-term direction

The target architecture is roughly:
1. keep strengthening the local client/runtime split
2. introduce headless simulation/node behavior
3. add peer discovery and jump-node flow
4. add region-of-interest networking and chunk authority
5. connect runtime permissions to blockchain-backed ownership and identity

## Status

Current implementation is strongest in:
- local voxel engine/client foundations
- local meshing/rendering architecture
- initial NFT-backed orchestration-layer contracts and tests

Still future work:
- decentralized runtime networking
- peer discovery / relay behavior
- authoritative chunk simulation across multiple nodes
- real gameplay integration with wallet/ownership flows
