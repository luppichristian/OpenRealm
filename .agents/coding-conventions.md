# Coding Conventions

## General rule

These conventions are repo-specific and should be matched when editing OpenRealm.
Do not replace them with generic personal preferences.

## C++ formatting

Follow `.clang-format`.

Current expectations:
- 2-space indentation
- no tabs
- Allman braces
- no practical line-length limit (`ColumnLimit: 0`)
- pointer alignment `Type* name`
- includes are not auto-sorted

## Naming

### Native C++
- classes, structs, enums, functions, and methods: `PascalCase`
- scoped constants: `kPascalCase`
- ordinary variables and member fields: `camelCase`
- no `m_`, `_`, or `s_` prefixes for member fields
- compile-time config values in `WorldConfig.h`: `UPPER_SNAKE_CASE`

### Solidity
- prefer `PascalCase` for public/external function names to stay visually aligned with the C++ style
- when inheriting standardized APIs such as ERC721 methods, keep the standard spellings for compatibility

## Namespace policy

- do not introduce named project-wide namespaces by default
- the codebase is intentionally flat at the symbol level
- do not wrap the project in `namespace OpenRealm` unless explicitly requested
- unnamed `namespace { ... }` blocks are already common in `.cpp` files and are fine for file-local helpers
- `static` free functions are also common; match the surrounding file instead of forcing one pattern everywhere

## Data modeling

The repo is comfortable with POD-style structs exposing public fields directly.

Use this style for plain state carriers and read models.
Examples to match include:
- `WorldData.h`
- `WorldEvent.h`
- chunk mesh job structures

Guidelines:
- use simple structs with defaults instead of over-encapsulation
- reserve `*State` structs for plain data carriers/read models
- do not require callers to assemble internal lifecycle state blobs for owning classes
- designated initializers are acceptable where supported by the compiler setup

## Ownership and class design

- classes tend to be concrete and stateful
- ownership is direct by value where practical
- keep connection/lifecycle flags owned by the class itself
- expose explicit lifecycle methods such as connect/disconnect or shutdown/reset
- do not introduce smart pointers by default
- prefer direct ownership, stack allocation, plain members, or explicit non-owning raw pointers as appropriate
- if heap allocation is truly necessary, justify it with concrete ownership/lifetime needs
- copy is often explicitly deleted for resource-owning types via `NonCopyable`
- destructors commonly centralize cleanup through `Shutdown()` or `Reset()`
- avoid inheritance-heavy abstractions unless strongly justified by repo-local needs

## Function style

- free functions are a normal pattern here, especially for:
  - rendering entrypoints
  - input translation
  - small helpers
- prefer straightforward imperative code
- early returns are normal and preferred for guards
- avoid unnecessary callback-heavy or template-heavy structure when a simple imperative path matches existing code better

## Header patterns

- use `#pragma once`
- headers may legitimately contain:
  - type definitions
  - inline constants
  - inline utility functions
- do not force all logic into `.cpp` files when the repo already uses header-driven utility/config headers

## Memory and containers

The repo mixes STL and manual allocation where appropriate.

Current patterns include:
- `std::array`
- `std::unordered_map`
- `std::deque`
- `std::vector`
- `malloc`
- `realloc`
- `free`
- `memset`

Guidance:
- do not auto-modernize raw allocation paths if it obscures hot-path behavior or ownership transfer
- if touching mesh-memory ownership, preserve current release/reset semantics carefully

## Architecture placement rules

- gameplay/world logic belongs in `node/world/` unless clearly client glue
- startup, rendering, meshing, audio, and high-level input flow belong in `node/client/`
- executable `main()` functions belong in `node/targets/`
- runtime transport/topology belongs in `node/runtime/`
- native blockchain/orchestration integration belongs in `node/blockchain/`

## Important implementation-specific reminders

- `World` should remain usable as a headless runtime component
  - no asset ownership
  - no audio playback
  - no GPU/model lifetime management
  - no direct frame-time polling from raylib
- client render state lives outside `VoxelWorld`
- `WorldData.h` should not regain render-state fields like `Model`, upload flags, or bounds caches
- background task execution is centralized in `TaskManager`
- client mesh jobs submit into shared `TaskManager` from `WorldMeshSystem`

## Documentation/agent convention

Agent-facing durable repo knowledge belongs in `.agents/`, not in user-facing README files.
