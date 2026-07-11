# Realm tester

Target mapping:
- build target id: `openrealm_realm_tester`
- compiled binary name: `openrealm-realm-tester`

## What it does

The realm tester is a headless runtime-layer probe that joins an OpenRealm realm as a signed runtime peer and verifies that the network exchanges valid runtime packets.

It currently verifies:
- runtime wallet auth can resolve a signer address from the configured wallet
- the realm blockchain endpoint is reachable and can contribute to the runtime realm hash when `realm.json` points at an RPC URL
- handshake packets are received and signature-validated
- challenge request and challenge response packets are received and signature-validated
- at least one authenticated peer is established
- a join response is received after the tester asks to join the realm
- a topology snapshot is received and contains at least the requested minimum number of nodes
- optional player snapshot receipt when `--require-player-snapshot` is enabled

The tester also sends its own handshake, challenge request, challenge response, join request, and periodic player snapshot probe packets so the network is exercised instead of passively inspected.

## Interface

This is a CLI/headless utility.
- No graphical window
- Human-readable stdout status lines
- Exit code `0` on success
- Exit code `1` when any required packet or prerequisite check is missing

## Command line arguments

Optional:
- `--config <path>`
  - Node config containing the runtime wallet used to sign packets
  - Default: `config.json`
- `--realm-dir <path>`
  - Realm directory to test
  - Default: `realms/test`
- `--jump-node-index <index>`
  - Which jump node entry from `jump_nodes.json` to bootstrap against
  - Default: `0`
- `--bind-host <host>`
  - Local UDP bind host
  - Default: `127.0.0.1`
- `--bind-port <port>`
  - Local UDP bind port
  - Default: `46301`
- `--join-target-x <value>`
- `--join-target-y <value>`
- `--join-target-z <value>`
  - Join target position used in the tester's join request
  - Defaults: `0 0 0`
- `--run-seconds <seconds>`
  - Maximum probe duration before failing if expectations are still missing
  - Default: `8`
- `--expect-topology-nodes <n>`
  - Minimum number of nodes expected in at least one topology snapshot
  - Default: `1`
- `--require-player-snapshot`
  - Require at least one inbound player snapshot before succeeding
- `--help`
  - Prints usage text

## How to run it

From the repository root:

```bash
bbs build -t openrealm_realm_tester
./build/default-windows-x86_64/bin/Debug/openrealm-realm-tester.exe --realm-dir realms/test --bind-port 46301
```

Typical use is to run it against a realm already launched by `openrealm-node-launcher` or by manually started relay/simulator processes.

Example against a launcher-created local session realm:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-realm-tester.exe \
  --config config.json \
  --realm-dir build/node_launcher/<timestamp>/realm \
  --bind-port 46301 \
  --run-seconds 10 \
  --expect-topology-nodes 2
```

## Success output

A passing run prints:
- `result: passed`
- whether the blockchain-backed realm hash was built from a reachable chain
- the authenticated peer count
- the largest topology snapshot size observed
- booleans for observed handshake, challenge request, challenge response, join response, topology snapshot, and player snapshot packets

A failing run prints `result: failed` plus the missing expectation or the first packet validation failure.
