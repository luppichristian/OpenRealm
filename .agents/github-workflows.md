# GitHub Workflows

## Workflow files

- `.github/workflows/orchestration-test.yml`
- `.github/workflows/orchestration-deploy.yml`

These workflows currently cover the blockchain/orchestration workspace only.

## `orchestration-test.yml`

Workflow name:
- `Test Orchestration Layer`

Triggers:
- `push` when paths touch:
  - `blockchain/**`
  - `realms/test/**`
  - `.github/workflows/orchestration-test.yml`
  - `.github/workflows/orchestration-deploy.yml`
- `pull_request` on the same path set
- `workflow_dispatch`

Concurrency:
- group: `orchestration-test-${{ github.ref }}`
- cancels in-progress runs on the same ref

Job behavior:
- runs on `ubuntu-latest`
- works from `blockchain/`
- checks out repo
- sets up Node.js 22 with npm cache keyed by `blockchain/package-lock.json`
- runs:
  - `npm install`
  - `npm run build`
  - `npm test`
- starts ephemeral Ganache
- polls `eth_chainId` until Ganache is ready
- smoke-deploys test realm contracts via `node ../realms/test/deploy.js ...`
- uploads:
  - `blockchain/artifacts`
  - `blockchain/deployments/test.json`
  - `blockchain/ganache.log`

## `orchestration-deploy.yml`

Workflow name:
- `Deploy Orchestration Layer`

Trigger:
- `workflow_dispatch`

Inputs:
- `environment`
- `network`
- `rpc_url`
- `owner_address`
- `upload_artifacts`

Concurrency:
- group: `orchestration-deploy-${{ github.event.inputs.environment || 'local' }}`
- does not cancel in-progress runs

### Job 1: `local-smoke-deploy`
Always runs first.

Behavior:
- installs dependencies
- builds artifacts
- starts ephemeral Ganache
- waits for readiness through JSON-RPC polling
- deploys test realm contracts through `node ../realms/test/deploy.js ...`
- optionally uploads smoke-deploy artifacts

### Job 2: `external-deploy`
Runs only when `network != 'local'` and after the local smoke deploy succeeds.

Behavior:
- uses workflow `environment`
- reads:
  - input `rpc_url`
  - secret `ORCHESTRATION_RPC_URL`
  - secret `ORCHESTRATION_DEPLOY_PRIVATE_KEY`
  - input `owner_address`
  - input `network`
- validates deployment configuration
- deploys main realm contracts through `node ../realms/main/deploy.js`
- optionally uploads:
  - `blockchain/artifacts`
  - `blockchain/deployments/<network>.json`

## Agent guidance for workflow edits

- Keep workflow scope aligned with the orchestration workspace unless the project intentionally adds native CI.
- Preserve the smoke-deploy step before external/main deployment.
- Preserve Node.js 22 unless the repo intentionally changes JS runtime support.
- Preserve working directory `blockchain` for orchestration jobs.
- If changing deployment script names or paths, update both workflows together.
- If changing artifact output paths, update workflow uploads in the same change.
