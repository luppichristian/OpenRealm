# Project Design

## Project summary

OpenRealm is a C++20 voxel game prototype exploring a future decentralized voxel world without a single always-authoritative gameplay server.

The repo is split into three real concerns today:
- local client layer
  - rendering, input, menus, audio, and player-facing UX
- runtime layer
  - ENet-based node graph, joining, topology exchange, and player snapshot replication
- orchestration layer
  - Ethereum-compatible smart contracts for identity, ownership, and marketplace state

## Current reality vs future direction

What is concretely implemented today:
- playable local voxel client loop
- headless world simulation
- chunk storage, collision, raycast, and local voxel editing
- asynchronous chunk meshing with shared task manager support
- runtime session startup for client/simulator/relay roles
- topology/join/player-snapshot runtime packets
- Solidity ownership/marketplace contracts with build/test/deploy scripts
- native-side blockchain wrapper layer for RPC and contract reads

What is still future-facing:
- full voxel/chunk replication across many runtime nodes
- durable distributed continuity for world state
- mature ownership-aware remote editing over the runtime
- authority handoff across many simulation nodes
- consensus or stronger conflict-resolution machinery

## Local-first bias

The codebase is still intentionally local-first.

Verified current behavior from `node/client/Game.cpp`:
- the client starts a local `World`
- it attempts to layer a runtime session on top
- if runtime startup fails, gameplay continues in local-only mode

Do not describe OpenRealm as if network/runtime availability is already mandatory for basic play.

## Separation of concerns

### Local client layer owns
- window lifecycle
- rendering
- audio
- menus/options
- input translation
- client-only config persistence
- render-side world presentation/cache state

### Runtime layer owns
- jump-node bootstrap
- join requests/responses
- topology snapshots
- known-peer tracking
- player snapshot replication
- runtime realm compatibility checks

### Orchestration layer owns
- player registration / handles
- runtime-session authorization primitive
- chunk ownership claims
- delegated editor permissions
- listings, auctions, and protocol fees

## Design principles to preserve

- Local playability comes first.
- World simulation should remain usable headlessly.
- Rendering/client state should stay outside the headless world core.
- Blockchain usage should stay focused on low-frequency ownership/economy facts.
- Runtime networking should stay separate from render/UI concerns.
- The repo should not silently drift into a conventional central authoritative server design unless that direction is explicit.

## World continuity model

Current repo philosophy is volatile-first, not persistence-first:
- ownership metadata can outlive active simulation through the orchestration layer
- world voxel state is still primarily runtime/local memory state
- continuity depends on active nodes, not a finished persistence layer

## Ownership and authority philosophy

The intended split remains:
- ownership is blockchain-backed
- simulation authority is runtime/locality-driven
- ownership should not automatically imply live simulation authority

That philosophy is visible in the repo, but not fully enforced end-to-end yet.

## Security / correctness concerns to remember

Still-important risk areas:
- fake runtime actions
- stale or replayed state
- bad peer advertisements
- network spam/flooding
- config/schema drift between launcher/runtime/CLI surfaces

When documenting or extending the repo, be explicit about which pieces are already enforced in code versus still directional.
