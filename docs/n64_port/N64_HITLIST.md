# OpenJKDF2 N64 Port: N64 Hitlist

This list identifies the exact areas requiring deep structural changes to adapt to N64 constraints.

## The VFS Hijack
The N64 lacks an OS file system. Data is read directly from the ROM cartridge.
- **Target Files:** `src/Main/jkRes.c`, `src/Win95/stdGob.c`, `src/types.h`
- **Action:** Intercept `jkRes_FileOpen`, `jkRes_FileRead`, and `std_pHS->fileOpen`.
- **Replacement:** Route these calls to `dfs_read()` (Dragon File System). By pre-extracting `.gob` files and packing them into the ROM via `dfs`, we bypass runtime `.gob` parsing and stream data directly into RAM.

## Memory Management
The N64 Expansion Pak provides a strict 8MB of RAM. The PC dynamic allocation strategy will fragment and crash.
- **Target Functions:** `pSithHS->alloc` and `pSithHS->free` definitions inside `sithHostServices`.
- **Action:** Remove reliance on `malloc`/`free` within the engine.
- **Replacement:** Implement a static zone/arena allocator. Given the predictability of Jedi Knight levels, assign fixed pools for `sithWorld` geometry, `sithThing` entities, and temporary memory for rendering commands (`tiny3d` display lists).

## Endianness Traps
The N64's VR4300 is Big-Endian. The PC assets (`.jkl`, `.3do`, `.cmp`, `.mat`) are Little-Endian.
- **Crash Points:** 
  - Direct fread of structs like `rdVector3`, `rdMatrix34`, `rdMaterial`, and `sithSector`.
  - Parsers in `src/World/sithWorld_Load.c` and `src/World/sithModel.c`.
- **Action:** A data pipeline must be established to pre-byteswap all binary assets *before* packing them into the ROM. Alternatively, inject byte-swapping macros immediately after `dfs_read()` for all integer and float arrays. Big-Endian conversion is mandatory, or floating-point positions will evaluate to NaN, crashing the physics engine.