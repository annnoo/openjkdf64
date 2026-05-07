# OpenJKDF2 N64 Port — Road to the Main Menu

*Last updated: 2026-05-06*

This document maps every step between the current minimal boot ROM and a working main-menu screen. Each step is self-contained, testable, and sequenced so that later steps build on earlier ones.

---

## Current State

- `src/main_n64.c` boots, displays static text, and increments a frame counter.
- `build_n64.sh` drives cmake inside the libdragon Docker container → `build_n64/openjkdf2.z64`.
- The engine (`src/Main/Main.c` etc.) is **not compiled** yet — the CMakeLists.txt N64 path skips everything.
- All `src/Platform/N64/` files are stubs (every function returns immediately).

---

## Target

The N64 ROM loads, initialises the engine, and displays the JK main-menu screen — background bitmap, menu options, mouse/controller cursor.

---

## Step 1 — Filesystem: DFS + GOB extraction pipeline

**Why first:** `Main_Startup` calls `jkGob_Startup` → `jkRes_Startup` almost immediately. Without a readable filesystem the engine cannot open a single file.

### What to do

1. **Create a data-pipeline script** (`tools/pack_n64_data.sh` or `tools/pack_n64_data.py`) that:
   - Accepts a JK install directory as input.
   - Unpacks `JK.GOB` (and `RES2.GOB` etc.) using an existing GOB extractor.
   - Converts all assets that need it (see Step 3 — endianness).
   - Packages the result into a DFS image (`mkdfs` from libdragon tools).
   - Embeds the DFS image into the ROM via `n64tool --fs`.

2. **Route `std_pHS->fileOpen` through DFS** in `src/Win95/stdGob.c` (or a thin N64 shim):
   - `#ifdef TARGET_N64` → replace `fopen` with `dfs_open`, `fread` with `dfs_read`, `fclose` with `dfs_close`.
   - `stdFileUtil` directory-scan functions can return empty/stub results for now.

3. **Enable `dfs_init(DFS_DEFAULT_LOCATION)`** in `src/main_n64.c` (was disabled to avoid crash — now safe once DFS image is embedded).

### Files touched
- `tools/pack_n64_data.sh` (new)
- `src/Win95/stdGob.c` — `#ifdef TARGET_N64` file-I/O shim
- `src/main_n64.c` — re-enable `dfs_init`
- `Makefile` / CMakeLists N64 path — embed DFS image

### Test
ROM boots and `dfs_open("ui/jk.gob")` returns a valid handle (print to isviewer debug log).

---

## Step 2 — Engine compilation: add engine source files to the N64 CMake build

**Why here:** Once FS works, the engine can be compiled in. We add files incrementally to avoid a wall of 500 build errors.

### What to do

Add source files to the N64 cmake path in `CMakeLists.txt` (the early-exit block) in this order, fixing compile errors as they appear:

