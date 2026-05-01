# OpenJKDF2 N64 Port: Agent Contracts

This document defines the exact C-header boundaries to allow three parallel AI strike teams to operate independently.

## Team 1: Data Pipeline
**Objective:** Convert PC game assets into N64-friendly, Big-Endian ROM blocks.
- **BCOG Binary Format:** Compile textual `.cog` scripts into a binary format (BCOG) to skip parsing overhead. Define a strict struct:
  ```c
  typedef struct {
      uint32_t num_symbols;
      uint32_t num_instructions;
      // ... Big-Endian payload
  } BCOG_Header;
  ```
- **Geometry Format:** Pre-calculate BSP visibility and convert `.3do` models to fixed-point arrays suitable for RSP microcode ingestion.

## Team 2: Systems & Hardware
**Objective:** Provide the N64 Libdragon host services backend.
- **Contract Stubs:** Provide implementations for the `sithHostServices` struct:
  ```c
  void* N64_Alloc(size_t size);
  void  N64_Free(void* ptr);
  
  stdFile_t N64_FileOpen(const char* path, const char* mode);
  size_t    N64_FileRead(stdFile_t fd, void* out, size_t len);
  
  void N64_Input_Update(void);
  uint32_t N64_Input_GetState(void); // Map libdragon controller to JK keys
  ```

## Team 3: Graphics (tiny3d)
**Objective:** Gut the OpenGL rasterizer and replace it with `tiny3d`.
- **Contract Stubs:** Replace OpenJKDF2 rendering hooks (`jk_gl.c` / `rdRaster.c` equivalents).
  ```c
  void N64_RenderInit(void);
  void N64_RenderClear(void);
  void N64_RenderSector(sithSector* sector);
  void N64_DrawModel(rdModel3* model, rdMatrix34* transform);
  void N64_RenderPresent(void);
  ```
- **Constraint:** OpenGL state machine logic must be fully removed. All rendering must formulate display lists pushed to the RCP (Reality Coprocessor).