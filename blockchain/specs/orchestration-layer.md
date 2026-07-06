# OpenRealm Orchestration Layer Spec

This document describes the current concrete orchestration-layer implementation under `blockchain/`.

## Contracts

### `GlobalParams`
Holds the shared on-chain tuning values that the runtime/UI can fetch later.

Responsibilities:
- expose the global chunk coordinate bounds
- expose the minimum purchase price for a free chunk
- expose marketplace-wide validation limits such as max fee bps and minimum auction duration

Current parameters:
- `MIN_CHUNK_COORD`
- `MAX_CHUNK_COORD`
- `MIN_CHUNK_PRICE`
- `MAX_FEE_BPS`
- `MIN_AUCTION_DURATION`

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
- enforce coordinate bounds through `GlobalParams`
- allow only registered players to buy/claim free chunks
- require the exact `MIN_CHUNK_PRICE` payment when claiming a free chunk
- mint one ERC721-like ownership token per claimed chunk
- map chunk coordinate -> token id -> owner
- allow direct owner transfer and voluntary abandonment with refund of the locked minimum chunk purchase price
- allow owner-managed delegated editors
- invalidate delegated-editor state on ownership transfer
- expose a marketplace transfer hook for official sales
- expose runtime-facing permission/state reads that resolve wallet owners, delegated editors, and authorized runtime-session keys

Current assumptions:
- ownership is NFT-backed through the local `ERC721Lite` utility, not plain storage
- every newly claimed free chunk costs `MIN_CHUNK_PRICE`, which is kept in the contract and refunded if the owner later abandons the chunk back to the free pool
- ownership does not imply runtime simulation authority
- delegated editors are runtime-facing permission hints that can be read by off-chain systems
- rich token metadata hosting is deferred

Runtime-facing surfaces:
- `CanEdit(x, y, account)`
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
- create fixed-price listings at or above `MIN_CHUNK_PRICE`
- create English auctions with reserve price + minimum bid increment, where the reserve is at or above `MIN_CHUNK_PRICE`
- prevent simultaneous active sale modes for one chunk token
- accept buyer payments and bids from registered players only
- invalidate stale sales when ownership changes outside the marketplace
- transfer ownership through `ChunkClaims`
- retain a protocol fee in basis points, capped by `GlobalParams.MAX_FEE_BPS`
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
4. minimum-priced free chunk claims, abandonment refunds, and reclaiming a freed chunk
5. fixed-price marketplace purchase + fee retention + sale-state query
6. English auction bidding, sale-state reads, and settlement
7. stale listing invalidation after direct token transfer
8. rejection of unregistered buyers

## Deferred Items

Not implemented yet:
- wallet rotation / recovery flows
- governance and upgrade control patterns
- third-party marketplace/royalty standards
- hosted NFT metadata and richer token URI generation
- indexer/subgraph project files
- runtime message envelope standards beyond on-chain runtime-session authorization
