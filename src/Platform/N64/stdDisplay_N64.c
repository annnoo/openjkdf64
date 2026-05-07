// stdDisplay_N64.c — display / VBuffer implementation for Nintendo 64 port
// Adapted from stdDisplay_Twl.c with N64-specific blit path.

#ifdef TARGET_N64
#include "Win95/stdDisplay.h"

#include "stdPlatform.h"
#include "jk.h"
#include "Win95/Video.h"
#include "Win95/Window.h"
#include "General/stdColor.h"

#include <libdragon.h>
#include <string.h>

uint32_t Video_menuTexId = 0;
uint32_t Video_overlayTexId = 0;
rdColor24 stdDisplay_masterPalette[256];
int Video_bModeSet = 0;

// ── Shared top-section (normally compiled from stdDisplay.c) ─────────────────

void stdDisplay_SetGammaTable(int len, flex_d_t *table)
{
    stdDisplay_gammaTableLen = len;
    stdDisplay_paGammaTable = table;
}

uint8_t* stdDisplay_GetPalette()
{
    return (uint8_t*)stdDisplay_gammaPalette;
}

// ── N64 display implementation ────────────────────────────────────────────────

int stdDisplay_Startup()
{
    stdDisplay_bStartup = 1;
    return 1;
}

int stdDisplay_FindClosestDevice(void* a)
{
    Video_dword_866D78 = 0;
    return 0;
}

int stdDisplay_Open(int a)
{
    stdDisplay_pCurDevice = &stdDisplay_aDevices[0];
    stdDisplay_bOpen = 1;
    return 1;
}

void stdDisplay_Close()
{
    stdDisplay_bOpen = 0;
}

int stdDisplay_FindClosestMode(render_pair *a1, struct stdVideoMode *render_surface, unsigned int max_modes)
{
    Video_curMode = 0;
    stdDisplay_bPaged = 1;
    stdDisplay_bModeSet = 1;
    return 0;
}

int stdDisplay_SetMode(unsigned int modeIdx, const void *palette, int paged)
{
    uint32_t newW = 320;
    uint32_t newH = 240;

    stdDisplay_pCurVideoMode = &Video_renderSurface[modeIdx];

    stdDisplay_pCurVideoMode->format.format.bpp  = 8;
    stdDisplay_pCurVideoMode->format.width_in_pixels = newW;
    stdDisplay_pCurVideoMode->format.width       = newW;
    stdDisplay_pCurVideoMode->format.height      = newH;

    _memcpy(&Video_otherBuf.format,   &stdDisplay_pCurVideoMode->format, sizeof(Video_otherBuf.format));
    _memcpy(&Video_menuBuffer.format, &stdDisplay_pCurVideoMode->format, sizeof(Video_menuBuffer.format));

    uint32_t pitch = newW;

    Video_menuBuffer.format.width_in_bytes   = pitch;
    Video_otherBuf.format.width_in_bytes     = pitch;
    Video_menuBuffer.format.width_in_pixels  = pitch;
    Video_otherBuf.format.width_in_pixels    = pitch;
    Video_menuBuffer.format.width            = newW;
    Video_otherBuf.format.width              = newW;
    Video_menuBuffer.format.height           = newH;
    Video_otherBuf.format.height             = newH;
    Video_menuBuffer.format.format.bpp       = 8;
    Video_otherBuf.format.format.bpp         = 8;

    Video_menuBuffer.format.texture_size_in_bytes = pitch * newH;
    Video_otherBuf.format.texture_size_in_bytes   = pitch * newH;

    if (!Video_menuBuffer.surface_lock_alloc)
        Video_menuBuffer.surface_lock_alloc = (char*)std_pHS->alloc(640 * 480); // game forces minimum 640x480 internally

    Video_bModeSet = 1;
    return 1;
}

int stdDisplay_ClearRect(stdVBuffer *buf, int fillColor, rdRect *rect)
{
    return stdDisplay_VBufferFill(buf, fillColor, rect);
}

int stdDisplay_DDrawGdiSurfaceFlip()
{
    stdDisplay_DrawAndFlipGdi(0);
    return 1;
}

int stdDisplay_ddraw_waitforvblank()
{
    Window_SdlVblank();
    return 1;
}

int stdDisplay_SetMasterPalette(uint8_t* pal)
{
    rdColor24* pal24 = (rdColor24*)pal;
    if (!pal) return 0;
    memcpy(stdDisplay_masterPalette, pal24, sizeof(stdDisplay_masterPalette));
    return 1;
}

