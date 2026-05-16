#include "Platform/std3D.h"

#include "General/stdMath.h"
#include "General/stdBitmap.h"
#include "Win95/stdDisplay.h"
#include "stdPlatform.h"

#include <libdragon.h>
#include <rdpq.h>
#include <rdpq_tri.h>
#include <rdpq_mode.h>

// ── globals ───────────────────────────────────────────────────────────────────

int std3D_bReinitHudElements = 0;

rdDDrawSurface* std3D_aLoadedSurfaces[STD3D_MAX_TEXTURES] = {0};
size_t std3D_loadedTexturesAmt = 0;

// Set by std3D_StartScene, cleared by n64_frame_begin in stdDisplay_N64.c.
// Tells DrawAndFlipGdi to skip the CPU palette-blit (RDPQ wrote the frame).
int n64_did_3d_this_frame = 0;

// Track whether a 3D scene is currently open (between Start/EndScene).
static int n64_3d_scene_active = 0;

// Surface captured at StartScene, shown at EndScene.
static surface_t *n64_scene_surface = NULL;

// ── vertex scratch buffer ─────────────────────────────────────────────────────
// TRIFMT_SHADE vertex layout: {X, Y, R, G, B, A}  (6 floats, colours 0..1)

#define N64_MAX_VERTICES 1024
static float n64_vert_floats[N64_MAX_VERTICES * 6];
static int   n64_vbuf_count = 0;

// ── colour helpers ────────────────────────────────────────────────────────────
#define ARGB_A(c)  (((c) >> 24) & 0xFF)
#define ARGB_R(c)  (((c) >> 16) & 0xFF)
#define ARGB_G(c)  (((c) >>  8) & 0xFF)
#define ARGB_B(c)  (((c) >>  0) & 0xFF)

// ── lifecycle ─────────────────────────────────────────────────────────────────

void std3D_N64_SetZBuffer(int enable) {
    if (enable) {
        rdpq_mode_zbuf(true, true);
    } else {
        rdpq_mode_zbuf(false, false);
    }
}

int std3D_Startup() { 

    t3d_init((T3DInitParams){0});
    std3D_n64Viewport = t3d_viewport_create();
    return 1; 
}

void std3D_Shutdown() {}

// ── scene management ──────────────────────────────────────────────────────────

extern surface_t* stdDisplay_N64_GetCurrentSurface();

static uint32_t n64_scene_count = 0;
static uint32_t n64_total_tris  = 0;
static uint32_t n64_total_verts = 0;

int std3D_StartScene() {
    surface_t *disp = stdDisplay_N64_GetCurrentSurface();
    n64_scene_count++;

    n64_3d_scene_active  = 1;
    n64_did_3d_this_frame = 1;
    n64_scene_surface = disp;

    if (disp) {
        rdpq_attach(disp, NULL);
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_SHADE);
        rdpq_clear(RGBA32(0, 0, 0, 255));
    }
    return 1;
}

int std3D_EndScene() {
    n64_3d_scene_active = 0;

    if (n64_scene_surface) {
        rdpq_detach_wait();
        display_show(n64_scene_surface);
        n64_scene_surface = NULL;
        extern void stdDisplay_N64_ClearCurrentSurface(void);
        stdDisplay_N64_ClearCurrentSurface();
    }
    return 1;
}

// ── render list ───────────────────────────────────────────────────────────────

void std3D_ResetRenderList() {}
int  std3D_RenderListVerticesFinish() { return 1; }

void std3D_DrawRenderList() {
}

int std3D_AddRenderListVertices(D3DVERTEX *vertex_array, int count) {
    if (count > N64_MAX_VERTICES) count = N64_MAX_VERTICES;
    n64_vbuf_count = count;
    n64_total_verts += count;

    static int logged = 0;
    if (!logged && count > 0) {
        D3DVERTEX *s = &vertex_array[0];
        uint32_t c = (uint32_t)s->color;
        debugf("[N64] AddVerts first call: count=%d v0=(%.1f,%.1f,%.1f) color=0x%08lx\n",
               count, (double)s->x, (double)s->y, (double)s->z, (unsigned long)c);
        logged = 1;
    }

    for (int i = 0; i < count; i++) {
        D3DVERTEX *src = &vertex_array[i];
        float     *v   = &n64_vert_floats[i * 6];
        uint32_t   c   = (uint32_t)src->color;

        v[0] = src->x;
        v[1] = src->y;
        v[2] = ARGB_R(c) / 255.0f;
        v[3] = ARGB_G(c) / 255.0f;
        v[4] = ARGB_B(c) / 255.0f;
        v[5] = ARGB_A(c) ? (ARGB_A(c) / 255.0f) : 1.0f; // treat alpha=0 as fully opaque
    }
    return 1;
}

