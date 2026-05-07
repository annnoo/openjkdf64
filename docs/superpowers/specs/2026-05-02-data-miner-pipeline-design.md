# Data Pipeline Design: Extraction & Build Rules

This document outlines the design for the N64 asset extraction and conversion pipeline, specifically for the GOB/GOO archives and media assets.

## 1. Extraction Tool: `tools/extract_gob.py`

### Purpose
To extract proprietary LucasArts GOB and GOO archives into a raw asset directory for processing.

### Architecture
- **Input:** Path to a `.gob` or `.goo` file.
- **Output:** A directory structure mirrored from the archive contents in `build/assets_raw/`.
- **Logic:**
    - Parse 12-byte header: `magic` (4 bytes), `version` (4 bytes), `entryTable_offs` (4 bytes).
    - Seek to `entryTable_offs` and read `numFiles` (4 bytes).
    - Iterate `numFiles` times to read 136-byte `stdGobEntry` structs:
        - `fileOffset` (4 bytes)
        - `fileSize` (4 bytes)
        - `fname` (128 bytes, null-terminated)
    - Extract each file by reading `fileSize` bytes from `fileOffset` and writing to the output directory.

## 2. Media Rule Generator: `tools/convert_media.py`

### Purpose
To scan extracted assets and generate build system rules for converting PC media to N64 formats using `libdragon` tools.

### Architecture
- **Input:** The `build/assets_raw/` directory.
- **Output:** A `media_rules.mk` (or CMake equivalent) file.
- **Logic:**
    - Recurse through the directory to find all `.wav` files.
    - For each `.wav`, generate a build rule that:
        - Targets `build/assets_n64/<path>/<filename>.wav64`.
        - Uses `audioconv64` with optimized N64 settings (ADPCM).
    - This allows the main `Makefile` to include these rules and handle incremental builds/parallelization.

## 3. Workflow Integration
1. User runs `extract_gob.py resource.gob`.
2. User runs `convert_media.py`.
3. User runs `make`, which consumes the generated rules to produce N64 ROM-ready assets.

## 4. Documentation: `tools/README.md`
- Detailed usage instructions for the Python scripts.
- Prerequisites (Python 3).
- Explanation of the output directory structure.