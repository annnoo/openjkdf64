# OpenJKDF2 N64 Port: File Architecture & Module Mapping

This document provides a deeper, file-by-file breakdown of the core engine modules that will be ported to the N64. This serves as a quick-reference for the AI strike teams to understand the responsibility of each `KEEP` file without needing to read the entire file.

## 1. World & Geometry (`src/World/`)
These files manage the 3D environment, the BSP tree, and entity collision.
- **`sithWorld.c/.h`**: The root container for the level. Manages the loading, initialization, and ticking of all sectors, surfaces, and things in the current map.
- **`sithSector.c/.h`**: Represents a convex volume in the BSP tree. Handles point-in-sector tests, lighting accumulation, and tracking which entities (Things) are currently inside it.
- **`sithSurface.c/.h`**: Represents a face on a sector. Handles portal logic (adjoins connecting sectors), texture mapping metadata, and collision planes.
- **`sithThing.c/.h`**: The base entity class (Player, Enemies, Projectiles, Items). Handles entity physics integration, bounds testing, and attachment to sectors.
- **`sithModel.c/.h`**: Handles the loading and instance-tracking of 3D meshes (`.3do` files).
- **`sithTemplate.c/.h`**: The data-driven archetype system for Things. Loads templates from `master.tpl` and instantiates them into `sithThing` objects.

## 2. Rendering Primitives (`src/Engine/`)
These files define the math and data structures for the graphics, *independent* of OpenGL.
- **`rdModel3.c/.h`**: The core 3D model data structure. Contains vertices, texture coordinates, and hierarchical nodes (for animated characters). *Team 1 will target this for RSP microcode conversion.*
- **`rdMaterial.c/.h`**: The texture/material definition. *Team 1 and 3 will need to adapt this from paletted PC formats to N64 RGBA16/CI textures.*
- **`rdKeyframe.c/.h` & `sithAnimClass.c/.h`**: The skeletal animation system used by Puppets.

## 3. COG Virtual Machine (`src/Cog/`)
The scripting system driving gameplay logic.
- **`sithCog.c/.h`**: Manages the lifecycle of a script instance.
- **`sithCogParse.c/.h`**: Parses textual `.cog` scripts into bytecode. *Team 1 will replace this entirely with an offline Python/Go script to generate BCOG binaries.*
- **`sithCogExec.c/.h`**: The VM interpreter. Executes the stack-based bytecode.
- **`sithCogFunction.c/.h`**: The massive lookup table of engine API functions (e.g., `Print`, `FireProjectile`) exposed to COG scripts.

## 4. Artificial Intelligence (`src/AI/`)
- **`sithAI.c/.h`**: The main tick for enemy thinking. Handles state machines, pathfinding across sector adjoins, and targeting.
- **`sithAIClass.c/.h`**: Defines the behavior profiles for different enemy types.

## 5. Main Initialization (`src/Main/`)
- **`jk.c` / `jkRes.c`**: The highest-level engine wrappers. `jk_Tick()` drives the entire game. `jkRes.c` handles the virtual file system (which Team 2 will re-wire to the N64 `dfs`).
- **`main.c`**: The OS entry point. *Team 2 will replace the SDL/Windows event loop with a simple libdragon `while(1)` loop pumping controller input and audio buffers.*

## Summary for Strike Teams
- **Team 1 (Data):** Focus strictly on modifying the loaders in `src/World/` and `src/Engine/` to accept N64-optimized, Big-Endian payloads instead of raw PC parsing.
- **Team 2 (Systems):** Focus on `jkRes.c`, `std_pHS` structs, and `main.c` to bind `libdragon` DFS, Controller, and Audio subsystems.
- **Team 3 (Graphics):** Treat `rdModel3`, `sithSector`, and `sithSurface` as read-only data sources. Build `tiny3d` display lists directly from them, ignoring all existing code in `src/Raster/`.