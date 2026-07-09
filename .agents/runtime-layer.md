# Runtime Layer

## Scope

The runtime layer is the native ENet-based node-to-node networking side of OpenRealm.

It currently owns:
- join/bootstrap flow through jump nodes
- topology snapshots and known-peer tracking
- locality-biased connection refresh
- player snapshot replication
- runtime-side realm compatibility checks
- client/simulator/relay runtime session startup

It does not own:
- on-chain ownership truth
- marketplace/economy logic
- contract deployment
- client menu UX

## Verified implementation surface

Primary files:
- `node/runtime/RuntimeSession.*`
- `node/runtime/RuntimeClient.*`
- `node/runtime/Packet.*`
- `node/runtime/PacketValidator.*`
- `node/runtime/NodeConfigFiles.*`
- `node/runtime/ActiveNodeBucket.*`
- `node/runtime/ChunkInterestBucket.*`
- `node/runtime/RuntimeRealm.*`
- `node/runtime/ProtocolVersion.h`

Current source count in the repo:
- `node/`: 41 `.cpp` files and 48 `.h` files total
- runtime packet/version core is concentrated in `node/runtime/`

## Protocol/versioning

Current hardcoded versions:
- runtime protocol version: `2`
- packet header/wire version: `2`

These come from `node/runtime/ProtocolVersion.h`.

## Packet model

Current packet types from `Packet.h`:
- `Handshake`
- `JoinRequest`
- `JoinResponse`
- `TopologySnapshot`
- `PlayerSnapshot`

Current packet payloads carry:
- node id
- realm hash
- world position
- interest area
- node role / accepts-joins state
- join target/candidate info
- player position + yaw/pitch activity snapshot

The runtime is real, but still narrow. Do not describe it as a complete replicated-world or consensus protocol.

## Node roles and current behavior

### Client
Current client behavior is implemented in `node/client/Game.cpp`:
- starts a local `World`
- loads client config and selected realm
- attempts to start a `RuntimeSession` with role `Client`
- falls back to local-only play if the runtime session fails to start
- applies a resolved runtime spawn later if join resolution succeeds after startup

Important implication:
- local gameplay is still the baseline
- runtime participation is additive, not a hard requirement for the client loop to exist

### Simulator
Current simulator behavior from `node/targets/Simulator.cpp`:
- loads `config.json` through `NodeConfigFiles`
- loads realm files through `RealmConfigFiles`
- starts a runtime session with role `Simulator`
- initializes headless `World`
- runs world update + runtime tick loop until frame budget expires (or forever when `frames == 0`)

### Relay
Current relay behavior from `node/targets/Relay.cpp`:
- loads `config.json` + realm files
- starts a runtime session with role `Relay`
- runs runtime tick loop without a world pointer
- logs periodic status lines

### Jump nodes
Jump nodes are read from `realms/<name>/jump_nodes.json`.

They are:
- bootstrap/discovery addresses
- candidates for initial join/bootstrap contact

They are not:
- authoritative servers
- spawn points
- permanent owners of simulation state

## Join and topology model

Current `RuntimeSession` behavior includes:
- `joinResolved` becomes true immediately when runtime is disabled or when no valid jump node is configured
- otherwise the node periodically issues join requests toward the configured jump node
- topology refresh and topology broadcast are periodic timers
- player snapshot broadcast is periodic when a `World*` is present
- remote players are spawned/despawned into the headless/playable world via `WorldEvent`

This is an explicit-position join flow, not generic random matchmaking.

## Realm compatibility / hashing

Current launch paths build a runtime realm hash from:
- runtime protocol version
- blockchain config loaded from the selected realm
- a chain-id surrogate

Important nuance:
- file-launched client/simulator/relay code currently uses the realm directory string as the chain-id surrogate when constructing the local runtime hash
- `BuildRuntimeRealmState(...)` in `RealmConfigFiles.cpp` can also build a richer state using live RPC `eth_chainId` plus fetched `GlobalParams`

So the repo contains both:
- a file/config-based current startup path
- a more blockchain-aware helper path for deeper integration

## Config schema that the runtime actually reads

### Canonical shared root config fields
`config.json` is target-agnostic and flat at the root. Do not reintroduce top-level target/group wrappers such as `wallet`, `client`, `runtime`, `service`, or `simulation`.

Identity rule: do not persist a manual `nodeId` in `config.json`. Runtime identity is derived from the live bind address at startup, so peer identity is not operator-configurable.

Shared root fields:
- `accountAddress`
- `bindAddress.{host,port}`
- `enabled`
- `acceptsJoins`
- `position.{x,y,z}`
- `areaOfInterest.{radiusX,radiusY,radiusZ}`
- `join.maxCandidates`
- `join.maxHops`
- `limits.maxNodeConnections`
- `limits.maxKnownNodes`
- `periodsMs.{neighborRefresh,topologyBroadcast,playerBroadcast}`
- `receiveTimeoutMs`
- `masterVolume`
- `mouseSensitivity`
- `invertMouseY`
- `showFps`
- `frames`
- `frameTime`
- `sleepMs`
- `ticks`

Launch-time selections do not belong in `config.json`:
- realm selection is chosen at runtime (`--realm-dir` for headless targets, client menu selection for the playable client)
- jump-node selection is chosen at runtime (`--jump-node-index` for headless targets, client menu selection for the playable client)
- join target selection is chosen at runtime by the client menu and is not persisted in `config.json`

Clamp/default behavior is implemented in `NodeConfigFiles.cpp` and `NodeConfigFiles.h`.

Comments are now accepted in file-based JSON configs loaded by the native repo (`config.json`, realm files, launcher-generated variants) because the loaders parse with comment ignoring enabled. Treat these files as JSON-with-comments for hand editing, but remember any code path that rewrites them via `json.dump(...)` will drop the comments.

### Secondary launcher/CLI fields
There is still a second config surface used by launcher/CLI-oriented code:
- `interest.chunkX`
- `interest.chunkY`
- optional `interest.radius`
- `emitPlaceEvent`

Where this appears today:
- `node_launcher/Config.cpp` writes `interest.chunkX/chunkY` for simulator-generated configs
- `node/cli/NodeCli.cpp` still reflects the older launcher-oriented config workflow

But:
- `NodeConfigFiles.cpp` does not read `interest.*`
- the canonical runtime loader still reads `areaOfInterest.*`
- `node/cli/` is not compiled into the current targets, so treat it as non-authoritative when it disagrees with the live target code

Treat this as current repo reality and potential drift, not as a fully unified config design.

## Runtime/world integration

Current world-facing runtime integration includes:
- remote player spawn/despawn through `WorldEventType::Spawn` / `Despawn`
- remote player movement/look updates through player snapshots
- client-side runtime spawn correction after join resolution
- headless simulator ticking with a real `World*`

Current runtime does not yet prove:
- full voxel/chunk replication between nodes
- durable distributed world ownership handoff
- consensus
- robust permission-enforced build editing over the network

## Runtime authority expectations

What is true today:
- the runtime carries topology, player position, and join-state machinery
- blockchain-aware permission helpers exist in the repo
- the world remains volatile/local-first

What should not be claimed yet:
- complete decentralized gameplay authority
- complete ownership-aware edit replication
- production-ready persistence/continuity guarantees

## Files to inspect first for runtime work

- `node/client/Game.cpp`
- `node/targets/Simulator.cpp`
- `node/targets/Relay.cpp`
- `node/runtime/RuntimeSession.cpp`
- `node/runtime/Packet.h`
- `node/runtime/NodeConfigFiles.cpp`
- `node_launcher/Config.cpp`
- `node/cli/NodeCli.cpp`
