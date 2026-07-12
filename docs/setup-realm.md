# Set up a real OpenRealm realm

This guide explains how to deploy the official `realms/main` OpenRealm realm on a real Ethereum-compatible blockchain and then connect real OpenRealm runtime nodes to it.

Treat `realms/main` as the canonical example for a real deployment.
If you later create your own custom realm or custom node setup, you still need the same kinds of files and values, just configured differently for your own network.

This is the path for a real deployment:
- a real RPC endpoint
- a real funded deployer wallet
- real contract addresses
- a real realm config
- at least one real reachable relay node

This guide does not use Ganache or the local `realms/test` helpers.

## What you are deploying

An OpenRealm realm has two parts:

1. The orchestration layer on a blockchain
   - `PlayerRegistry`
   - `GlobalParams`
   - `ChunkClaims`
   - `Marketplace`

2. The runtime layer off-chain
   - relay nodes
   - simulator nodes
   - clients
   - a realm directory with `realm.json` and `jump_nodes.json`

The blockchain stores durable state such as identity, ownership, permissions, listings, and protocol parameters.
The runtime nodes handle live simulation and networking.

A realm is only ready for real use when both layers are configured and point to each other correctly.

## Before you start

You need all of the following:

- Node.js and npm installed
- `bbs` installed for the native binaries
- the OpenRealm repository checked out
- dependencies installed in `blockchain/`
- a real EVM-compatible RPC URL
- a wallet private key for deployment
- enough native gas token in that wallet to pay deployment gas
- at least one machine that can host a relay node and accept inbound traffic

From the repository root:

```bash
cd blockchain
npm install
npm run verify
cd ..
```

`npm run verify` rebuilds the contract artifacts and runs the current test suite before you deploy.

## Important files

You will work with these files:

- `realms/main/realm.json`
- `realms/main/jump_nodes.json`
- `config.json`
- `blockchain/deployments/main.json`

For the official main realm, those exact `realms/main/*` files are the real example to follow.
If you later build a custom realm, you must still provide the same kinds of files:
- one `realm.json` with the real RPC URL and deployed contract addresses
- one `jump_nodes.json` with real reachable bootstrap relays
- one node `config.json` per machine or role, configured for that node's wallet, bind address, and runtime behavior

Important behavior to understand:
- `realms/main/deploy.js` is only a thin wrapper around the generic deployment flow.
- `realms/main/realm.json` ships with placeholder values.
- the deployment script writes the deployed addresses to `blockchain/deployments/main.json`
- the deployment script does not automatically rewrite `realms/main/realm.json`

That means you must copy the deployed addresses into `realms/main/realm.json` yourself after deployment.

## Step 1: choose the real chain you will use

Pick the real Ethereum-compatible network that will host the realm.

You need:
- an RPC URL for that network
- the chain's native gas token
- confidence that you want durable player identity, chunk ownership, and marketplace data on that chain

For a first rehearsal, use a public testnet with real RPC infrastructure.
For actual player use, repeat the same process on the live network you want to support.

## Step 2: prepare a funded deployer wallet

Create or choose a wallet that will deploy the contracts.

This wallet must:
- control the private key you will pass to the deploy script
- hold enough gas to deploy 4 contracts and link them
- be trusted, because by default the deployed owner is the deployer wallet unless you override it with `--owner`

Keep the private key secret.
Do not commit it to the repository.
Do not write it into `realm.json`.

## Step 3: point the main realm at the real RPC

Open `realms/main/realm.json` and replace the placeholder RPC URL.

Current placeholder:

```json
"rpcUrl": "https://rpc.example.invalid"
```

Change it to your real endpoint.
Example:

```json
"rpcUrl": "https://your-real-rpc.example"
```

Do not fill the contract addresses yet if you have not deployed the contracts.
Leave them as placeholders until the deployment step finishes.

## Step 4: deploy the contracts to the real chain

From the repository root:

```bash
cd blockchain
npm run deploy:main -- --private-key YOUR_PRIVATE_KEY
```

What this does:
- runs `realms/main/deploy.js`
- forwards into `blockchain/scripts/deployRealm.js`
- loads `realms/main/realm.json`
- takes the RPC URL from that realm config if you did not pass `--rpc`
- deploys the 4 contracts
- links `ChunkClaims` to `Marketplace`
- writes a deployment record to `blockchain/deployments/main.json`