1. **Core utilities** (no platform deps):
   - `src/General/std*.c` (stdString, stdMath, stdMemory, stdColor, stdFnames, stdFont, stdBitmap, stdConffile, stdFileUtil, stdHashTable, stdLinklist, stdPcx, stdLbm)
   - `src/Primitives/rd*.c`
   - `src/Win95/stdGob.c`, `stdGdi.c` (stub what isn't needed)

2. **Cog / globals**:
   - The N64 path uses `COG_USE_PRECOMPILED` — skip the cogapp python step.
   - Provide a pre-generated `globals.c` / `globals.h` (run cogapp once on host, commit the output).
   - Add `src/Cog/sithCogBinary.c` (precompiled cog loader).

3. **Engine core**:
   - `src/Engine/rdroid.c`, `rdActive.c`, `rdCanvas.c`, `rdClip.c`, etc.
   - `src/Raster/rdCache.c`, `rdFace.c`, `rdRaster.c`
   - `src/World/sithWorld.c`, `sithSurface.c`, `sithModel.c`, etc.
   - `src/Main/sithMain.c`, `jkMain.c`, `jkGame.c`, `jkRes.c`, `jkGob.c`

4. **Platform N64 files** (already stubbed):
   - `src/Platform/N64/std3D.c`
   - `src/Platform/N64/stdControl.c`
   - `src/Platform/N64/stdSound.c`
   - `src/Platform/N64/jkGUIDisplay.c`
   - `src/Platform/N64/Window_N64.c`
   - `src/Platform/N64/fcaseopen_stub.c`

5. **GUI**:
   - `src/Gui/jkGUI*.c` (all menu files)

6. **Switch `main_n64.c` to call `Window_Main_Linux`** (already written in `Window_N64.c`) instead of the standalone display loop.

### Key guards already in place
- `src/General/stdFileUtil.c` — `dirent`/`nftw` guarded for N64
- `src/Main/jkCutscene.c` — smacker guarded
- `src/Cog/sithCogBinary.c` — compress guarded

### Files touched
- `CMakeLists.txt` N64 early-exit block — grow `SRCS` list incrementally
- Various `.c` files — new `#ifdef TARGET_N64` guards as needed

### Test
ROM compiles with all engine files. `Main_Startup` is called (confirm via isviewer `printf`).

---

## Step 3 — Endianness: byte-swap all binary assets

**Why here:** The VR4300 is big-endian. JK assets (`.jkl`, `.3do`, `.mat`, `.cmp`) are little-endian binary structs. Reading them raw will give garbage floats → crash.

### What to do

**Preferred approach — offline conversion (pre-swap at pack time):**

1. Add a byte-swap pass to the data-pipeline script (Step 1).
2. For each known binary format, swap multi-byte fields before packing into DFS.
3. At runtime, `dfs_read` just gets already-correct big-endian data — no runtime cost.

**Fallback — runtime swap macros:**
- Add `#ifdef TARGET_N64` swap macros (`le16_to_cpu`, `le32_to_cpu`) in the struct-read paths:
  - `src/World/sithWorld.c` — JKL loader
  - `src/World/sithModel.c` — 3DO loader
  - `src/World/sithMaterial.c` — MAT loader
  - `src/Engine/rdColormap.c` — CMP loader

### Files touched
- `tools/pack_n64_data.sh` — byte-swap pass
- Loader `.c` files — `le32_to_cpu` macros (if runtime approach)

### Test
`sithWorld_Load("jedi.jkl")` completes without crashing; sector count reads as a sane number.

---

## Step 4 — Memory: arena allocator

**Why here:** N64 has 8 MB (or 4 MB without Expansion Pak). `malloc`/`free` fragmentation will crash during level load.

### What to do

1. **Implement a simple two-zone arena allocator** in `src/Platform/N64/n64_mem.c`:
   - `ZONE_PERMANENT` — engine init, GUI bitmaps, fonts (never freed).
   - `ZONE_LEVEL` — level geometry, models, textures (bulk-freed on level unload).
   - Each zone is a fixed contiguous block carved out of the N64's 8 MB.

2. **Hook `pSithHS->alloc` / `pSithHS->free`** (set in `stdInitServices`) to use zone alloc.
   - For the menu phase, everything goes in `ZONE_PERMANENT`.
   - When a level is loaded, switch to `ZONE_LEVEL`.

3. **Size guidance (rough, 8 MB total):**
   - Kernel + libdragon: ~0.5 MB
   - Engine code (compressed): ~1.5 MB
   - `ZONE_PERMANENT` (GUI, fonts, strings): ~1 MB
   - `ZONE_LEVEL` (geometry + textures): ~4 MB
   - tiny3d display lists + RDRAM buffers: ~0.5 MB
   - Stack + misc: ~0.5 MB

### Files touched
- `src/Platform/N64/n64_mem.c` (new)
- `src/Platform/N64/n64_mem.h` (new)
- `src/stdPlatform.c` — N64 branch in `stdInitServices`

### Test
10 000 allocations of varied size do not crash. `Main_Startup` completes init without OOM.

---

## Step 5 — wuRegistry: config persistence stub

**Why here:** `Main_Startup` calls `wuRegistry_Startup` very early. On N64 there is no Windows registry and no persistent writable storage (unless we use a save pak).

### What to do

1. Replace `src/Platform/wuRegistry.c` with an N64 stub (`#ifdef TARGET_N64`) that:
   - Stores all key-value pairs in a flat in-RAM table.
   - `wuRegistry_GetString` returns hardcoded sane defaults.
   - `wuRegistry_SetString` is a no-op (or writes to a small flash/sram region if available).

2. Hardcode critical values:
   - `playerShortName` = `"Player"`
   - `gameName` = `"OpenJKDF2"`
   - Display mode: 320×240, 16bpp, no 3D accel flag.

### Files touched
- `src/Platform/wuRegistry.c` — `#ifdef TARGET_N64` in-RAM stub

### Test
`wuRegistry_Startup` returns without crash. `wuRegistry_GetString("playerShortName", ...)` returns `"Player"`.

---

## Step 6 — Display init: Windows_InitWindow on N64

**Why here:** `Main_Startup` calls `Windows_InitWindow()` which sets up the display device. This must succeed for the rest of init (GUI startup, std3D) to proceed.

### What to do

1. In `src/Win95/Windows.c` (or `Window_N64.c`), add `#ifdef TARGET_N64` path in `Windows_InitWindow()`:
   - Skip DirectDraw/SDL2 device enumeration.
   - Directly call `stdDisplay_SetupN64()` (new function) which sets:
     - `stdDisplay_g_backBuffer` to a libdragon `surface_t` wrapper.
     - Display mode struct to 320×240×16.
   - Return 1 (success).

2. `stdDisplay` on N64: the engine blits GUI elements into `stdVBuffer` (a software framebuffer). We need `stdDisplay_DrawAndFlip` to present that buffer to libdragon's `display_show`.
   - Add `src/Win95/stdDisplay.c` N64 path: `surface_t *disp = display_get(); memcpy(disp->buffer, vbuffer->data, 320*240*2); display_show(disp);`

### Files touched
- `src/Win95/Windows.c` — `#ifdef TARGET_N64` in `Windows_InitWindow`
- `src/Win95/stdDisplay.c` — N64 blit/flip path

### Test
`Windows_InitWindow()` returns 1. The engine proceeds past the display init checkpoint.

---

## Step 7 — std3D minimal: enough to not crash

**Why here:** After `Windows_InitWindow`, `Main_Startup` calls `std3D_Startup()` and then the GUI startup functions all call into std3D for texture uploads.

The current N64 `std3D.c` is all stubs that return 1/0. This is enough to not crash, but textures won't render.

### What to do (for menu phase only — full impl is Step 9)

1. Implement `std3D_AddToTextureCache` to actually store the `stdVBuffer` pointer (so GUI lookups don't get null).
2. Implement `std3D_DrawUIBitmap` and `std3D_DrawUIBitmapRGBA` as software blitters:
   - Copy pixels from `stdVBuffer` → `stdDisplay_g_backBuffer` at the given position.
   - Handle 8-bit indexed (palette) and 16-bit RGB.
   - This is the same approach as the original software renderer on PC.
3. Implement `std3D_DrawUIClearedRect` / `_RGBA` — memset a rect in the back buffer.
4. Implement `std3D_DrawMenu` — call `display_show` to present the finished frame.

This gives software-rendered menus without any 3D hardware. Perfectly fine for the menu phase.

### Files touched
- `src/Platform/N64/std3D.c` — implement UI blitting functions

### Test
A GUI startup function (`jkGuiMain_Startup`) completes without crash. The back buffer contains plausible pixel data after a `std3D_DrawMenu` call.

---

## Step 8 — stdControl: joypad → JK input events

**Why here:** Menu navigation requires at minimum D-pad/analog → mouse-cursor mapping and A/B → click.

### What to do

1. In `src/Platform/N64/stdControl.c`, implement:
   - `stdControl_Startup` — calls `joypad_init()`.
   - `stdControl_ReadJoysticks` — calls `joypad_poll()`, reads buttons and analog stick.
   - Map N64 controller → JK mouse-cursor delta (`Window_menu_mouseX/Y`) + left/right click flags.
   - Map Start → Escape (already done in `Window_MessageLoop`).
   - Map A → Enter/confirm, B → back/cancel.

2. `jkGuiRend_UpdateController` (called in `Window_MessageLoop`) reads `Window_menu_mouseX/Y` — feed the analog stick into those globals.

### Files touched
- `src/Platform/N64/stdControl.c`
- `src/Platform/N64/Window_N64.c` — analog-to-cursor mapping in `Window_MessageLoop`

### Test
Moving the analog stick moves the on-screen cursor. Pressing A fires a click event.

---

## Step 9 — GOB + resource load: get jkRes_LoadCD working

**Why here:** After all startup functions, `Main_Startup` calls `jkRes_LoadCD(0)`. This opens `JK.GOB` / `RES2.GOB`, reads the resource index, and loads the base UI resources (fonts, bitmaps, strings). Without this, nothing renders.

### What to do

1. Ensure the DFS image (Step 1) contains the extracted GOB content in the path structure JK expects (`ui/`, `misc/`, `3do/`, etc.).
2. `jkRes_LoadCD` calls `stdGob_Load("JK.GOB")` — this must succeed via the DFS shim.
3. After GOB load, the engine loads `ui/jkstrings.uni` (localization strings) and the base font bitmaps.
4. Verify string table and font load without crash.

### Files touched
- `tools/pack_n64_data.sh` — verify GOB file layout matches expected paths
- `src/Win95/stdGob.c` — DFS shim (refined from Step 1)

### Test
`jkRes_LoadCD(0)` returns 1. Font bitmaps are loaded. `jkStrings_GetUniStringWithFallback("MENU_NEWGAME")` returns the expected string.

---

## Step 10 — Main menu renders on screen

**Why here:** With filesystem, memory, display, input, and resource loading all working, `jkMain_MainShow` → `jkGuiMain_Open` should be reachable.

### What to do

1. Trace the call path after `jkRes_LoadCD`:
   ```
   Main_Startup → jkRes_LoadCD → jkSmack_SmackPlay (skip/stub) 
   → jkMain state machine → jkMain_MainShow → jkGuiMain_Open
   → jkGuiRend_DrawGUI → std3D_DrawMenu → display_show
   ```

2. **Skip cutscenes**: `jkSmack_SmackPlay` and `jkCutscene_*` must be no-ops on N64 (already stubbed — verify).

3. **jkGuiMain_Open** draws:
   - Background bitmap (`title.mat` or similar) via `std3D_DrawUIBitmap`
   - Menu item text via `stdFont_Draw*`
   - Cursor via `stdControl` mouse position

4. Each `std3D_DrawUIBitmap` call renders a software-blitted rect into the back buffer. `std3D_DrawMenu` calls `display_show` to flip.

5. Iterate: fix crashes as they appear, add missing stubs, repeat.

### Files touched
- `src/Platform/N64/std3D.c` — polish software blit
- `src/Main/jkMain.c` — possibly add `#ifdef TARGET_N64` to skip video playback wait loops
- `src/Main/jkSmack.c` — stub `jkSmack_SmackPlay` for N64

### Test
**The main menu is visible on screen.** Background image renders. Menu items ("New Game", "Load Game", etc.) are readable. Analog stick moves cursor. A-button triggers menu selection.

---

## Summary table

| Step | Goal | Key files | Testable milestone |
|------|------|-----------|-------------------|
| 1 | Filesystem (DFS + GOB shim) | `stdGob.c`, `tools/pack_n64_data.sh` | `dfs_open` succeeds |
| 2 | Engine compiles for N64 | `CMakeLists.txt` | `Main_Startup` is called |
| 3 | Endianness conversion | `sithWorld.c`, data pipeline | JKL loads without NaN crash |
| 4 | Arena memory allocator | `n64_mem.c`, `stdPlatform.c` | No OOM during init |
| 5 | wuRegistry stub | `wuRegistry.c` | Registry reads return defaults |
| 6 | Display init | `Windows.c`, `stdDisplay.c` | `Windows_InitWindow` returns 1 |
| 7 | std3D software UI blit | `Platform/N64/std3D.c` | GUI textures upload without crash |
| 8 | Controller input | `Platform/N64/stdControl.c` | Cursor moves with analog stick |
| 9 | GOB + resource load | `stdGob.c`, data pipeline | `jkRes_LoadCD` returns 1 |
| 10 | Main menu visible | `jkMain.c`, `std3D.c` | **Menu renders on screen** |

---

## Notes on ordering

- Steps 1–2 can partially overlap (compile the engine with stub FS first, fix FS second).
- Steps 3 and 4 can be done in parallel — they are independent.
- Steps 5, 6 can be done together — both are short.
- Step 7 (std3D UI blit) is the largest single chunk of new code.
- Step 9 depends on Step 1 being complete.
- Step 10 will reveal many small crashes that require mini-fixes not listed here.

---

## What comes after the menu

Once the menu renders, the next milestone is **entering a level**:
- `sithWorld_Load` — needs full endianness + arena alloc (Steps 3+4)
- `std3D` 3D rendering — implement tiny3d backend (separate document)
- Audio — `stdSound` via libdragon's `audio_init` + WAV playback
- Save/load — SRAM or Controller Pak (low priority)
