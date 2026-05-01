# OpenJKDF2 Architecture Overview

OpenJKDF2 is a function-by-function reimplementation of Jedi Knight: Dark Forces II in C. The architecture follows the original engine's design closely, which was modular and layered, typical for high-performance games of the late 90s.

## Engine Layers

### 1. Game Layer (JK / Sith)
The top-level logic of the game.
- **Source**: `src/Main`, `src/World`, `src/Gameplay`, `src/Cog`
- **Responsibilities**:
    - Character movement and AI (`src/World/sithActor.c`, `src/AI`)
    - Level state management (Sectors, Surfaces)
    - Scripting execution (COG)
    - Physics and Collision (`src/Engine/sithPhysics.c`, `src/Engine/sithCollision.c`)
    - Weapon systems and Player state (`src/World/sithWeapon.c`, `src/World/jkPlayer.c`)

### 2. Engine Layer (RenderDroid / rd)
The core rendering and math engine.
- **Source**: `src/Engine`, `src/Primitives`
- **Responsibilities**:
    - 3D Math (Vectors, Matrices)
    - Model loading and processing (3DO)
    - Visibility determination (Portal traversal)
    - Clipping and Projection
    - Animation systems (Keyframes, Puppets)

### 3. Rasterization Layer (Raster)
The bridge between 3D geometry and 2D drawing.
- **Source**: `src/Raster`
- **Responsibilities**:
    - Batching triangles and lines (`rdCache.c`)
    - Software rasterization (Original engine's fallback)
    - Managing vertex buffers and texture states for the hardware backends.

### 4. Platform Abstraction Layer (std / Platform)
The interface with the Operating System and Hardware.
- **Source**: `src/Platform`, `src/Win95`, `src/General`
- **Responsibilities**:
    - File I/O and Archive management (GOB)
    - Memory allocation
    - Sound system abstraction
    - Hardware 3D acceleration interface (`std3D.h`)
    - Windowing and Input (SDL2, Win32)

## Component Interaction Flow

1. **Initialization**: `Main.c` calls `sith_Startup`, which initializes all subsystems (Memory, GOB, Sound, Video, World).
2. **Main Loop**:
    - **Input**: Polls keyboard/mouse/joystick.
    - **Update**: `sith_Tick` updates physics, AI, and scripts.
    - **Render**: `sithRender_Draw` calculates visibility and sends geometry to the rasterizer.
3. **Shutdown**: Cleans up resources and closes hardware interfaces.
