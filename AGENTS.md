# AGENTS.md

## Maintenance Rule

Treat this file as the agent's working memory for the repository.

- Always keep `AGENTS.md` up to date automatically when you discover, change, or establish project conventions, architecture, directory structure, build rules, or important implementation constraints.
- Update this file as part of the same work when those facts change; do not leave it stale for later.
- Prefer correcting `AGENTS.md` immediately over relying on implicit memory or prior conversation context.

## Project

OpenRealm is a C++20 voxel game prototype aimed at a serverless, decentralized voxel world.

What exists in this repository today is the local engine/client foundation:

- real-time game loop
- local voxel storage and editing
- player simulation and collision
- asynchronous chunk meshing
- raylib-based rendering/audio

Do not describe this repo as if it already contains distributed networking or consensus logic. The decentralized/serverless model is the project direction, not the current implementation state.

## Tech Stack

- Language: C++20
- Build system: `bbs`
- Windowing/render/input/audio: `raylib`
- Profiling dependency: `tracy`

## Repository Layout

- `src/Main.cpp`
  - Minimal entrypoint. Creates `Game` and calls `Run()`.
- `src/Game.*`
  - App shell and frame loop.
  - Owns window/audio init and shutdown.
  - Owns the top-level `World` instance and `ColorMenu`.
- `src/PlayerController.*`
  - Free-function input layer.
  - Converts mouse/keyboard input into `WorldEvent`s.
  - Draws HUD elements.
- `src/ColorMenu.*`
  - In-game voxel color picker UI.
- `src/AssetManager.*`
  - Lazy asset cache for textures, shaders, shader locations, and sounds.
- `src/SoundPlayer.*`
  - Local sound playback helper with pooled alias voices.
- `src/Utils.h`
  - Header-only utilities used across world/render/gameplay code.
- `src/Base.h`
  - Central low-level include hub for raylib, raymath, and C headers.
- `src/world/World.*`
  - Top-level world coordinator.
  - Owns voxel data, player system, event queue, asset/audio integration, and mesh system.
- `src/world/VoxelWorld.*`
  - Authoritative local voxel/chunk storage and collision/raycast logic.
- `src/world/PlayerSystem.*`
  - Player state management and movement/look/edit simulation.
- `src/world/WorldMeshSystem.*`
  - Mesh job capture, queueing, and GPU upload application.
- `src/world/ChunkMeshWorkerPool.*`
  - Background worker threads for chunk meshing.
- `src/world/ChunkMesher.*`
  - Actual chunk mesh generation implementation.
- `src/world/ChunkMeshBuilder.*`
  - Raw mesh buffer builder.
- `src/world/ChunkMeshJobResult.*`
  - Move-only mesh job result container.
- `src/world/WorldRenderer.*`
  - Free-function world rendering path.
- `src/world/WorldEvent.h`
  - World event enum and payload struct.
- `src/world/WorldEventQueue.*`
  - Fixed-capacity local event queue.
- `src/world/WorldConfig.h`
  - Global compile-time constants.
- `src/world/WorldData.h`
  - Shared POD data structures for chunks, players, ray hits, and movement results.
- `assets/`
  - Runtime shaders and sounds.
- `build/`
  - Local build artifacts.
- `dist/`
  - Output/distribution artifacts.

## Architecture Notes

- The codebase is mostly split into two layers:
  - app shell in `src/`
  - world/simulation/render systems in `src/world/`
- `World` is the main composition root for world-side systems.
- The code favors direct ownership and explicit orchestration over abstract interfaces.
- Voxel data is chunked and sectioned. Meshing is asynchronous, while gameplay/world mutation remains local and immediate.
- Rendering is a mix of:
  - shader-driven fullscreen floor rendering
  - uploaded chunk meshes rendered in 3D
- Input and some rendering helpers are implemented as free functions rather than class methods.

## Style And Conventions

These are not generic C++ preferences. They reflect the code that is already in this repository.

### Formatting