stdVBuffer* stdDisplay_VBufferNew(stdVBufferTexFmt *fmt, int create_ddraw_surface, int gpu_mem, const void* palette)
{
    stdVBuffer* out = (stdVBuffer*)std_pHS->alloc(sizeof(stdVBuffer));
    if (!out) return NULL;

    _memset(out, 0, sizeof(*out));
    _memcpy(&out->format, fmt, sizeof(out->format));

    out->format.width_in_bytes = fmt->width;
    out->format.width_in_pixels = fmt->width;
    out->format.texture_size_in_bytes = fmt->width * fmt->height;

    if (fmt->format.bpp == 16) {
        out->format.texture_size_in_bytes *= 2;
    }

    out->surface_lock_alloc = (char*)std_pHS->alloc(out->format.texture_size_in_bytes);
    if (!out->surface_lock_alloc) {
        std_pHS->free(out);
        return NULL;
    }

    return out;
}

int stdDisplay_VBufferLock(stdVBuffer *buf)
{
    if (!buf) return 0;
    if (!buf->surface_lock_alloc) return 0;
    return 1;
}

void stdDisplay_VBufferUnlock(stdVBuffer *buf)
{
    (void)buf;
}

int stdDisplay_VBufferCopy(stdVBuffer *vbuf, stdVBuffer *vbuf2, unsigned int blit_x, int blit_y, rdRect *rect, int alpha_maybe)
{
    if (!vbuf) return 1;

    rdRect fallback = {0, 0, vbuf2->format.width, vbuf2->format.height};
    if (!rect) rect = &fallback;

    rdRect dstRect = {blit_x, blit_y, rect->width, rect->height};
    rdRect srcRect = {rect->x, rect->y, rect->width, rect->height};

    uint8_t* srcPixels = vbuf2 ? (uint8_t*)vbuf2->surface_lock_alloc : NULL;
    uint8_t* dstPixels = (uint8_t*)vbuf->surface_lock_alloc;
    uint32_t srcStride  = vbuf2 ? vbuf2->format.width_in_bytes : 0;
    uint32_t dstStride  = vbuf->format.width_in_bytes;

    if (!srcPixels || !dstPixels) return 0;

    // Debug: print large blits (area > 2000 pixels) to catch bad overdraws
    static int _vcpy_dbg = 0;
    if (_vcpy_dbg < 20) {
        int area = rect->width * rect->height;
        if (area > 2000) {
            debugf("[VBufCpy#%d] src=%ux%u(s=%u) dst=%ux%u(s=%u) blit=(%u,%d) rect=(%d,%d,%d,%d) alpha=%d scale2x=%d\n",
                _vcpy_dbg++,
                vbuf2->format.width, vbuf2->format.height, srcStride,
                vbuf->format.width,  vbuf->format.height,  dstStride,
                blit_x, blit_y, rect->x, rect->y, rect->width, rect->height, alpha_maybe,
                (vbuf2->format.width == vbuf->format.width * 2 && vbuf2->format.height == vbuf->format.height * 2));
        }
    }

    // Detect 2x downscale: source is twice the dest size (640x480 → 320x240).
    // Apply 2:1 nearest-neighbor downsampling so the full image fills the dest.
    int scale2x = (vbuf2->format.width  == vbuf->format.width  * 2 &&
                   vbuf2->format.height == vbuf->format.height * 2);

    if (scale2x) {
        // Scaled blit: one dst pixel per 2x2 src block.
        // dstRect dimensions in dst coords; iterate over dst pixels.
        int dstW = vbuf->format.width  - (int)blit_x;
        int dstH = vbuf->format.height - (int)blit_y;
        if (dstW <= 0 || dstH <= 0) return 1;

        int has_alpha = (alpha_maybe & 1);
        for (int j = 0; j < dstH; j++) {
            int srcJ = srcRect.y + j * 2;
            if ((uint32_t)srcJ >= (uint32_t)vbuf2->format.height) break;
            for (int i = 0; i < dstW; i++) {
                int srcI = srcRect.x + i * 2;
                if ((uint32_t)srcI >= (uint32_t)vbuf2->format.width) break;
                uint8_t pixel = srcPixels[srcI + srcJ * srcStride];
                if (!pixel && has_alpha) continue;
                dstPixels[(i + blit_x) + (j + blit_y) * dstStride] = pixel;
            }
        }
        return 1;
    }

    int has_alpha = !(rect->width == 640) && (alpha_maybe & 1);

    for (int j = 0; j < rect->height; j++) {
        for (int i = 0; i < rect->width; i++) {
            if ((uint32_t)(i + srcRect.x) >= (uint32_t)vbuf2->format.width)  continue;
            if ((uint32_t)(j + srcRect.y) >= (uint32_t)vbuf2->format.height) continue;
            uint8_t pixel = srcPixels[(i + srcRect.x) + ((j + srcRect.y) * srcStride)];
            if (!pixel && has_alpha) continue;
            if ((uint32_t)(i + dstRect.x) >= (uint32_t)vbuf->format.width)  continue;
            if ((uint32_t)(j + dstRect.y) >= (uint32_t)vbuf->format.height) continue;
            dstPixels[(i + dstRect.x) + ((j + dstRect.y) * dstStride)] = pixel;
        }
    }
    return 1;
}

