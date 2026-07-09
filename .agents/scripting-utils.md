# Scripting Utilities

## Native build and run utilities (`bbs`)

Run from repo root.

Useful commands:
- `bbs info project`
- `bbs update --init-toolchain`
- `bbs build`
- `bbs build -t openrealm_client`
- `bbs build -t openrealm_simulator`
- `bbs build -t openrealm_relay`
- `bbs build -t openrealm_node_launcher`
- `bbs run -t openrealm_client`
- `bbs run -t openrealm_node_launcher -a --realm test --relays 1 --simulators 2 --run-seconds 5`

Recent direct checks performed successfully:
- `bbs info project`
- `bbs build -t openrealm_node_launcher`
- `./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --help`

## Blockchain workspace commands

Run from `blockchain/` unless noted otherwise.

### Install/build/test
- `npm install`
- `npm run build`
- `npm test`
- `npm run verify`

What they do:
- `build`
  - compiles Solidity and rewrites `blockchain/artifacts/`
- `test`
  - runs the Mocha/Ganache orchestration suite
- `verify`
  - runs build + test

Recent direct check performed successfully:
- `npm run verify`

## Deployment helpers

### Generic scripts in `blockchain/scripts/`
- `node scripts/compile.js`
- `node scripts/deploy.js`
- `node scripts/deployRealm.js`

### Package-script wrappers
- `npm run deploy`
- `npm run deploy:realm`
- `npm run ganache:test`
- `npm run deploy:test:local`
- `npm run deploy:main`

### Realm-local wrappers
- `realms/test/start-ganache-local.js`
- `realms/test/deploy-local.js`
- `realms/test/deploy.js`
- `realms/main/deploy.js`

## Preferred command choices

### Native target inspection
```bash
bbs info project
```

### Build a single native target
```bash
bbs build -t openrealm_node_launcher
bbs build -t openrealm_relay
bbs build -t openrealm_simulator
bbs build -t openrealm_client
```

### Open launcher help / confirm launcher CLI
```bash
./build/default-windows-x86_64/bin/Debug/openrealm-node-launcher.exe --help
```

### Local blockchain dev loop
```bash
npm install
npm run verify
```

### Start local Ganache through repo wrapper
From `blockchain/`:
```bash
node ../realms/test/start-ganache-local.js
```
or:
```bash
npm run ganache:test
```

### Local test-realm deploy
From `blockchain/`:
```bash
node ../realms/test/deploy-local.js [privateKey] [ownerAddress]
```
or:
```bash
npm run deploy:test:local
```

### Generic realm-aware deploy
```bash
npm run deploy:realm -- --realm test --private-key YOUR_LOCAL_PRIVATE_KEY --owner YOUR_OWNER_ADDRESS
npm run deploy:realm -- --realm main --rpc https://your-rpc.example --private-key 0xyourprivatekey --owner 0xyourowner
```

## Expected generated outputs

- native build artifacts
  - `build/default-windows-x86_64/` on this machine
- launcher-generated session data
  - `build/node_launcher/<timestamp>/`
- contract artifacts
  - `blockchain/artifacts/`
- deployment records
  - `blockchain/deployments/<network>.json`

## Important scripting rules

- Use `bbs` as the native source of truth, not ad-hoc CMake commands.
- The blockchain helper surface is intentionally JavaScript-first.
- Use realm-aware wrappers whenever deployment defaults come from `realms/<name>/realm.json`.
- `compileContracts()` pins `evmVersion: 'shanghai'` for Ganache compatibility.
- For doc-only changes, use a temporary `hermes-verify-*` script and report the result as ad-hoc verification, not as a full suite claim.