Useful optional flags:

```bash
npm run deploy:main -- \
  --private-key YOUR_PRIVATE_KEY \
  --owner YOUR_OWNER_ADDRESS \
  --fee-bps 500 \
  --min-chunk-price-wei 10000000000000000 \
  --min-chunk-coord -30000 \
  --max-chunk-coord 30000 \
  --min-auction-duration-seconds 60
```

What the important options mean:
- `--owner`
  - owner/admin address for owner-controlled contract actions
- `--fee-bps`
  - marketplace protocol fee in basis points
  - `500` means 5%
- `--min-chunk-price-wei`
  - minimum chunk claim price in wei
- `--min-chunk-coord` and `--max-chunk-coord`
  - world claim bounds enforced on-chain
- `--min-auction-duration-seconds`
  - minimum allowed auction duration

If you prefer environment variables instead of long command lines, the deployer also understands:
- `RPC_URL`
- `PRIVATE_KEY`
- `OWNER_ADDRESS`
- `DEPLOY_NETWORK`
- `MIN_CHUNK_COORD`
- `MAX_CHUNK_COORD`
- `MIN_CHUNK_PRICE_WEI`
- `MAX_FEE_BPS`
- `MIN_AUCTION_DURATION_SECONDS`
- `FEE_BPS`

## Step 5: save the deployed addresses into the realm config

After a successful deployment, open:

```text
blockchain/deployments/main.json
```

That file contains the deployed contract addresses and the chain id.

Copy the contract addresses into `realms/main/realm.json`:
- `globalParamsAddress`
- `playerRegistryAddress`
- `chunkClaimsAddress`
- `marketplaceAddress`

After editing, `realms/main/realm.json` should look like this shape:

```json
{
  "name": "main",
  "blockchain": {
    "protocolVersion": 1,
    "rpcUrl": "https://your-real-rpc.example",
    "globalParamsAddress": "0x...",
    "playerRegistryAddress": "0x...",
    "chunkClaimsAddress": "0x...",
    "marketplaceAddress": "0x...",
    "connectionTimeoutSeconds": 2,
    "readTimeoutSeconds": 2,
    "writeTimeoutSeconds": 2
  }
}
```

This step is required.
If you skip it, OpenRealm nodes will still point at zero addresses and the realm will not be ready for real use.

## Step 6: configure the jump nodes players will use

Open `realms/main/jump_nodes.json`.

This file tells clients and nodes where to bootstrap into the runtime network.
It must contain real reachable relay addresses, not just local placeholders.

A real example shape:

```json
{
  "jumpNodes": [
    {
      "host": "relay1.yourdomain.com",
      "port": 46001,
      "label": "main-relay-1"
    }
  ]
}
```

For real use:
- `host` should be a public IP or DNS name that players can reach
- `port` should match the relay's actual UDP bind port
- your firewall, cloud security group, router, and hosting provider must all allow that traffic

If you want redundancy, add more than one jump node.

## Step 7: configure the runtime wallet and node settings

Open `config.json`.

This file controls the runtime node you launch locally on a machine.
The current code supports a flat root config and also reads wallet data.

For a real node, make sure these values are correct:
- `accountAddress` at the root of `config.json`, or `wallet.accountAddress` if you are using the nested wallet form
- `enabled`
- `acceptsJoins`
- `bindAddress.host`
- `bindAddress.port`

Minimum practical requirements for a public relay:
- `enabled` must be `true`
- `acceptsJoins` should be `true`
- `bindAddress.port` should be the public relay UDP port
- the configured account should be the wallet identity you want this node to present

If the machine is supposed to accept remote peers, the bind host and surrounding network setup must allow that.
`127.0.0.1` is only suitable for local-only testing.

## Step 8: build the native binaries

From the repository root:

```bash
bbs build -t openrealm_relay
bbs build -t openrealm_simulator
bbs build -t openrealm_client
bbs build -t openrealm_realm_tester
```

You do not need the node launcher for a real deployment.
The launcher is mainly for local multi-process testing.
For a real realm, you usually run the relay and simulator deliberately as separate managed processes.

## Step 9: start the first real relay node

From the repository root:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-relay.exe \
  --config config.json \
  --realm-dir realms/main \
  --jump-node-index 0
