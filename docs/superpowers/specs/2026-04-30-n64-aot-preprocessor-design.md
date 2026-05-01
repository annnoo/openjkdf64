# Spec: OpenJKDF2 N64 AOT Preprocessor (Phase 0)

This document specifies the design for the Ahead-of-Time (AOT) preprocessing tools required for the OpenJKDF2 N64 port.

## 1. Goal
Convert original Jedi Knight: Dark Forces II assets (PC format) into N64-optimized binary formats that can be directly mapped into memory on the console.

## 2. Architecture: Native C++ Utility
The preprocessor will be a standalone C++ application that shares core headers with the OpenJKDF2 source code.

### Components:
1.  **Shared Headers**: `#include "src/types.h"` and `#include "src/engine_config.h"`.
2.  **Asset Loaders**: Reuse or adapt existing engine code for:
    - GOB (Archive) extraction.
    - MAT/BM (Texture) parsing.
    - 3DO (Model) parsing.
    - JKL (Level) parsing.
    - COG (Script) compilation (via modified Flex/Bison front-end).
3.  **Binary Exporters**: Custom logic to write memory-aligned binary blobs for the N64.

## 3. Targeted Conversions

### 3.1 COG Scripts (Pre-compilation)
- **Input**: Plaintext `.cog` files.
- **Process**: Parse symbols and logic into a bytecode format.
- **Output**: Binary `.bcog` files containing the symbol table and bytecode.

### 3.2 Textures (MAT/BM)
- **Input**: 8-bit paletted `.mat` or `.bm`.
- **Process**: 
    - Convert to N64 formats (CI4, CI8, or RGBA16).
    - Pre-tile textures into 4KB TMEM-friendly chunks.
    - Generate N64 palettes.
- **Output**: Binary `.n64tex` files.

### 3.3 Models (3DO)
- **Input**: ASCII or binary `.3do`.
- **Process**: Flatten hierarchy where possible; convert vertices to N64 fixed-point format (`Vtx`).
- **Output**: Binary `.n64mod` files.

### 3.4 Levels (JKL)
- **Input**: Plaintext `.jkl`.
- **Process**: Pre-calculate static lighting and visibility (if not already done); convert string IDs to binary offsets.
- **Output**: Binary `.n64lvl` files.

## 4. Integration
The tools will be built as part of the host build process (Linux/macOS) and invoked during the ROM creation phase.

## 5. Success Criteria
- The preprocessor can successfully convert a full JK episode.
- The resulting binary files are byte-for-byte compatible with the N64 engine's struct alignment.
- Executable size on N64 is reduced by removing text parsers (Flex/Bison/String loaders).
