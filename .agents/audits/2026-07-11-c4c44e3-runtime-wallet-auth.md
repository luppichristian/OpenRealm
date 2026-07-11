# Runtime wallet-auth completion audit

- production date: 2026-07-11
- commit hash: c4c44e3f945948309813025f35d5deb198cb66e0
- scope: `blockchain/scripts/runtimeWalletAuth.js`, `node/runtime/*`, `node/client/Game.cpp`, `node/targets/Relay.cpp`, `node/targets/Simulator.cpp`
- status: completed
- completion date: 2026-07-11

## Completion note

This audit records the runtime wallet-authentication completion pass that closes the major unresolved identity/authentication items from the earlier runtime safety audit.

Implemented in the codebase:
- wallet-backed runtime signer resolution via `blockchain/scripts/runtimeWalletAuth.js`
- native runtime auth helper layer in `node/runtime/RuntimeAuth.*`
- signed packet proofs carried across handshake, challenge request, challenge response, join, topology, and player packet families
- runtime handshake identity fields and challenge nonce flow in `node/runtime/Packet.*`
- packet-family signature/session/sequence validation in `node/runtime/PacketValidator.*`
- per-peer auth/session tracking extensions in `node/runtime/ActiveNodeBucket.*`
- runtime session challenge/response authentication flow and authenticated-peer gating in `node/runtime/RuntimeSession.*`
- startup/config wiring for relay, simulator, and client runtime targets
- protocol version bump for the authenticated runtime packet surface

Notable implementation fix during verification:
- the runtime auth helper temp-payload path generator in `RuntimeAuth.cpp` was changed from timestamp-only naming to process-unique plus counter-unique naming, fixing a real concurrent-start race where two nodes could stomp each other's signer payload files and produce impossible signer mismatches.

## Verification summary

Fresh verification completed for this pass:
- `bbs build -t openrealm_relay`
- `bbs build -t openrealm_simulator`
- `bbs build -t openrealm_client`
- ad-hoc verifier launched two clean localhost relays on fresh ports and confirmed mutual authenticated-peer events in both relay logs:
  - `runtime authenticated peer 127.0.0.1:48102`
  - `runtime authenticated peer 127.0.0.1:48101`

Verification notes:
- the clean concurrent repro workspace used `C:\Users\Utente\AppData\Local\Temp\hermes-openrealm-auth-clean-ox38f9_u`
- transport was separately sanity-checked with a raw UDP garbage probe earlier in the investigation to confirm the receive/parse loop was alive before narrowing the auth state machine

## Executive summary

The runtime now has a concrete wallet-backed peer authentication flow instead of compatibility-only acceptance.

What this pass adds:
- each node resolves a signer identity from wallet material
- high-impact runtime packets carry signed proofs
- direct peers authenticate through handshake plus challenge/response
- packet families now validate session, sequence, and signature expectations before normal handling
- authenticated status is tracked per peer and used as a gate in later runtime behavior

Bottom line: this pass materially upgrades the runtime from unauthenticated compatibility filtering toward authenticated peer sessions backed by the existing wallet surface.

## Files changed in this pass

Primary new files:
- `blockchain/scripts/runtimeWalletAuth.js`
- `node/runtime/RuntimeAuth.cpp`
- `node/runtime/RuntimeAuth.h`

Primary modified files:
- `node/runtime/Packet.cpp`
- `node/runtime/Packet.h`
- `node/runtime/PacketValidator.cpp`
- `node/runtime/PacketValidator.h`
- `node/runtime/ActiveNodeBucket.cpp`
- `node/runtime/ActiveNodeBucket.h`
- `node/runtime/RuntimeSession.cpp`
- `node/runtime/RuntimeSession.h`
- `node/runtime/ProtocolVersion.h`
- `node/client/Game.cpp`
- `node/targets/Relay.cpp`
- `node/targets/Simulator.cpp`

## Limit of this audit

This audit records the authenticated runtime session pass and its focused localhost verification. It does not claim a comprehensive hostile-network review beyond the implemented signed packet/session/authentication flow exercised here.