```

What this does:
- loads your node config
- loads `realms/main/realm.json`
- loads `realms/main/jump_nodes.json`
- starts a relay runtime session
- uses the blockchain-backed realm data when building the runtime realm hash

Before you launch this relay, make sure `realms/main/jump_nodes.json` already advertises the public host and UDP port that other nodes and players should use to reach your realm.
What matters is that the addresses in `jump_nodes.json` are coherent and reachable by the rest of your nodes and players.

## Step 10: start a simulator node for the world

From the repository root:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-simulator.exe \
  --config config.json \
  --realm-dir realms/main \
  --jump-node-index 0
```

The simulator is the world-owning headless process.
A realm is not useful to real players if you only deploy the contracts but never run the runtime world.

## Step 11: verify the realm before inviting players

Use the realm tester against the real realm:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-realm-tester.exe \
  --config config.json \
  --realm-dir realms/main \
  --jump-node-index 0 \
  --bind-port 46301 \
  --run-seconds 10 \
  --expect-topology-nodes 1
```

A passing run should report:
- `result: passed`
- at least one authenticated peer
- a topology snapshot
- a join response

If this does not pass, the realm is not ready for real users yet.
Fix the issue before sharing the realm.

## Step 12: test a real client connection

Start the client:

```bash
./build/default-windows-x86_64/bin/Debug/openrealm-client.exe
```

Then:
1. open Play
2. choose the `main` realm
3. choose a jump node
4. choose the join target
5. confirm the client can connect to the running network

This is the final beginner-friendly proof that the full stack is alive:
- blockchain config resolves
- runtime nodes are reachable
- the client can join the realm

## Recommended first production checklist

Before you call the realm live, confirm all of this:

- `npm run verify` passes in `blockchain/`
- contracts were deployed to the chain you intended
- `blockchain/deployments/main.json` exists and matches the live deployment
- `realms/main/realm.json` contains the real RPC URL and real contract addresses
- `realms/main/jump_nodes.json` contains real reachable relay addresses
- the relay process is reachable from another machine
- the simulator process is running
- `openrealm-realm-tester` passes
- the client can join the realm
- the deployer key is stored safely outside the repo

## Common mistakes

### Leaving placeholder addresses in `realms/main/realm.json`

This is the most common failure.
The deploy script writes `blockchain/deployments/main.json`, but it does not automatically copy those values into the realm file.

### Using `127.0.0.1` in `jump_nodes.json`

That only works for local testing on the same machine.
Real users need a public host or reachable private network address.

### Forgetting gas funds

A real deployment cannot succeed unless the deployer wallet has enough gas for all contract deployments and the linking transaction.

### Deploying contracts but not running the runtime

The blockchain alone is not the whole realm.
Players still need relay and simulator nodes.

### Using the local test scripts by mistake

For a real realm, do not use:
- `npm run ganache:test`
- `npm run deploy:test:local`
- `realms/test/*`

Those are for local development.
For a real realm, use `realms/main/*` and your real chain.

## Minimal command summary

From the repository root:

```bash
cd blockchain
npm install
npm run verify
npm run deploy:main -- --private-key YOUR_PRIVATE_KEY
cd ..

bbs build -t openrealm_relay
bbs build -t openrealm_simulator
bbs build -t openrealm_client
bbs build -t openrealm_realm_tester

./build/default-windows-x86_64/bin/Debug/openrealm-relay.exe --config config.json --realm-dir realms/main --jump-node-index 0
./build/default-windows-x86_64/bin/Debug/openrealm-simulator.exe --config config.json --realm-dir realms/main --jump-node-index 0
./build/default-windows-x86_64/bin/Debug/openrealm-realm-tester.exe --config config.json --realm-dir realms/main --jump-node-index 0 --bind-port 46301 --run-seconds 10 --expect-topology-nodes 1
```

## Final note

If you want a real OpenRealm realm that is ready for real use, think in this order:

1. deploy contracts to the real chain
2. copy the deployed addresses into `realms/main/realm.json`
3. publish real jump-node addresses in `realms/main/jump_nodes.json`
4. run real relay and simulator nodes
5. verify with `openrealm-realm-tester`
6. join with the real client

If any one of those steps is missing, the realm is not fully deployed yet.
