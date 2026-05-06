# OpenJKDF2-N64 Asset Pipeline

This directory contains tools for preparing game assets for the N64 port.
The full pipeline goes from a licensed copy of JK:DF2 to a ready-to-package
`tools/out/` directory that the N64 ROM build consumes.

## Requirements

- Python 3.x
- `audioconv64` (part of the libdragon toolchain, must be in your PATH)
- CMake + a native C compiler (for `cogc`)

---

## Step-by-step

### 1. Extract game archives

Extract `resource.gob` (and `Res2.gob`, episode `.gob` files, etc.) into the
raw asset tree under `tools/out/`.

```bash
python3 tools/extract_gob.py path/to/resource.gob tools/out
python3 tools/extract_gob.py path/to/Res2.gob     tools/out
# repeat for each .gob / .goo file
```

This populates `tools/out/cog/`, `tools/out/3do/`, `tools/out/mat/`, etc.

### 2. Prepare audio conversion

Scan the extracted files for audio and generate a conversion script.

```bash
python3 tools/convert_media.py tools/out
```

### 3. Convert audio to N64 format

Execute the generated script to convert `.wav` files to N64-compatible
`.wav64` format via `audioconv64`.

```bash
./convert_audio.sh
```

### 4. Compile COG scripts

All `.cog` scripts under `tools/out/cog/` are compiled to binary `.bcog`
files automatically as part of the CMake build.  Configure once, then every
subsequent `cmake --build` re-compiles any changed scripts:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake_modules/toolchain_native.cmake
cmake --build build --target compile_cogs
# or just: cmake --build build   (compile_cogs is part of ALL)
```

Output `.bcog` files are written next to their source `.cog` files:

```
tools/out/cog/
  weap_bryar.cog      <- JK source (not shipped)
  weap_bryar.bcog     <- binary, consumed by the N64 runtime
  ...
```

The N64 runtime loads `.bcog` directly when built with
`-DCOG_USE_PRECOMPILED`.  It hard-fails if a `.bcog` is missing — there is
no fallback to the text parser on device.

### 5. Package for ROM

Once all the above steps are done, `tools/out/` contains everything needed.
Point the N64 ROM's filesystem image builder (DragonFS / mkdfs or similar)
at `tools/out/` to bundle all assets into the ROM:

```bash
mkdfs tools/out.dfs tools/out
```

Then link `tools/out.dfs` into the final `.z64` ROM as usual via the
libdragon CMake rules.

---

## Directory layout after the full pipeline

```
tools/out/
  cog/        COG scripts (.cog) + compiled bytecode (.bcog)
  3do/        3D object models
  jkl/        Level files
  mat/        Material/texture files
  misc/       Miscellaneous data files
  sound/      Audio converted to .wav64
  ui/         UI graphics
  voice/      Voice clips converted to .wav64
```

---

## Archive format (GOB/GOO)

`extract_gob.py` parses the LucasArts GOB/GOO format:

- **Magic:** `GOB ` or `GOO `
- **Version:** 20 (0x14)
- **Indexing:** Offset-based file entry table with 128-byte filenames.
- **Legacy handling:** Strips the `0x1A` EOF byte from extracted files.
