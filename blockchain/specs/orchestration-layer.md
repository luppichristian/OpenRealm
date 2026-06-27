# OpenRealm Orchestration Layer Spec

This document describes the current concrete orchestration-layer implementation under `blockchain/`.

## Contracts

### `PlayerRegistry`
Tracks wallet-linked player registration.

Responsibilities:
- Register a handle and metadata URI
- enforce one active registration per wallet
- enforce unique active handles
- allow handle updates
- allow unregistering while preserving historical player id
- expose handle-hash-friendly events for indexers
- authorize expiring runtime-session keys for off-chain/runtime message signing
- resolve an arbitrary actor address into either a registered wallet account or an authorized runtime session owner

Current assumptions:
- one wallet maps to one active player identity
- handle uniqueness is enforced by `keccak256(bytes(handle))`
- registration state is the on-chain gate used by the chunk-claim layer
- runtime-session keys are ephemeral delegations intended for gameplay/runtime signing so the wallet itself does not need to sign every action

Runtime-facing surfaces:
- `PlayerIdOf(account)`
- `ResolveRuntimeAccount(actor)`
- `GetRuntimeSession(session)`

### `ChunkClaims`
Tracks NFT-backed ownership of chunk coordinates.

Responsibilities:
- enforce coordinate bounds `[-30000, 30000]`
- allow only registered players to claim chunks
- mint one ERC721-like ownership token per claimed chunk
- map chunk coordinate -> token id -> owner
- allow direct owner transfer and voluntary abandonment
- allow owner-managed delegated editors
- invalidate delegated-editor state on ownership transfer
- expose a marketplace transfer hook for official sales
- expose runtime-facing permission/state reads that resolve wallet owners, delegated editors, and authorized runtime-session keys

Current assumptions:
- ownership is NFT-backed through the local `ERC721Lite` utility, not plain storage
- ownership does not imply runtime simulation authority
- delegated editors are runtime-facing permission hints that can be read by off-chain systems
- rich token metadata hosting is deferred

Runtime-facing surfaces:
- `CanEdit(x, y, account)`
- `CanEditWithRuntimeSigner(x, y, actor)`
- `EditorEpochOfChunk(x, y)`
- `GetChunkRuntimeState(x, y, actor)`
- `GetClaim(x, y)` / `GetClaimByTokenId(tokenId)`

`GetChunkRuntimeState(...)` is intended to be the primary one-call read for the next runtime layer. It returns:
- whether the chunk is claimed
- token id / coordinates / claim timestamp
- current owner address and stable `ownerPlayerId`
- current permission/editor epoch for cache invalidation
- resolved actor account for the supplied signer
- whether the supplied signer is registered / can edit / is acting through a runtime-session delegation

### `Marketplace`
Runs the official chunk sale flow.

Responsibilities:
- create fixed-price listings
- create English auctions with reserve price + minimum bid increment
- prevent simultaneous active sale modes for one chunk token
- accept buyer payments and bids from registered players only
- invalidate stale sales when ownership changes outside the marketplace
- transfer ownership through `ChunkClaims`
- retain a protocol fee in basis points
- allow owner withdrawal of accumulated fees
- emit chunk-key/token-id-friendly events for indexers
- expose unified sale-state reads for runtime/UI consumption

Current assumptions:
- fixed-price and auction sales are both first-party marketplace flows
- stale sales must be explicitly invalidated if ownership changes outside the marketplace
- fee capture remains marketplace-mediated rather than embedded in the NFT transfer primitive itself

Runtime-facing surfaces:
- `GetSaleStateForChunk(x, y)`
- `GetSaleStateForToken(tokenId)`

These sale-state views are intended for runtime/UI code that wants a single read describing whether a chunk currently has:
- no active sale
- an active listing
- an active auction

along with seller ownership freshness and current pricing/bid fields.

## Tooling

The current local toolchain is intentionally lightweight:
- `solc` for compilation
- `ganache` for ephemeral local EVM execution during tests and local deployment
- `ethers` for deployment and interaction in tests/scripts
- `mocha` as the test runner

## Build / Test / Deploy

Run locally with:

```bash
cd blockchain
npm install
npm run build
npm test
```

For local deployment:

```bash
npx ganache --wallet.totalAccounts 10 --chain.chainId 31337
npm run deploy:local -- --private-key YOUR_LOCAL_PRIVATE_KEY --owner YOUR_OWNER_ADDRESS
```

For non-local deployment:

```bash
RPC_URL=https://your-rpc.example \
PRIVATE_KEY=0xyourprivatekey \
OWNER_ADDRESS=0xyourowner \
FEE_BPS=500 \
npm run deploy -- --network sepolia
```

Deployment writes a record to `deployments/<network>.json`.

## Main Flows Covered By Tests

1. player registration, handle uniqueness, handle release on Unregister
2. runtime-session authorization and runtime-facing chunk permission resolution
3. NFT-backed chunk claims and registered-only transfers
4. fixed-price marketplace purchase + fee retention + sale-state query
5. English auction bidding, sale-state reads, and settlement
6. stale listing invalidation after direct token transfer
7. rejection of unregistered buyers

## Deferred Items

Not implemented yet:
- wallet rotation / recovery flows
- governance and upgrade control patterns
- third-party marketplace/royalty standards
- hosted NFT metadata and richer token URI generation
- indexer/subgraph project files
- runtime message envelope standards beyond on-chain runtime-session authorization