int stdDisplay_VBufferFill(stdVBuffer *vbuf, int fillColor, rdRect *rect)
{
    if (!vbuf) return 1;

    rdRect fallback = {0, 0, vbuf->format.width, vbuf->format.height};
    if (!rect) rect = &fallback;

    uint8_t* dstPixels = (uint8_t*)vbuf->surface_lock_alloc;
    uint32_t dstStride  = vbuf->format.width_in_bytes;
    uint32_t max_idx    = dstStride * vbuf->format.height;

    if (!dstPixels) return 0;

    for (int j = 0; j < rect->height; j++) {
        for (int i = 0; i < rect->width; i++) {
            uint32_t idx = (i + rect->x) + ((j + rect->y) * dstStride);
            if (idx < max_idx)
                dstPixels[idx] = (uint8_t)fillColor;
        }
    }
    return 1;
}

int stdDisplay_VBufferSetColorKey(stdVBuffer *vbuf, int color)
{
    if (!vbuf) return 1;
    vbuf->transparent_color = color;
    return 1;
}

void stdDisplay_VBufferFree(stdVBuffer *vbuf)
{
    if (!vbuf) return;
    stdDisplay_VBufferUnlock(vbuf);
    if (vbuf->surface_lock_alloc)
        std_pHS->free(vbuf->surface_lock_alloc);
    vbuf->surface_lock_alloc = NULL;
    std_pHS->free(vbuf);
}

void stdDisplay_ddraw_surface_flip2() {}

void stdDisplay_RestoreDisplayMode() {}

stdVBuffer* stdDisplay_VBufferConvertColorFormat(void* a, stdVBuffer* b)
{
    return b;
}

int stdDisplay_GammaCorrect3(int a1)
{
    return 1;
}

int stdDisplay_SetCooperativeLevel(uint32_t a) { return 0; }

// ── N64 frame presentation ────────────────────────────────────────────────────
// Convert 8bpp paletted Video_menuBuffer → RGB16 and blit to the N64 framebuffer.
//
// We acquire a display surface ONCE per main-loop iteration (n64_frame_begin /
// n64_frame_end), then reuse it for every DrawAndFlipGdi call within that frame.
// This prevents the double-buffer desync that caused every-other-frame flicker.

static surface_t *n64_current_disp = NULL;

void n64_frame_begin(void)
{
    if (!n64_current_disp)
        n64_current_disp = display_get();
}

void n64_frame_end(void)
{
    if (n64_current_disp) {
        display_show(n64_current_disp);
        n64_current_disp = NULL;
    }
}

int stdDisplay_DrawAndFlipGdi(uint32_t a)
{
    uint8_t* src = (uint8_t*)Video_menuBuffer.surface_lock_alloc;
    if (!src) return 0;

    // Use the surface acquired at frame start; acquire lazily if called before n64_frame_begin.
    if (!n64_current_disp)
        n64_current_disp = display_get();
    surface_t *disp = n64_current_disp;
    if (!disp) return 0;

    const uint32_t DISP_W = 320;
    const uint32_t DISP_H = 240;

    uint32_t src_stride = Video_menuBuffer.format.width_in_bytes;
    if (src_stride == 0 || src_stride > 2048) src_stride = DISP_W;

    uint32_t dst_stride_pixels = disp->stride / 2;

    uint16_t* dst = (uint16_t*)disp->buffer;

    // If the source buffer is 2× the display size (640×480 → 320×240), downsample 2:1.
    uint32_t src_w = Video_menuBuffer.format.width;
    uint32_t src_h = Video_menuBuffer.format.height;
    int scale2x = (src_w >= DISP_W * 2 && src_h >= DISP_H * 2);

    for (uint32_t y = 0; y < DISP_H; y++) {
        uint32_t sy = scale2x ? y * 2 : y;
        for (uint32_t x = 0; x < DISP_W; x++) {
            uint32_t sx = scale2x ? x * 2 : x;
            uint8_t pal_idx = src[sy * src_stride + sx];
            rdColor24 c = stdDisplay_masterPalette[pal_idx];
            // RGB888 → RGBA5551 (N64 native 16-bit, big-endian)
            uint16_t px = (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 3) << 6) | ((c.b >> 3) << 1) | 1);
            dst[y * dst_stride_pixels + x] = px;
        }
    }

    // display_show() is called by n64_frame_end() at the end of the main loop.
    return 0;
}

void stdDisplay_422A50() {}

#endif // TARGET_N64
