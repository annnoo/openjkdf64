# Data Pipeline Requirements (N64)

The vanilla OpenJKDF2 repository loads PC assets directly. For the N64, a robust offline data pipeline is mandatory to ensure the C engine code can operate on bare metal.

## 1. Endianness Pre-Baking
The N64 VR4300 is Big-Endian. All binary assets must be pre-swapped by an offline Python script before being bundled into the `.dfs` ROM filesystem.
- **Targets:** `.3do` (3D Models), `.cmp` (Colormaps), `.mat` (Materials), `.jkl` (Level Geometry).
- **Rule:** Every 32-bit integer, 16-bit short, and 32-bit float within these structs must have its byte order reversed. If this is not done offline, the N64 CPU will waste critical cycles doing `BSWAP` instructions during load screens.

## 2. Model and Geometry Conversion
- **N64 `Vtx` Struct:** The RCP requires vertices to be formatted as exactly 16 bytes: `x, y, z (shorts), flag (short), s, t (shorts), r, g, b, a (bytes)`.
- **Pipeline Task:** The `.3do` parser must be gutted. Instead, the offline pipeline will parse the `.3do`, normalize the floating-point vertices into fixed-point shorts, apply the material colors, and spit out raw arrays of N64 `Vtx` structs. The C code will merely `dfs_read()` these arrays directly into RAM.

## 3. Texture Compression
- PC `.mat` files are indexed 8-bit bitmaps mapping to a `.cmp` colormap.
- **Pipeline Task:** Convert `.mat` files into native N64 format (RGBA16, CI8, or CI4). Use `libdragon`'s `mksprite` tool to generate `.sprite` files, which the N64 hardware texture cache (TMEM) can ingest natively.

## 4. Archive Extraction
- The engine uses `.gob` files, which are uncompressed ZIP-like archives.
- **Pipeline Task:** Completely eliminate `.gob` reading at runtime. The build script must extract `jk1.gob` on the host PC and repack the individual files into the libdragon `.dfs` filesystem. This turns a complex archive extraction into a simple, hardware-accelerated DMA read (`dfs_read`).