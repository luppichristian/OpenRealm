# Orchestration Layer

## Scope

The `blockchain/` workspace is OpenRealm's orchestration layer.

It intentionally handles low-frequency identity / ownership / economy facts while native runtime and world code keep moment-to-moment simulation local.

## Verified current workspace surface

Current Solidity file count:
- 9 `.sol` files total

Breakdown:
- deployable contracts
  - `PlayerRegistry.sol`
  - `GlobalParams.sol`
  - `ChunkClaims.sol`
  - `Marketplace.sol`
- interfaces
  - `interfaces/IPlayerRegistry.sol`
  - `interfaces/IGlobalParams.sol`
  - `interfaces/IChunkClaims.sol`
- utility contracts
  - `utils/Ownable.sol`
  - `utils/ERC721Lite.sol`

Current JS helper/test surface:
- `blockchain/scripts/compile.js`
- `blockchain/scripts/deploy.js`
- `blockchain/scripts/deployRealm.js`
- `blockchain/scripts/realmConfig.js`
- `blockchain/scripts/protocolVersion.js`
- `blockchain/test/orchestration.test.js`
- `blockchain/test/helpers/compileContracts.js`

## Contract responsibilities

### `GlobalParams`
Shared on-chain parameter source.

Current deployment/config values cover:
- min chunk coordinate
- max chunk coordinate
- min chunk claim price
- max protocol fee bps
- min auction duration

### `PlayerRegistry`
Identity layer.

Current verified behavior from code/tests:
- one active profile per wallet
- unique handles
- metadata URI updates
- handle changes
- unregister / re-register while preserving stable `playerId`
- runtime-session authorization with expiry
- runtime-session revocation
- resolving either a direct wallet or an authorized runtime session key

Important runtime-facing reads:
- `PlayerIdOf(...)`
- `IsRegistered(...)`
- `GetProfile(...)`
- `GetRuntimeSession(...)`
- `ResolveRuntimeAccount(...)`

### `ChunkClaims`
Chunk ownership layer.

Current verified behavior from code/tests:
- only registered players can claim
- claim requires exact payment of `MIN_CHUNK_PRICE`
- coordinate bounds enforced through `GlobalParams`
- each claim is an `ERC721Lite` token
- ownership transfers preserve token identity
- delegated editors exist per chunk
- delegated editor epoch increments on transfer / abandon so stale permissions die cheaply
- abandoning refunds the locked minimum chunk price and returns the chunk to the free pool
- runtime-facing read model exists through `GetChunkRuntimeState(...)`

Important reads:
- `OwnerOf(x, y)`
- `TokenIdOfChunk(x, y)`
- `CanEdit(x, y, account)`
- `EditorEpochOfChunk(x, y)`
- `GetClaim(...)`
- `GetClaimByTokenId(...)`
- `GetChunkRuntimeState(...)`
- `IsRegisteredPlayer(...)`

### `Marketplace`
Sale layer for claimed chunks.

Current verified behavior from code/tests:
- fixed-price listings
- English auctions
- seller ownership revalidation before purchase/settlement
- stale listing/auction invalidation after external ownership change
- protocol fee retention with owner withdrawal
- reserve/listing minimums enforced against `MIN_CHUNK_PRICE`
- max fee bps enforced against `MAX_FEE_BPS`

Important reads:
- `GetSaleStateForChunk(...)`
- `GetSaleStateForToken(...)`
- `listings(id)`
- `auctions(id)`

## Native integration layer

The native repo does not talk to Solidity directly from gameplay code.
It goes through `node/blockchain/` wrappers such as:
- `BlockchainRpcClient.*`
- `PlayerRegistryContract.*`
- `ChunkClaimsContract.*`
- `MarketplaceContract.*`
- `GlobalParamsContract.*`
- `Blockchain.*`

That wrapper layer already contains explicit selectors for runtime-facing calls such as:
- `ResolveRuntimeAccount(address)`
- `GetChunkRuntimeState(int32,int32,address)`
- marketplace sale-state reads
- `eth_chainId`

## Build/test/deploy command surface

From `blockchain/package.json`:
- `npm run build`
  - `node scripts/compile.js`
- `npm test`
  - `mocha --recursive --reporter spec test/**/*.test.js`
- `npm run verify`
  - build + test
- `npm run deploy`
  - generic deploy script
- `npm run deploy:realm`
  - realm-aware deploy wrapper
- `npm run ganache:test`
  - `node ../realms/test/start-ganache-local.js`
- `npm run deploy:test:local`
  - `node ../realms/test/deploy-local.js`
- `npm run deploy:main`
  - `node ../realms/main/deploy.js`

## Generated outputs

Current generated outputs include:
- `blockchain/artifacts/*.json`
- `blockchain/artifacts/manifest.json`
- `blockchain/deployments/<network>.json`

`compile.js` rebuilds `blockchain/artifacts/` from scratch and writes 9 artifacts plus the manifest.

## Realm-aware deployment flow

### Generic scripts
- `scripts/deploy.js`
  - low-level deployer; accepts RPC URL, private key, owner, network, and global parameter overrides
- `scripts/deployRealm.js`
  - loads a realm first, then forwards to `deploy.js`
- `scripts/realmConfig.js`
  - resolves `--realm <name-or-path>` to `realm.json` and validates `protocolVersion`

### Thin realm wrappers
- `realms/test/deploy.js`
  - forwards to `deployRealm.js --realm <test-dir>`
- `realms/test/deploy-local.js`
  - convenience wrapper using local Ganache defaults
- `realms/main/deploy.js`
  - forwards to `deployRealm.js --realm <main-dir>`
- `realms/test/start-ganache-local.js`
  - starts a local Ganache node with fixed mnemonic/chain id 31337

## Verified test coverage

`npm run verify` succeeds in `blockchain/` right now.

The current Mocha suite has 8 passing tests covering:
- deployment-record protocol version
- registration/handle uniqueness/unregister/re-register behavior
- runtime-session authorization and runtime-facing permission resolution
- paid chunk claiming and registered-only transfer restrictions
- fixed-price listing purchase + fee retention + sale-state reads
- English auction bidding/refunds/settlement + sale-state reads
- stale listing invalidation after direct token transfer
- chunk abandonment refund + re-claim behavior

## Important boundaries

This layer is the source of truth for:
- player identity
- chunk ownership
- delegated editor permissions
- marketplace sale state
- protocol/economy parameters

It is not the source of truth for:
- per-frame movement
- rendering
- physics
- topology routing
- voxel meshing
- client menus