#ifdef RDCACHE_RENDER_NGONS
void std3D_AddRenderListNGons(rdNGon *ngons, unsigned int num_ngons) {
    if (!n64_3d_scene_active) return;
    for (unsigned int j = 0; j < num_ngons; j++) {
        int start = ngons[j].vertIdxStart;
        int count = ngons[j].numVertices;
        // Fan triangulation from first vertex
        for (int i = 1; i < count - 1; i++) {
            int i0 = start;
            int i1 = start + i;
            int i2 = start + i + 1;
            if (i0 < 0 || i0 >= n64_vbuf_count) continue;
            if (i1 < 0 || i1 >= n64_vbuf_count) continue;
            if (i2 < 0 || i2 >= n64_vbuf_count) continue;
            rdpq_triangle(&TRIFMT_SHADE,
                          &n64_vert_floats[i0 * 6],
                          &n64_vert_floats[i1 * 6],
                          &n64_vert_floats[i2 * 6]);
        }
    }
}
#else
void std3D_AddRenderListTris(rdTri *tris, unsigned int num_tris) {
    if (!n64_3d_scene_active) return;
    n64_total_tris += num_tris;

    static int logged = 0;
    if (!logged) {
        debugf("[N64] AddTris first call: num=%u vbuf_count=%d total_verts=%lu\n",
               num_tris, n64_vbuf_count, (unsigned long)n64_total_verts);
        if (num_tris > 0) {
            debugf("[N64]   tri0: v=%d,%d,%d flags=0x%x\n",
                   tris[0].v1, tris[0].v2, tris[0].v3, tris[0].flags);
        }
        logged = 1;
    }
    for (unsigned int i = 0; i < num_tris; i++) {
        int v1 = tris[i].v1;
        int v2 = tris[i].v2;
        int v3 = tris[i].v3;
        if (v1 < 0 || v1 >= n64_vbuf_count) continue;
        if (v2 < 0 || v2 >= n64_vbuf_count) continue;
        if (v3 < 0 || v3 >= n64_vbuf_count) continue;
        rdpq_triangle(&TRIFMT_SHADE,
                      &n64_vert_floats[v1 * 6],
                      &n64_vert_floats[v2 * 6],
                      &n64_vert_floats[v3 * 6]);
    }
}
#endif

void std3D_AddRenderListLines(rdLine* lines, uint32_t num_lines) {}

// ── stubs ─────────────────────────────────────────────────────────────────────

int  std3D_SetCurrentPalette(rdColor24 *a1, int a2) { return 1; }
void std3D_GetValidDimension(unsigned int inW, unsigned int inH, unsigned int *outW, unsigned int *outH)
{
    *outW = inW;
    *outH = inH;
}
int  std3D_DrawOverlay() { return 0; }
void std3D_UnloadAllTextures() {}

void std3D_UpdateFrameCount(rdDDrawSurface *pTexture) {}
void std3D_RemoveTextureFromCacheList(rdDDrawSurface *pCacheTexture) {}
void std3D_AddTextureToCacheList(rdDDrawSurface *pTexture) {}
int  std3D_PurgeTextureCache(size_t size) { return 0; }
void std3D_PurgeEntireTextureCache() {}
int  std3D_ClearZBuffer() { return 1; }
int  std3D_AddToTextureCache(stdVBuffer *vbuf, rdDDrawSurface *texture, int is_alpha_tex, int no_alpha)
{
    // Mark loaded so the game doesn't keep trying to re-upload.
    if (texture) texture->texture_loaded = 1;
    return 0;
}
void std3D_DrawMenu() {}
void std3D_DrawSceneFbo() {}
void std3D_FreeResources() {}
void std3D_InitializeViewport(rdRect *viewRect) {}
int  std3D_GetValidDimensions(int a1, int a2, int a3, int a4) { return 1; }
int  std3D_FindClosestDevice(uint32_t index, int a2) { return 0; }
int  std3D_SetRenderList(intptr_t a1) { return 0; }
intptr_t std3D_GetRenderList() { return 0; }
int  std3D_CreateExecuteBuffer() { return 1; }

void std3D_PurgeUIEntry(int i, int idx) {}
void std3D_PurgeTextureEntry(int i) {}
void std3D_PurgeBitmapRefs(stdBitmap *pBitmap) {}
void std3D_PurgeSurfaceRefs(rdDDrawSurface *texture) {}
void std3D_UpdateSettings() {}
void std3D_Screenshot(const char* pFpath) {}

void std3D_ResetUIRenderList() {}
int  std3D_AddBitmapToTextureCache(stdBitmap *texture, int mipIdx, int is_alpha_tex, int no_alpha) { return 0; }
void std3D_DrawUIBitmapRGBA(stdBitmap* pBmp, int mipIdx, flex_t dstX, flex_t dstY, rdRect* srcRect, flex_t scaleX, flex_t scaleY, int bAlphaOverwrite, uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a) {}
void std3D_DrawUIBitmap(stdBitmap* pBmp, int mipIdx, flex_t dstX, flex_t dstY, rdRect* srcRect, flex_t scale, int bAlphaOverwrite) {}
void std3D_DrawUIClearedRect(uint8_t palIdx, rdRect* dstRect) {}
void std3D_DrawUIClearedRectRGBA(uint8_t color_r, uint8_t color_g, uint8_t color_b, uint8_t color_a, rdRect* dstRect) {}
int  std3D_IsReady() { return 1; }
int  std3D_HasAlpha() { return 0; }
int  std3D_HasModulateAlpha() { return 0; }
int  std3D_HasAlphaFlatStippled() { return 0; }
