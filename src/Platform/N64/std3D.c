#include "Platform/std3D.h"

#include "General/stdMath.h"
#include "General/stdBitmap.h"
#include "Win95/stdDisplay.h"
#include "stdPlatform.h"

#include <t3d/t3d.h>
#include <t3d/t3dmath.h>

static T3DViewport std3D_n64Viewport;
static T3DVertPacked n64_vbuf[256];
static int n64_vbuf_count = 0;

int std3D_bReinitHudElements = 0;

rdDDrawSurface* std3D_aLoadedSurfaces[STD3D_MAX_TEXTURES] = {0};
size_t std3D_loadedTexturesAmt = 0;

int std3D_Startup() { 
    t3d_init((T3DInitParams){0});
    std3D_n64Viewport = t3d_viewport_create();
    return 1; 
}

void std3D_Shutdown() {
    t3d_destroy();
}

int std3D_StartScene() { 
    t3d_frame_start();
    t3d_viewport_set_ortho(&std3D_n64Viewport, 0, 320, 240, 0, -100.0f, 100.0f);
    t3d_viewport_attach(&std3D_n64Viewport);
    return 1; 
}

int std3D_EndScene() { 
    t3d_tri_sync();
    return 1; 
}
void std3D_ResetRenderList() {}
int std3D_RenderListVerticesFinish() { return 1; }
void std3D_DrawRenderList() {}
int std3D_SetCurrentPalette(rdColor24 *a1, int a2) { return 1; }
void std3D_GetValidDimension(unsigned int inW, unsigned int inH, unsigned int *outW, unsigned int *outH)
{
    *outW = inW;
    *outH = inH;
}
int std3D_DrawOverlay() { return 0; }
void std3D_UnloadAllTextures() {}

static void n64_load_vertices() {
    if (n64_vbuf_count > 0) {
        // t3d_vert_load takes a max of 70 vertices at a time.
        int load_count = n64_vbuf_count > 70 ? 70 : n64_vbuf_count;
        t3d_vert_load(n64_vbuf, 0, load_count);
    }
}

#ifdef RDCACHE_RENDER_NGONS
void std3D_AddRenderListNGons(rdNGon *ngons, unsigned int num_ngons) {
    n64_load_vertices();

    for (unsigned int j = 0; j < num_ngons; j++) {
        int startIdx = ngons[j].vertIdxStart;
        int count = ngons[j].numVertices;
        
        // Draw as a fan from the first vertex
        for (int i = 1; i < count - 1; i++) {
            uint32_t v0 = startIdx;
            uint32_t v1 = startIdx + i;
            uint32_t v2 = startIdx + i + 1;
            
            // Safety bounds for first pass
            if (v0 < 70 && v1 < 70 && v2 < 70) {
                t3d_tri_draw(v0, v1, v2);
            }
        }
    }
}
#else
void std3D_AddRenderListTris(rdTri *tris, unsigned int num_tris) {
    n64_load_vertices();
    
    for (unsigned int i = 0; i < num_tris; i++) {
        // Skip indices > 69 for now until batching is implemented
        if (tris[i].v1 < 70 && tris[i].v2 < 70 && tris[i].v3 < 70) {
            t3d_tri_draw(tris[i].v1, tris[i].v2, tris[i].v3);
        }
    }
}
#endif

void std3D_AddRenderListLines(rdLine* lines, uint32_t num_lines) {}

int std3D_AddRenderListVertices(D3DVERTEX *vertex_array, int count) { 
    if (count > 256) count = 256; // Safety clamp

    n64_vbuf_count = count;

    for (int i = 0; i < count; i++) {
        D3DVERTEX* src = &vertex_array[i];
        
        int16_t* pos = t3d_vertbuffer_get_pos(n64_vbuf, i);
        // Cast the pre-projected floats to integers for the fixed point RSP coordinates
        pos[0] = (int16_t)src->x;
        pos[1] = (int16_t)src->y;
        pos[2] = (int16_t)src->z;
        
        uint8_t* rgba = t3d_vertbuffer_get_rgba(n64_vbuf, i);
        // Extract ARGB from color and pack as RGBA
        // Or default to white if we just want to see geometry
        rgba[0] = 255;
        rgba[1] = 255;
        rgba[2] = 255;
        rgba[3] = 255;
    }
    
    return 1; 
}

void std3D_UpdateFrameCount(rdDDrawSurface *pTexture) {}
void std3D_RemoveTextureFromCacheList(rdDDrawSurface *pCacheTexture) {}
void std3D_AddTextureToCacheList(rdDDrawSurface *pTexture) {}
int std3D_PurgeTextureCache(size_t size) { return 0; }
void std3D_PurgeEntireTextureCache() {}
int std3D_ClearZBuffer() { return 1; }
int std3D_AddToTextureCache(stdVBuffer *vbuf, rdDDrawSurface *texture, int is_alpha_tex, int no_alpha) { return 0; }
void std3D_DrawMenu() {}
void std3D_DrawSceneFbo() {}
void std3D_FreeResources() {}
void std3D_InitializeViewport(rdRect *viewRect) {}
int std3D_GetValidDimensions(int a1, int a2, int a3, int a4) { return 1; }
int std3D_FindClosestDevice(uint32_t index, int a2) { return 0; }
int std3D_SetRenderList(intptr_t a1) { return 0; }
intptr_t std3D_GetRenderList() { return 0; }
int std3D_CreateExecuteBuffer() { return 1; }

void std3D_PurgeUIEntry(int i, int idx) {}
void std3D_PurgeTextureEntry(int i) {}
void std3D_PurgeBitmapRefs(stdBitmap *pBitmap) {}
void std3D_PurgeSurfaceRefs(rdDDrawSurface *texture) {}
void std3D_UpdateSettings() {}
void std3D_Screenshot(const char* pFpath) {}

void std3D_ResetUIRenderList() {}
int std3D_AddBitmapToTextureCache(stdBitmap *texture, int mipIdx, int is_alpha_tex, int no_alpha) { return 0; }
void std3D_DrawUIBitmapRGBA(stdBitmap* pBmp, int mipIdx, flex_t dstX, flex_t dstY, rdRect* srcRect, flex_t scaleX, flex_t scaleY, int bAlphaOverwrite, uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a) {}
void std3D_DrawUIBitmap(stdBitmap* pBmp, int mipIdx, flex_t dstX, flex_t dstY, rdRect* srcRect, flex_t scale, int bAlphaOverwrite) {}
void std3D_DrawUIClearedRect(uint8_t palIdx, rdRect* dstRect) {}
void std3D_DrawUIClearedRectRGBA(uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a, rdRect* dstRect) {}
int std3D_IsReady() { return 1; }
int std3D_HasAlpha() { return 0; }
int std3D_HasModulateAlpha() { return 0; }
int std3D_HasAlphaFlatStippled() { return 0; }
