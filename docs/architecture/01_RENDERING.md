# Rendering Pipeline Architecture

OpenJKDF2's rendering engine, RenderDroid (rd), was designed for high efficiency on late 90s hardware. It supports both software rasterization and hardware acceleration via a thin abstraction layer.

## Visibility and Clipping

### Portal System
The engine uses a portal-based visibility system.
- Levels are divided into **Sectors**.
- Sectors are connected by **Adjoins** (portals).
- Rendering starts in the camera's sector and recursively traverses adjoins that are visible within the current view frustum.
- `sithRender_Clip` and `sithRender_KindaClip` handle this recursive traversal.

### Clipping (`rdClip.c`)
Before rasterization, geometry is clipped against:
- The 6 planes of the view frustum (Near, Far, Left, Right, Top, Bottom).
- Additional clipping planes for portals.
- RenderDroid performs most of this clipping in world or view space before sending triangles to the hardware.

## Geometry Processing

### 3D Objects (3DO)
3D models are stored in `.3do` files.
- They consist of a hierarchy of **Nodes**.
- Each node can contain a **Mesh**.
- Meshes consist of **Vertices**, **Faces**, and **Textures**.
- RenderDroid projects these vertices to screen space and applies lighting.

### Sprites and Particles
The engine supports 2D sprites (billboarded or fixed) and particle systems for effects like explosions and laser bolts.

## Hardware Abstraction Layer (`std3D.h`)

The `std3D` interface is the target for any hardware port (OpenGL, DSi, N64).

### Key Functions
- `std3D_StartScene()` / `std3D_EndScene()`: Delimits frame rendering.
- `std3D_AddRenderListVertices()`: Buffers vertex data.
- `std3D_AddRenderListTris()`: Buffers indices and material state.
- `std3D_DrawRenderList()`: Triggers the actual draw call.
- `std3D_AddToTextureCache()`: Uploads textures to the GPU.

### Texture Management
The engine primarily uses **8-bit paletted textures** (MAT files). The HAL is responsible for:
- Converting 8-bit to hardware-native formats (e.g., RGBA or hardware palettes).
- Managing a texture cache to handle cases where video memory is smaller than the total asset size.
