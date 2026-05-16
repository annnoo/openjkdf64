// main_n64.c — Nintendo 64 entry point for OpenJKDF2
// Initialises libdragon, then hands control to the engine via Window_Main_Linux.

#include <libdragon.h>
#include <stdio.h>
#include <string.h>

// Forward declaration — implemented in Platform/N64/Window_N64.c
int Window_Main_Linux(int argc, char **argv);

// N64 has no window manager — the app is always active/foreground.
// jkMain_GuiAdvance() gates the gameplay tick on g_app_suspended == 1,
// which the Win95 Window_Handler sets when g_app_active && g_window_active.
// Since Window_Handler is never called on N64 we set these manually.
extern uint32_t g_app_suspended;
extern uint32_t g_app_active;
extern uint32_t g_window_active;

int main(void)
{
    // Core libdragon init
    debug_init_isviewer();
    debug_init_usblog();

    extern char __bss_start[];
    extern char __bss_end[];
    debugf("[N64] main() start. BSS: %p - %p\n", __bss_start, __bss_end);

    // Display init — 320x240 16-bit, 2 buffers, no RDP gamma/filter
    display_init(RESOLUTION_320x240, DEPTH_16_BPP, 2, GAMMA_NONE, FILTERS_DISABLED);

    debugf("[N64] display_init done\n");

    // Must init DFS before any file I/O (jkStrings_Startup etc. will crash otherwise)
    dfs_init(DFS_DEFAULT_LOCATION);

    debugf("[N64] dfs_init done\n");

    // Quick sanity test: draw a red frame then continue
    {
        surface_t *fb = display_get();
        graphics_fill_screen(fb, graphics_make_color(255, 0, 0, 255));
        display_show(fb);
    }

    rdpq_init();
    joypad_init();
    timer_init();

    // N64 is always active/foreground — tell the engine so jkMain_GuiAdvance
    // reaches the gameplay tick path (which is gated on g_app_suspended == 1).
    g_app_active    = 1;
    g_window_active = 1;
    g_app_suspended = 1;

    debugf("[N64] calling Window_Main_Linux\n");

    // Hand off to the engine (returns only on exit)
    char *argv_fake[] = { "openjkdf2", NULL };
    Window_Main_Linux(1, argv_fake);

    debugf("[N64] Window_Main_Linux returned\n");

    // Should never reach here on N64
    while (1) {}
    return 0;
}
