# OpenRealm Agent Docs

Use this directory as the canonical repo guide.

Maintenance rule:
- `.agents/` is the canonical agent memory for this repository.
- After any prompt that changes repo structure, behavior, commands, workflows, conventions, or other durable repo knowledge, update the relevant `.agents/*.md` files before finishing.
- Agents are also responsible for keeping the user-facing binary docs under `docs/` in sync, especially `docs/relay_node.md`, `docs/client_node.md`, `docs/simulator_node.md`, and `docs/node_launcher.md` when targets, flags, interfaces, or runtime behavior change.
- Keep `README.md` minimal: it should only help agents jump to the right file.

Read only the file you need:
- `project-design.md` — product direction, current reality, and architecture guardrails
- `project-structure.md` — directory ownership, generated outputs, and where code actually lives
- `build-targets.md` — `bbs` targets, dependencies, and native build/run entrypoints
- `runtime-layer.md` — ENet runtime, packet/versioning model, node roles, config schema, launcher caveats
- `orchestration-layer.md` — Solidity workspace, contracts, scripts, tests, and realm deployment flow
- `scripting-utils.md` — command entrypoints and wrappers
- `coding-conventions.md` — formatting and code-style rules
- `github-workflows.md` — CI and manual deploy workflow behavior
- `audits.md` — audit storage rules and required metadata for saved audits
