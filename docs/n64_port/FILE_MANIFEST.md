# OpenJKDF2 N64 Port: File Manifest

This document categorizes the repository files to delineate what to keep, replace, or delete for the bare-metal N64 port.

## KEEP (Core Logic)
These directories contain the core game rules, math, COG logic, and AI that are platform-agnostic and will compile natively on the N64 MIPS VR4300 processor:
- `src/AI/`: Sith Engine Artificial Intelligence.
- `src/Cog/`: COG script parser and Virtual Machine.
- `src/Engine/`: Core engine structures (Keyframes, Materials, Particles, Puppets).
- `src/Gameplay/`: Game rules and item logic.
- `src/World/`: World state, sectors, collision, and thing management.
- `src/Main/` (Partial): High-level logic like `jk.c` and engine init flow, minus OS specific implementations.
- `src/Primitives/`: Base math and geometry primitives.

## GUT & REPLACE (Hardware Interfacing)
These files touch PC hardware layers (SDL, OpenGL, <windows.h>, libc IO) and must be stripped and replaced with `libdragon` equivalents:
- `src/Platform/`: OS-specific wrappers (Windows, Linux, macOS, WASM, Android). Must be replaced with N64 implementations.
- `src/Win95/`: Legacy Windows OS interfaces, standard C IO wrappers, and OS-dependent audio/video stubs.
- `src/Raster/`: The rendering backend. Contains OpenGL logic that needs to be fully gutted and replaced with `tiny3d` RSP microcode calls.
- `src/Devices/`: Interfaces for keyboard, mouse, controllers, and audio (OpenAL/SDL_mixer). Replace with `libdragon` audio and controller APIs.
- `src/General/stdFileUtil.c`, `stdBmp.c`, `stdFont.c`: Re-wire file operations to the N64 Dragon File System (`dfs_read`).

## DELETE (PC Junk)
Files and subsystems not applicable to a memory-constrained N64 environment:
- `src/Gui/`: High-resolution PC menus, control mapping screens, and complex UI layouts. A minimalist replacement is needed.
- `src/Dss/` (Networking): DirectPlay and multiplayer PC networking code. N64 has no native network adapter out-of-the-box for this.
- `3rdparty/`: External PC libraries (SDL, freeglut, libpng, zlib) that bloat the ROM and RAM.
- `.github/`, `cmake_modules/`, `build_*.sh`: Unnecessary build systems; we will strictly use the `libdragon` `Makefile` ecosystem.