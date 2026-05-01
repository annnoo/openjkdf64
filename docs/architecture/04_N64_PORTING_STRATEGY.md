# N64 Porting Strategy

Porting OpenJKDF2 to the N64 requires a strategic approach to overcome memory and processing constraints.

## Target Hardware
- **Processor**: MIPS VR4300 (approx. 93.75 MHz).
- **RAM**: 8MB (Expansion Pak required).
- **Graphics**: Reality Display Processor (RDP) and Reality Signal Processor (RSP).

## AOT (Ahead-of-Time) Preprocessing

To maximize performance and minimize memory usage on the N64, we will move as much processing as possible to a build-time step.

### 1. Asset Conversion
- **Textures**: All MAT and BM files will be converted to N64-native textures (CI4, CI8, RGBA16) and pre-tiled to fit TMEM (4KB).
- **Models**: 3DO files will be converted to a binary format that can be directly loaded into tiny3d-compatible vertex buffers.
- **Level Data**: JKL files will be pre-parsed into a binary format to avoid slow string parsing at load time.

### 2. Audio and Video
- **Audio**: Sound effects (WAV) will be converted to lower sample rates and possibly compressed formats supported by libdragon.
- **Music**: Music will be stripped or converted to high-compression formats.
- **Video**: Smacker video loading will be stripped out entirely to save space and CPU cycles.

### 3. Script Pre-compilation (COG)
- Instead of parsing COG scripts at runtime using `flex` and `byacc`, we will pre-compile them into a compact binary bytecode format during the build process. The N64 engine will only contain the `sithCogExec` virtual machine.

## Rendering Engine (tiny3d Integration)

We will implement a new hardware backend for `std3D.h` using **tiny3d**.

### Optimization Goals:
1.  **RSP Offloading**: Move vertex projection and lighting calculations to the RSP.
2.  **TMEM Management**: The N64's 4KB TMEM is the biggest bottleneck.
    - Implement a smart texture tiler or downscaler.
    - Leverage 4-bit and 8-bit paletted textures to maximize TMEM usage.
3.  **Command Buffering**: Use tiny3d's command buffers to minimize CPU stalling.

## CPU and Math Optimizations

### Fixed Point Math
The VR4300's floating-point performance is limited.
- We will use the `EXPERIMENTAL_FIXED_POINT` system where possible.
- Focus on performance-critical areas: Collision detection, Physics, and AI.

### Memory Footprint
- **Binary Resource Loading**: Move away from string-based GOB/JKL/COG parsing to direct binary loading.
- **Code Stripping**: Remove all unused code, including video playback, developer consoles (unless needed for debugging), and non-N64 platform code.

## Development Phases
1.  **Phase 0: Build-time Tools**: Develop the AOT preprocessing pipeline for assets, COGs, and levels.
2.  **Phase 1: Boot and Basic HAL**: Get the engine booting on N64 using pre-processed binary assets.
3.  **Phase 2: UI and 2D Rendering**: Implement 2D bitmap drawing for pre-processed HUD elements.
4.  **Phase 3: 3D Geometry**: Implement vertex and triangle drawing using tiny3d and binary models.
5.  **Phase 4: Texture Mapping**: Implement the N64 texture cache using pre-tiled textures.
6.  **Phase 5: Performance Tuning**: Optimize math and VM execution for 30+ FPS gameplay.
