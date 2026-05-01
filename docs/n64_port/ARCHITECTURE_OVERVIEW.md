# OpenJKDF2 N64 Port: Architecture Overview

## The Main Loop
The engine starts in `src/main.c` with the `main(int argc, char** argv)` function.
The core game loop handles:
- **Event Polling:** Processing OS/Hardware events (currently via SDL/Platform specific implementations).
- **Time/Tick Management:** Calculating delta-time and dispatching logic ticks to the simulation.
- **Engine Tick (`jk_Tick` / `sith_Tick`):** The primary update function for the Sith Engine simulation, which updates game state, AI, and physics.
- **Render Tick:** Pushing the updated world state to the renderer/rasterizer.

## Subsystem Flow
The engine is structured as a collection of interdependent subsystems:
- **Memory (HostServices / `pSithHS`):** All dynamic memory is allocated through a host services struct (`pSithHS->alloc`, `pSithHS->free`). This isolates the engine from direct OS malloc calls.
- **VFS (Virtual File System):** File I/O is routed through `jkRes` and `pSithHS` to handle raw files and extraction from `.gob` archives.
- **COG Virtual Machine (`src/Cog/`):** A custom scripting VM. It reads parsed COG scripts, manages a symbol table and execution stack (`sithCogExec`), and triggers events based on game state changes.
- **Rendering (`src/Raster/`, `src/World/`):** The world is represented as a BSP/Portal engine. Sectors and surfaces are submitted to the rasterizer for drawing.
- **Physics/Collision:** Handled within the `sith` core (e.g., `sithWorld`, `sithSector`, `sithThing`), calculating intersections between things and sector geometry.
- **Audio (`src/Devices/sithSound`):** Sound effects and music playback, tightly coupled to engine events and COG script triggers.

## Data Structures
Core structures holding the game state include:
- `sithWorld`: The root container for the current level, holding arrays of sectors, things, materials, and models.
- `sithThing`: Represents entities in the world (player, enemies, projectiles, items).
- `sithSector` / `sithSurface`: Defines the BSP geometry, portal connectivity, and collision bounds.
- `rdModel3` / `rdMaterial`: Represents 3D mesh data and texture metadata.
- `sithCog`: Represents an instance of a running script, with local variables and state.