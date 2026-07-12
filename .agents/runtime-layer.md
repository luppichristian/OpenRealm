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
- runtime protocol version: `3`
- packet header/wire version: `3`

These come from `node/runtime/ProtocolVersion.h`.
The bump reflects incompatible runtime hardening in both topology/join semantics and packet framing.

## Packet model

Current packet types from `Packet.h`:
- `Handshake`
- `JoinRequest`
- `JoinResponse`
- `TopologySnapshot`
- `PlayerSnapshot`

Current packet payloads carry:
- sender session id + monotonic per-sender sequence metadata
- realm hash
- world position
- interest area
- node role / accepts-joins state
- join target/candidate info
- player position + yaw/pitch activity snapshot

The runtime is real, but still narrow. Do not describe it as a complete replicated-world or consensus protocol.

## Runtime security model

The runtime is now compatibility-checked and locally hardened, but it is still not a trustless network.

What is enforced in the current code:
- packet parsing requires runtime packet magic, wire version, payload-length consistency, supported packet type, and checksum match
- every packet family is validated semantically in `PacketValidator.cpp`, not just handshakes
- peers are tracked by runtime session id plus per-packet-family monotonic sequence windows in `ActiveNodeBucket`
- suspicious peers accumulate suspicion/strike state and can be quarantined or banned in-memory for the current session
- quarantined/banned peers are excluded from neighbor refresh, topology snapshots, join responses, and normal send paths
- topology gossip is treated as hints; third-party nodes from join/topology packets require direct handshake confirmation before they become active peers
- player snapshots are checked for impossible movement, excessive player-vs-node drift, and interest-area contradictions before they affect world state

What is still not enforced:
- cryptographic node identity
- signed packets or challenge-response handshakes
- Byzantine-safe consensus or authoritative anti-spoofing guarantees

Treat the current runtime as a hardened gossip/bootstrap layer with local containment, not as a cryptographically authenticated trustless protocol.

## Node roles and current behavior

### Client
Current client behavior is implemented in `node/client/Game.cpp`:
- starts a local `World`
- loads client config and selected realm
- accepts optional launcher-driven overrides for config path, realm directory, jump-node index, join target, and auto-play
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
- otherwise the node periodically issues join requests toward the configured jump node and any directly confirmed join-capable peers
- join requests use per-request entropy and join responses are only accepted inside a bounded request window
- topology refresh and topology broadcast are periodic timers
- player snapshot broadcast is periodic when a `World*` is present
- remote players are spawned/despawned into the headless/playable world via `WorldEvent`
- direct topology/join advertisements for third parties are stored as provenance-carrying hints until a direct handshake confirms the peer

This is an explicit-position join flow, not generic random matchmaking.

## Realm compatibility / hashing

Current launch paths build a runtime realm hash from:
- runtime protocol version
- blockchain config loaded from the selected realm
- a chain-id value that prefers the richer runtime realm state when RPC truth is available and otherwise falls back to file-based startup data

Important nuance:
- headless relay/simulator startup now tries the richer `BuildRuntimeRealmState(...)` path when RPC configuration is present, including live `eth_chainId` and fetched `GlobalParams`
- the native realm loader now rejects `realm.json` files whose blockchain `protocolVersion` does not match compiled `kBlockchainProtocolVersion`
- the realm tester now treats blockchain success as a consistency check, not just RPC reachability: it requires non-zero configured orchestration contract addresses, readable `GlobalParams`/`PlayerRegistry`, matching `ChunkClaims`/`Marketplace` cross-links, and `Marketplace.feeBps()` within `GlobalParams.MAX_FEE_BPS()`
- when RPC truth is unavailable at startup, the current fallback still uses the realm directory string as a chain-id surrogate so file-based local boot remains possible

So the repo now has a preferred richer startup path with a compatibility fallback, not two equally strong security boundaries.

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
