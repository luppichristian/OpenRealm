# OpenRealm orchestration layer audit

Production date: 2026-07-10
Commit hash: ae3fbb0158058758d680df0f056478d2cb6e1023

## Scope

This audit covers:
- Solidity orchestration layer under `blockchain/`
- Native C++ blockchain wrapper layer under `node/blockchain/`

Goals:
- check whether the C++ wrapper surface reflects the Solidity implementation correctly
- identify implementation gaps in the native interface
- assess whether the Solidity orchestration layer is safe to ship

## What I verified

1. Solidity build + test suite
   - ran `npm run verify` in `blockchain/`
   - result: 8 passing tests

2. Native build path for the blockchain wrapper layer
   - ran `bbs build -t openrealm_relay`
   - result: success, produced `build/default-windows-x86_64/bin/Debug/openrealm-relay.exe`

## Executive summary

The Solidity/C++ API surfaces are mostly aligned at the shape level: the native wrappers cover the important deployed contract surfaces for `PlayerRegistry`, `ChunkClaims`, `Marketplace`, and `GlobalParams`, including the runtime-facing read models the repo expects.

However, I do not think the orchestration layer is ready to ship as-is.

The main blocker is in `Marketplace.sol`: the auction flow uses inline push refunds and inline seller payouts. A malicious or simply non-payable contract bidder/seller can intentionally make those ETH transfers fail and freeze bid progression, stale-auction invalidation, or final settlement. In the worst case, bidder funds can remain trapped in the marketplace because no recovery path exists.

Separately, the C++ write-path is operationally incomplete for typical real RPCs: `node/blockchain/BlockchainRpcClient.cpp` only uses `eth_sendTransaction`, and `Wallet` stores only an address, not signing material. That means the native write APIs only work against RPCs that already expose an unlocked account. Reads are fine; writes are not generally deployable.

## Detailed findings

### 1. High: auction flow is vulnerable to bidder/seller-controlled denial of service

Severity: High

Affected code:
- `blockchain/contracts/Marketplace.sol:310-346`
- `blockchain/contracts/Marketplace.sol:365-386`
- `blockchain/contracts/Marketplace.sol:389-416`
- `blockchain/contracts/PlayerRegistry.sol:64-97`

Issue:
- `PlaceAuctionBid(...)` immediately refunds the previous highest bidder with a raw `call` and reverts on refund failure.
- `InvalidateStaleAuction(...)` also refunds the previous highest bidder inline and reverts on refund failure.
- `SettleAuction(...)` pays the seller inline and reverts on payout failure.
- `PlayerRegistry` allows any address to register; there is no EOA-only restriction, so contract bidders/sellers are allowed.

Impact:
- A highest bidder implemented as a contract with a reverting fallback/receive function can block all higher bids forever.
- The same kind of bidder can also block stale-auction invalidation, because the refund path must succeed before invalidation completes.
- A seller implemented as a contract with a reverting fallback/receive function can block `SettleAuction(...)` after the auction ends.
- In the settlement case, the highest bid remains stuck in the marketplace because the whole transaction reverts and there is no pull-based recovery path for the bidder.

Why this matters:
- This is not just a seller self-DoS. In auctions it can lock counterparty funds and freeze market state.
- The current tests only use EOAs and therefore do not exercise adversarial receiver behavior.

Recommended fix:
- Convert bidder refunds and seller payouts to a pull-payment model.
- Record refundable balances / seller proceeds in storage and let recipients withdraw separately.
- Keep auction/listing state transitions independent from external ETH transfers.
- Add explicit tests with reverting receiver contracts for:
  - previous bidder refund failure
  - seller payout failure on auction settlement
  - seller payout failure on listing purchase
  - stale-auction invalidation with a reverting prior bidder

### 2. Medium: listing purchase can be made permanently unsellable by a non-payable seller contract

Severity: Medium

Affected code:
- `blockchain/contracts/Marketplace.sol:225-263`
- `blockchain/contracts/PlayerRegistry.sol:64-97`

Issue:
- `PurchaseListing(...)` transfers the chunk and then pushes ETH to the seller via raw `call`, reverting if the payout fails.
- Because any address can register, a seller can be a contract that refuses ETH.

Impact:
- The purchase transaction fully reverts, so buyer funds are not trapped, but the listing becomes effectively unpurchasable until ownership changes or the seller cancels.
- This is a less severe variant of the auction issue, but it still makes first-party marketplace flows unreliable.

Recommended fix:
- Same pull-payment redesign as above.

### 3. Medium: native C++ write surface is not generally usable against normal remote RPC providers

Severity: Medium

Affected code:
- `node/blockchain/BlockchainRpcClient.cpp:136-179`
- `node/blockchain/SmartContract.cpp:68-93`
- `node/blockchain/Wallet.h:5-23`
- `node/blockchain/Wallet.cpp:12-42`

Issue:
- Native transactions are sent only through `eth_sendTransaction`.
- `Wallet` stores only an account address and has no private key, signer, hardware-wallet hook, raw-transaction builder, nonce management, or `eth_sendRawTransaction` path.

Impact:
- The C++ wrappers expose many mutating methods (`Register`, `ClaimChunk`, `CreateListing`, `PlaceAuctionBid`, etc.), but those writes only work if the RPC node itself has an unlocked account for the sender.
- That is fine for local Ganache-style development, but not for the normal mainnet/testnet RPC model used by hosted providers.
- So the native interface currently reflects the Solidity write surface at the API level, but not at the operational level.

