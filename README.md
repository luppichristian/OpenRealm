# OpenRealm

[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-blue)](https://github.com/luppichristian/OpenRealm)
[![Language](https://img.shields.io/badge/native-C%2B%2B20-00599C)](https://github.com/luppichristian/OpenRealm)
[![Contracts](https://img.shields.io/badge/contracts-Solidity-363636)](https://github.com/luppichristian/OpenRealm/tree/master/blockchain)
[![Build](https://img.shields.io/badge/build-bbs-orange)](https://github.com/luppichristian/bbs)
[![Runtime Tests](https://img.shields.io/github/actions/workflow/status/luppichristian/OpenRealm/runtime-realm-tester.yml?branch=master&label=runtime%20tests)](https://github.com/luppichristian/OpenRealm/actions/workflows/runtime-realm-tester.yml)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

OpenRealm is a local-first voxel world stack that combines a native client, runtime node network, and Ethereum-compatible orchestration layer for identity, ownership, and marketplace state.

It is built for worlds that want fast native simulation, explicit ownership rules, and a cleaner boundary between live gameplay and durable economic state.

```mermaid
flowchart LR
  C[Native client] --> R[Runtime nodes\nrelay / simulator]
  R --> W[World simulation]
  C --> B[Blockchain wrappers]
  B --> S[Solidity orchestration\nidentity / ownership / marketplace]
```

## What OpenRealm includes

- A native C++20 voxel client with rendering, input, audio, menus, and local world interaction
- Headless relay and simulator targets for runtime networking and world execution
- ENet-based runtime sessions for bootstrap, topology exchange, and player snapshot replication
- Solidity contracts for player identity, chunk claims, delegated editing, listings, auctions, and protocol parameters
- JavaScript tooling for contract build, test, and deployment flows
- Realm-specific wrappers for local smoke deployment and main-network deployment

## Architecture

OpenRealm is organized around three layers:

- Client layer
  - windowed gameplay, rendering, menus, audio, and player-facing UX
- Runtime layer
  - relay/simulator processes, node discovery, session bootstrap, topology exchange, and live replication
- Orchestration layer
  - player identity, chunk ownership, delegated editing permissions, listings, auctions, and deployment records

This split keeps simulation close to the game while moving slower, durable world rules into a separate orchestration surface.

## Quick start

### Native build

OpenRealm uses `bbs` for native builds.

```bash
bbs info project
bbs build -t openrealm_client
bbs build -t openrealm_node_launcher
```

### Orchestration workspace

The orchestration workspace lives under `blockchain/`.

```bash
cd blockchain
npm install
npm run verify
```

That command rebuilds the contract artifacts and runs the current Mocha suite.

## Realms and deployment

OpenRealm currently ships with two realm configurations:
- `realms/test`
- `realms/main`

`realms/main` is the canonical example for a real deployment and the official main realm layout.
`realms/test` exists for local Ganache-backed testing and smoke deployment only.

If you later create your own custom realm, use the same file shapes as `realms/main`:
- a `realm.json` with the real RPC URL and deployed contract addresses
- a `jump_nodes.json` with real reachable bootstrap relays
- node `config.json` files per machine or role, configured for that node's wallet, bind address, and runtime behavior

Useful commands:

```bash
cd blockchain
npm run ganache:test
npm run deploy:test:local
npm run deploy:main
```

## Native targets

The native repo currently exposes four main targets:

- `openrealm_client`
  - playable windowed client
- `openrealm_relay`
  - headless relay/runtime node
- `openrealm_simulator`
  - headless simulation/runtime node
- `openrealm_node_launcher`
  - local multi-process launcher for relay/simulator sessions

## Development notes

- Native build definitions live in `project.bbs`.
- The orchestration workspace has CI and deployment workflows in `.github/workflows/`.
- Root `AGENTS.md` and `CLAUDE.md` point to the internal `.agents/` repo guide used for agent-facing maintenance notes.

## License

OpenRealm is licensed under the MIT License. See `LICENSE`.