- Follow `.clang-format`.
- 2-space indentation.
- No tabs.
- Allman braces.
- No practical line-length limit; `ColumnLimit: 0`.
- Pointer alignment is `Type* name`.
- Includes are not auto-sorted.

### Naming

- Classes, structs, enums, functions, and methods use `PascalCase`.
- Constants use `kPascalCase` when they are scoped constants in classes or files.
- Regular variables and member fields use plain `camelCase`.
- Member fields do not use `m_`, `_`, or `s_` prefixes.
- Global compile-time configuration values in `WorldConfig.h` use `UPPER_SNAKE_CASE`-style identifiers such as `CHUNK_SIZE_XZ`, `MOVE_SPEED`, and `WORLD_UP`.

### Namespaces

- Do not introduce named namespaces by default.
- The current codebase is intentionally flat at the symbol level.
- The only namespace usage currently present is an anonymous namespace in `ChunkMesher.cpp` for file-local helpers.
- For file-local helpers in `.cpp` files, prefer either:
  - an anonymous namespace
  - `static` free functions
- Do not wrap the project in `namespace OpenRealm` unless the user explicitly asks for that refactor.

### Data Modeling

- This codebase is comfortable with POD-style structs that expose public fields directly.
- `WorldData.h`, `WorldEvent.h`, and `ChunkMeshJob` are the model for this style.
- Use simple structs with defaults for state carriers instead of over-encapsulating them.
- Designated initializers are already used and are acceptable where supported by the current compiler setup.

### Class Design

- Classes tend to be concrete and stateful.
- Ownership is direct by value where practical.
- Do not introduce smart pointers by default.
- Prefer direct ownership, stack allocation, plain members, or explicit raw-pointer/non-owning pointer usage consistent with the existing codebase.
- If a heap allocation is genuinely necessary, justify it with a concrete ownership/lifetime need instead of reaching for `std::unique_ptr` or `std::shared_ptr` automatically.
- Copy is often explicitly deleted for resource-owning types.
- Destructors commonly call `Shutdown()` or `Reset()` to centralize cleanup.
- Avoid adding inheritance-heavy abstractions unless there is a strong repo-local reason.

### Functions

- Free functions are a normal pattern here, especially for:
  - rendering entrypoints
  - input translation
  - small helpers
- Prefer straightforward imperative code over callback-heavy or template-heavy designs.
- Early returns are common and should remain the default for guard conditions.

### Header Patterns

- Use `#pragma once`.
- Headers commonly contain:
  - type definitions
  - inline constants
  - inline utility functions
- `Utils.h` and `WorldConfig.h` are intentionally header-driven; do not force everything into `.cpp` files just to satisfy a generic rule.

### Memory And Containers

- The code mixes STL containers and manual allocation depending on the problem:
  - `std::array`, `std::unordered_map`, `std::deque`, `std::vector`
  - `malloc`, `realloc`, `free`, `memset` in mesh-building paths
- Do not "modernize" raw allocation paths automatically if it would obscure hot-path behavior or ownership transfer.
- If touching mesh memory ownership, preserve the current release/reset semantics carefully.

## Implementation Guidance

- Keep gameplay/world logic in `src/world/` unless it is clearly app-shell/UI glue.
- Keep app startup, window lifecycle, and high-level input flow in `src/`.
- When adding source files, check whether they belong under `src/` or `src/world/`; the current `project.bbs` glob picks up:
  - `src/*.cpp`
  - `src/world/*.cpp`
- New subdirectories under `src/` are not automatically part of the build unless `project.bbs` is updated.
- If you add assets, put them under `assets/` with stable folder naming that matches the current `BuildAssetPath()` convention.

## Behavior To Preserve

- Local playability comes first.
- Voxel edits should remain immediate from the local player perspective.
- Chunk meshing should remain asynchronous and budgeted.
- World state and rendering should stay simple enough to iterate on quickly.

When extending the codebase toward decentralization, build on top of these local systems instead of prematurely replacing them with a traditional client/server architecture.
