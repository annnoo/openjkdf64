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
static int n64_palette_logged = 0;
static bool n64_palette_ready = false;

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
        Video_menuBuffer.surface_lock_alloc = (char*)std_pHS->alloc(320 * 240);

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
    n64_palette_ready = true;
    if (!n64_palette_logged) {
        // Log a few palette entries to confirm initialization
        debugf("[N64] Master palette set: p0=(%d,%d,%d) p1=(%d,%d,%d) p15=(%d,%d,%d) p255=(%d,%d,%d)\n",
               stdDisplay_masterPalette[0].r, stdDisplay_masterPalette[0].g, stdDisplay_masterPalette[0].b,
               stdDisplay_masterPalette[1].r, stdDisplay_masterPalette[1].g, stdDisplay_masterPalette[1].b,
               stdDisplay_masterPalette[15].r, stdDisplay_masterPalette[15].g, stdDisplay_masterPalette[15].b,
               stdDisplay_masterPalette[255].r, stdDisplay_masterPalette[255].g, stdDisplay_masterPalette[255].b);
        n64_palette_logged = 1;
    }
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

    static int vbufcopy_logged = 0;
    if (!vbufcopy_logged) {
        uintptr_t srcAddr = (uintptr_t)srcPixels;
        uintptr_t dstAddr = (uintptr_t)dstPixels;
        const char* srcCache = (srcAddr >= 0xA0000000 && srcAddr < 0xC0000000) ? "uncached" : "cached";
        const char* dstCache = (dstAddr >= 0xA0000000 && dstAddr < 0xC0000000) ? "uncached" : "cached";
        debugf("[N64] VBufferCopy: src=%p (%s) dst=%p (%s) srcW=%d srcH=%d dstW=%d dstH=%d srcStride=%lu dstStride=%lu alpha=%d\n",
               srcPixels, srcCache, dstPixels, dstCache,
               (int)vbuf2->format.width, (int)vbuf2->format.height,
               (int)vbuf->format.width, (int)vbuf->format.height,
               (unsigned long)srcStride, (unsigned long)dstStride, alpha_maybe);
        debugf("[N64] VBufferCopy fmt: srcBpp=%d dstBpp=%d srcBytes=%lu dstBytes=%lu\n",
               (int)vbuf2->format.format.bpp, (int)vbuf->format.format.bpp,
               (unsigned long)vbuf2->format.width_in_bytes,
               (unsigned long)vbuf->format.width_in_bytes);
        // Sample a few source bytes
        debugf("[N64] VBufferCopy sample src[0..7]=%02x %02x %02x %02x %02x %02x %02x %02x\n",
               srcPixels[0], srcPixels[1], srcPixels[2], srcPixels[3],
               srcPixels[4], srcPixels[5], srcPixels[6], srcPixels[7]);
        vbufcopy_logged = 1;
    }

    // Detect 2x downscale: source is twice the dest size (640x480 → 320x240).
    // Apply 2:1 nearest-neighbor downsampling so the full image fills the dest.
    int scale2x = (vbuf2->format.width  == vbuf->format.width  * 2 &&
                   vbuf2->format.height == vbuf->format.height * 2);

    // Extreme Sanity Checks to catch BSS corruption or backward writes
    if ((uintptr_t)dstPixels >= 0x80100000 && (uintptr_t)dstPixels < 0x80280000) {
        debugf("[VBufCpy] ERROR: dstPixels in BSS! %p\n", dstPixels);
        return 0;
    }
    if ((int32_t)dstStride < 0 || dstStride > 2048) {
        debugf("[VBufCpy] ERROR: dstStride corrupted! %lu\n", (unsigned long)dstStride);
        return 0;
    }
    if (vbuf->format.width > 2048 || vbuf->format.height > 2048) {
        debugf("[VBufCpy] ERROR: vbuf bounds corrupted! %ldx%ld\n",
               (long)vbuf->format.width, (long)vbuf->format.height);
        return 0;
    }

    if (scale2x) {
        // Scaled blit: one dst pixel per 2x2 src block.
        int has_alpha = (alpha_maybe & 1);
        uint32_t dstW = vbuf->format.width;
        uint32_t dstH = vbuf->format.height;

        for (uint32_t j = 0; j < dstH; j++) {
            uint32_t srcJ = (rect->y / 2 + j) * 2;
            if (srcJ >= (uint32_t)vbuf2->format.height) break;
            
            uint32_t dstY = blit_y + j;
            if (dstY >= dstH) continue;

            for (uint32_t i = 0; i < dstW; i++) {
                uint32_t srcI = (rect->x / 2 + i) * 2;
                if (srcI >= (uint32_t)vbuf2->format.width) break;
                
                uint8_t pixel = srcPixels[srcI + srcJ * srcStride];
                if (!pixel && has_alpha) continue;
                
                uint32_t dstX = blit_x + i;
                if (dstX >= dstW) continue;
                
                dstPixels[dstX + dstY * dstStride] = pixel;
            }
        }
        return 1;
    }

    int has_alpha = !(vbuf2->format.width == 640) && (alpha_maybe & 1);
    uint32_t dstW = vbuf->format.width;
    uint32_t dstH = vbuf->format.height;

    for (int j = 0; j < rect->height; j++) {
        uint32_t srcY = j + rect->y;
        if (srcY >= (uint32_t)vbuf2->format.height) break;
        
        uint32_t dstY = j + blit_y;
        if (dstY >= dstH) continue;

        for (int i = 0; i < rect->width; i++) {
            uint32_t srcX = i + rect->x;
            if (srcX >= (uint32_t)vbuf2->format.width) break;

            uint32_t dstX = i + blit_x;
            if (dstX >= dstW) continue;

            uint8_t pixel = srcPixels[srcX + srcY * srcStride];
            if (!pixel && has_alpha) continue;
            
            dstPixels[dstX + dstY * dstStride] = pixel;
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
static bool n64_frame_drawn = false;

// Set by std3D_StartScene when RDPQ renders 3D this frame.
// Cleared by n64_frame_begin so it resets each frame.
extern int n64_did_3d_this_frame;

// libdragon VI state (from vi.o). Used for corruption detection.
extern uint32_t __vi_cfg[VI_REGISTERS_COUNT];
#define N64_VI_CFG_WORDS (VI_REGISTERS_COUNT)
static uint32_t n64_vi_cfg_snapshot[N64_VI_CFG_WORDS];
static int n64_vi_cfg_snapshot_valid = 0;

surface_t* stdDisplay_N64_GetCurrentSurface()
{
    return n64_current_disp;
}

void stdDisplay_N64_ClearCurrentSurface(void)
{
    n64_current_disp = NULL;
}

// ── FPS counter state ─────────────────────────────────────────────────────────
static uint32_t fps_frame_count  = 0;
static uint32_t fps_last_ms      = 0;
static float    fps_current      = 0.0f;

void n64_frame_begin(void)
{
    n64_frame_drawn = false;
    n64_did_3d_this_frame = 0;
    if (!n64_vi_cfg_snapshot_valid) {
        memcpy(n64_vi_cfg_snapshot, (const void*) __vi_cfg, sizeof(n64_vi_cfg_snapshot));
        n64_vi_cfg_snapshot_valid = 1;
        debugf("[N64] VI cfg snapshot @%p (words=%d)\n", __vi_cfg, (int)N64_VI_CFG_WORDS);
    }

    if (!n64_current_disp)
        n64_current_disp = display_get();
}

void n64_frame_end(void)
{
    static uint32_t n64_frame_end_count = 0;
    n64_frame_end_count++;

    if (n64_current_disp && (n64_frame_drawn || n64_did_3d_this_frame)) {
        if (n64_frame_end_count <= 5) {
            debugf("[N64] n64_frame_end: display_show disp=%p drawn=%d 3d=%d frame=%lu\n",
                   n64_current_disp, (int)n64_frame_drawn, (int)n64_did_3d_this_frame,
                   (unsigned long)n64_frame_end_count);
        }
        display_show(n64_current_disp);
        n64_current_disp = NULL;
    } else if (n64_frame_end_count <= 3) {
        debugf("[N64] n64_frame_end: SKIP disp=%p drawn=%d 3d=%d\n",
               n64_current_disp, (int)n64_frame_drawn, (int)n64_did_3d_this_frame);
    }

    // Update snapshot silently (VI cfg changes are expected from double-buffering)
    if (n64_vi_cfg_snapshot_valid) {
        memcpy(n64_vi_cfg_snapshot, (const void*) __vi_cfg, sizeof(n64_vi_cfg_snapshot));
    }
}

int stdDisplay_DrawAndFlipGdi(uint32_t a)
{
    static int daflip_count = 0;
    daflip_count++;
    if (daflip_count <= 3) {
        debugf("[N64] DrawAndFlipGdi #%d palette_ready=%d did_3d=%d\n",
               daflip_count, (int)n64_palette_ready, (int)n64_did_3d_this_frame);
    }

    if (!n64_palette_ready) {
        // Skip rendering until palette is initialized
        return 0;
    }
    if (n64_vi_cfg_snapshot_valid) {
        if (memcmp(n64_vi_cfg_snapshot, (const void*) __vi_cfg, sizeof(n64_vi_cfg_snapshot)) != 0) {
            // VI cfg changes during blit are expected from double-buffering; just update snapshot.
            memcpy(n64_vi_cfg_snapshot, (const void*) __vi_cfg, sizeof(n64_vi_cfg_snapshot));
        }
    }

    uint8_t* src = (uint8_t*)Video_menuBuffer.surface_lock_alloc;
    if (!src) return 0;

    static int render_path_logged = 0;
    if (!render_path_logged) {
        debugf("[N64] DrawAndFlipGdi: menuBuf=%p (%dx%d stride=%lu) palette0=(%d,%d,%d)\n",
               src,
               (int)Video_menuBuffer.format.width,
               (int)Video_menuBuffer.format.height,
               (unsigned long)Video_menuBuffer.format.width_in_bytes,
               stdDisplay_masterPalette[0].r,
               stdDisplay_masterPalette[0].g,
               stdDisplay_masterPalette[0].b);
        render_path_logged = 1;
    }

    static int logged_framebuf_stats = 0;
    static int logged_first_nonzero = 0;
    if (!logged_framebuf_stats) {
        // Sample first 256 bytes to see if buffer is blank
        int nonzero = 0;
        for (int i = 0; i < 256; i++) {
            if (src[i]) { nonzero++; }
        }
        debugf("[N64] Menu buffer sample: %d/256 nonzero bytes\n", nonzero);

        // Check if palette looks zeroed
        int pal_nonzero = 0;
        for (int i = 0; i < 256; i++) {
            if (stdDisplay_masterPalette[i].r || stdDisplay_masterPalette[i].g || stdDisplay_masterPalette[i].b) {
                pal_nonzero++;
                break;
            }
        }
        debugf("[N64] Palette nonzero? %s\n", pal_nonzero ? "yes" : "NO");
        logged_framebuf_stats = 1;
    }

    if (!logged_first_nonzero) {
        int nonzero = 0;
        for (int i = 0; i < 1024; i++) {
            if (src[i]) { nonzero++; }
        }
        if (nonzero > 0) {
            debugf("[N64] Menu buffer first nonzero sample: %d/1024 bytes\n", nonzero);
            logged_first_nonzero = 1;
        }
    }

    // Use the surface acquired at frame start; acquire lazily if called before n64_frame_begin.
    bool local_disp_acquired = false;
    if (!n64_current_disp) {
        n64_current_disp = display_get();
        local_disp_acquired = true;
    }
    surface_t *disp = n64_current_disp;
    if (!disp) return 0;

    static bool logged_once = false;
    if (!logged_once) {
        debugf("[N64] DrawAndFlipGdi: disp->buffer=%p, size=%dx%d, stride=%d\n", 
               disp->buffer, disp->width, disp->height, disp->stride);
        logged_once = true;
    }

    // NOTE: Do NOT gate on Video_modeStruct.b3DAccel here.
    // That macro reads from a hardcoded PC-era address (0x008605C0) which is
    // beyond N64 RAM and returns garbage, always triggering the skip.
    // DrawAndFlipGdi IS the 2D blit — it should always execute.
    //
    // However, when RDPQ already rendered a 3D frame this tick, skip the
    // pixel loop to avoid overwriting the RDPQ output with 8bpp menu pixels.
    if (n64_did_3d_this_frame) {
        // RDPQ has rendered the 3D scene and detached. Blit the HUD (Video_menuBuffer)
        // on top using alpha mode: skip palette index 0 (transparent), write everything else.
        // This overlays health/ammo/crosshair onto the RDPQ framebuffer via CPU.
        if (!n64_current_disp) {
            n64_current_disp = display_get();
        }
        surface_t *disp3d = n64_current_disp;
        if (disp3d && src) {
            uint32_t src_stride3d = Video_menuBuffer.format.width_in_bytes;
            if (src_stride3d == 0 || src_stride3d > 2048) src_stride3d = 320;
            uintptr_t da = (uintptr_t)disp3d->buffer;
            uintptr_t ua = ((da & 0xE0000000) == 0x80000000) ? (da | 0xA0000000) : da;
            uint16_t* dst3d = (uint16_t*)ua;
            uint32_t dsp = disp3d->stride / 2;
            for (uint32_t y = 0; y < 240; y++) {
                for (uint32_t x = 0; x < 320; x++) {
                    uint8_t idx = src[y * src_stride3d + x];
                    if (!idx) continue; // transparent
                    rdColor24 c = stdDisplay_masterPalette[idx];
                    uint16_t px = (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 3) << 6) | ((c.b >> 3) << 1) | 1);
                    dst3d[y * dsp + x] = px;
                }
            }
            data_cache_writeback_invalidate_all();
        }
        n64_frame_drawn = true;
        if (local_disp_acquired) {
            display_show(disp3d);
            n64_current_disp = NULL;
        }
        return 0;
    }

    const uint32_t DISP_W = 320;
    const uint32_t DISP_H = 240;

    uint32_t src_stride = Video_menuBuffer.format.width_in_bytes;
    if (src_stride == 0 || src_stride > 2048) src_stride = DISP_W;

    uint32_t dst_stride_pixels = disp->stride / 2;

    uintptr_t disp_addr = (uintptr_t)disp->buffer;
    uintptr_t uncached_addr = ((disp_addr & 0xE0000000) == 0x80000000) ? (disp_addr | 0xA0000000) : disp_addr;
    uint16_t* dst = (uint16_t*)uncached_addr;
    static int disp_addr_logged = 0;
    if (!disp_addr_logged) {
        const char* dispCache = (disp_addr >= 0xA0000000 && disp_addr < 0xC0000000) ? "uncached" : "cached";
        debugf("[N64] DrawAndFlipGdi: disp->buffer=%p (%s) uncached_dst=%p\n", disp->buffer, dispCache, dst);
        disp_addr_logged = 1;
    }

    // Straight 1:1 blit: game runs natively at 320×240.
    for (uint32_t y = 0; y < DISP_H; y++) {
        for (uint32_t x = 0; x < DISP_W; x++) {
            uint8_t pal_idx = src[y * src_stride + x];
            rdColor24 c = stdDisplay_masterPalette[pal_idx];
            // RGB888 → RGBA5551 (N64 native 16-bit, big-endian)
            uint16_t px = (uint16_t)(((c.r >> 3) << 11) | ((c.g >> 3) << 6) | ((c.b >> 3) << 1) | 1);
            dst[y * dst_stride_pixels + x] = px;
        }
    }

    // Ensure framebuffer contents are visible to VI.
    data_cache_writeback_invalidate_all();

    n64_frame_drawn = true;

    if (local_disp_acquired) {
        display_show(disp);
        n64_current_disp = NULL;
    }

    // ── FPS counter (bottom-right) ────────────────────────────────────────────
    // Disabled on N64: graphics_draw_text can fault if font system isn't ready.
    // Re-enable once a safe font init is guaranteed.
    // fps_frame_count++;
    // uint32_t now_ms = (uint32_t)(TIMER_MICROS_LL(timer_ticks()) / 1000);
    // if (fps_last_ms == 0) fps_last_ms = now_ms;
    // uint32_t elapsed = now_ms - fps_last_ms;
    // if (elapsed >= 500) {  // update every 0.5s
    //     fps_current  = (float)fps_frame_count * 1000.0f / (float)elapsed;
    //     fps_frame_count = 0;
    //     fps_last_ms  = now_ms;
    // }
    // {
    //     char fps_str[16];
    //     int fps_int  = (int)fps_current;
    //     int fps_frac = (int)((fps_current - fps_int) * 10.0f);
    //     sprintf(fps_str, "FPS:%d.%d", fps_int, fps_frac);
    //     graphics_set_color(graphics_make_color(255, 255, 0, 255),
    //                        graphics_make_color(0,   0,   0, 255));
    //     graphics_draw_text(disp, DISP_W - 72, DISP_H - 10, fps_str);
    // }

    return 0;
}

void stdDisplay_422A50() {}

#endif // TARGET_N64
