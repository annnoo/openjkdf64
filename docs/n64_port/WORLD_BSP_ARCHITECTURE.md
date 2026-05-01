# World and BSP Architecture

The Sith engine uses a Portal-based BSP (Binary Space Partitioning) system. Unlike Quake, sectors are convex volumes connected by portals (adjoins), and visibility is determined dynamically by traversing adjoins from the player's sector.

## Core Hierarchy

### 1. `sithWorld`
The master structure loaded from a `.jkl` file. It holds contiguous arrays of:
- `sithSector` (Sectors)
- `sithSurface` (Surfaces)
- `sithThing` (Entities)
- `rdMaterial` (Textures)
- `rdModel3` (3D Meshes)

### 2. `sithSector`
A convex 3D volume.
- **Lighting:** Sectors track local ambient light and dynamic lighting overrides.
- **Colormap:** Defines the color palette (though N64 will likely utilize RGBA16 rather than indexed palettes, this field is critical for identifying environment hue).
- **Things List:** A linked list of `sithThing` entities currently bounded within this sector.

### 3. `sithSurface`
A polygon face belonging to a sector.
- **Adjoins:** If a surface connects to another sector, its `adjoin` pointer is non-null. The rasterizer walks through these adjoin surfaces to determine what is visible (Portal rendering).
- **Collision:** Used for raycasting and bounding-box intersection. Contains plane normals (`rdVector3`).

## N64 Porting Strategy
- **Static Memory Allocation:** The arrays in `sithWorld` must be allocated from a single N64 memory arena, rather than individual `malloc` calls.
- **Pre-computed Visibility:** While the PC engine resolves portal visibility at runtime on the CPU, the N64's VR4300 may struggle with deep portal recursion. The data pipeline should consider baking a PVS (Potentially Visible Set) into the `.jkl` binary to skip portal traversal logic entirely.
- **RSP Processing:** Sector vertices (`pWorld->vertices`) and Surface indices (`pWorld->vertexUVs`) must be interleaved into an N64 `Vtx` structure array (X, Y, Z, Flag, U, V, R, G, B, A) so `tiny3d` can efficiently load them into the RSP.