Recommended fix:
- Add a real signing path for native writes:
  - local key signing + `eth_sendRawTransaction`, or
  - a wallet/provider abstraction that delegates signing externally.
- Clearly separate read-only wrappers from write-capable wrappers until this is implemented.
- Add a repo note that current native writes require unlocked RPC accounts if that behavior is intentionally temporary.

### 4. Medium: C++ wrappers return predicted ids from preflight `eth_call`, not confirmed ids from the mined transaction

Severity: Medium

Affected code:
- `node/blockchain/ChunkClaimsContract.cpp:158-172`
- `node/blockchain/MarketplaceContract.cpp:286-301`
- `node/blockchain/MarketplaceContract.cpp:327-352`

Issue:
- `ClaimChunk`, `CreateListing`, and `CreateAuction` first run a preview `eth_call` to read the returned id, then send the real transaction, and then return the previewed id together with the receipt.
- This assumes the relevant counters/state do not change between preview and inclusion.

Impact:
- Under concurrent activity, the returned `tokenId`, `listingId`, or `auctionId` can be stale even if the transaction itself succeeds.
- The receipt does not correct the predicted value, because the wrapper does not parse emitted events/logs.

Recommended fix:
- Treat these ids as unknown until confirmed from transaction logs, or
- parse emitted events from the receipt and return the actual mined id.

### 5. Low: marketplace constructor validates `globalParamsAddress` but not `chunkClaimsAddress`

Severity: Low

Affected code:
- `blockchain/contracts/Marketplace.sol:145-160`

Issue:
- The constructor rejects a zero `globalParamsAddress` but does not reject a zero `chunkClaimsAddress`.

Impact:
- A misconfigured deployment can produce a marketplace that deploys successfully but is unusable.
- This is more of a deployment-hardening issue than an exploitable vulnerability.

Recommended fix:
- Add an explicit zero-address check for `chunkClaimsAddress`.

### 6. Low: `WithdrawFees` can burn accumulated fees if the owner passes the zero address

Severity: Low

Affected code:
- `blockchain/contracts/Marketplace.sol:468-476`

Issue:
- `WithdrawFees(address payable recipient)` does not validate `recipient != address(0)`.

Impact:
- Owner mistake can irreversibly burn protocol fees.

Recommended fix:
- Reject `address(0)` recipients.

### 7. Low: handle uniqueness is raw-hash based with no normalization policy

Severity: Low

Affected code:
- `blockchain/contracts/PlayerRegistry.sol:37-40`
- `blockchain/contracts/PlayerRegistry.sol:240-250`

Issue:
- Handle uniqueness is based on `keccak256(bytes(handle))` with no normalization.

Impact:
- Case variants or visually confusable Unicode strings can coexist if the product later expects canonical human-facing uniqueness.
- This is mainly a product/integrity issue, not a direct chain-safety issue.

Recommended fix:
- Decide whether handles should be case-sensitive.
- If not, normalize before hashing and document the policy.

## C++ wrapper completeness review

### What looks good

By manual inspection, the wrapper layer is broadly aligned with the current Solidity surface:
- `PlayerRegistryContract.*` covers ownership helpers, registration flow, runtime-session flow, and the runtime-facing read methods.
- `ChunkClaimsContract.*` covers admin wiring, claim/abandon/transfer/editor operations, runtime reads, and the exposed ERC721-like surface.
- `MarketplaceContract.*` covers fee configuration, listings, auctions, invalidation, settlement, sale-state reads, and fee withdrawal.
- `GlobalParamsContract.*` covers all immutable parameter getters.
- `Blockchain.*` correctly wires the four contract wrappers into one native facade.

I did not find any obvious selector-level mismatch during manual comparison of the wrapper signatures against the Solidity declarations.

### What is still incomplete

The main completeness problems are operational rather than shape-level:
- write calls require unlocked RPC accounts
- returned ids for create/claim flows are speculative previews rather than confirmed mined values

So the read surface is in good shape, but the write surface is not yet robust enough for production use.

## Test coverage observations

Current coverage is good for the happy-path orchestration model:
- registration / handle uniqueness / unregister
- runtime-session authorization
- chunk claims and transfers
- listing purchase flow
- auction bid / settle flow
- stale listing invalidation
- abandonment refund

But important adversarial cases are not covered:
- seller is a contract that rejects ETH
- bidder is a contract that rejects ETH
- refund/payout failure paths for listing and auction flows
- concurrent-id correctness for the C++ wrapper preview pattern
- native write behavior against non-unlocked RPC endpoints

## Shipping recommendation

Recommendation: do not ship the orchestration layer yet.

Minimum blockers before shipping:
1. Replace marketplace push payments/refunds with a pull-payment accounting model.
2. Add adversarial contract tests for refund/payout failure scenarios.
3. Decide whether the native layer is read-only for now or implement real transaction signing for `node/blockchain/` writes.
4. Stop returning speculative ids from the C++ create/claim wrappers or confirm them from mined receipts/events.

## Suggested next steps

1. Patch `Marketplace.sol` to use withdrawable balances for:
   - outbid refunds
   - seller proceeds
   - protocol fee accounting remains separate
2. Add Solidity tests with malicious receiver contracts.
3. If native writes matter for shipping, add signed raw transaction support in `node/blockchain/`.
4. Add a small native integration test or verifier that exercises read-only wrapper calls against the local deployment artifacts.
