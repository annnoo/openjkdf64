// Window_N64.c — minimal window/event loop for Nintendo 64 port
#include "Win95/Window.h"

#include "Platform/std3D.h"
#include "Main/Main.h"
#include "Main/jkMain.h"
#include "Main/jkGame.h"
#include "Gui/jkGUI.h"
#include "Win95/stdDisplay.h"
#include "World/jkPlayer.h"
#include "Platform/stdControl.h"
#include "stdPlatform.h"
#include "Devices/sithConsole.h"
#include "Platform/wuRegistry.h"
#include "Gui/jkGUIRend.h"

#include <libdragon.h>
#include <string.h>

// ── Global state ──────────────────────────────────────────────────────────────

int Window_xSize        = 320;
int Window_ySize        = 240;
int Window_screenXSize  = 320;
int Window_screenYsSize = 240;
int Window_isHiDpi      = 0;
int Window_isFullscreen = 1;
int Window_lastXRel     = 0;
int Window_lastYRel     = 0;
int Window_lastSampleMs = 0;
int Window_bMouseLeft   = 0;
int Window_bMouseRight  = 0;
int Window_mouseWheelX  = 0;
int Window_mouseWheelY  = 0;
int Window_bShouldPopSteamKeyboard = 0;
int Window_bNeedsKeyboardFixed     = 0;
int Window_resized      = 0;
int Window_needsRecreate = 0;
int Window_mouseX       = 0;
int Window_mouseY       = 0;
int Window_lastMouseX   = 0;
int Window_lastMouseY   = 0;
int Window_xPos         = 0;
int Window_yPos         = 0;
int Window_menu_mouseX  = 0;
int Window_menu_mouseY  = 0;
int Window_bFlipRequested = 0;

extern int jkGuiBuildMulti_bRendering;

// ── Helpers ───────────────────────────────────────────────────────────────────

void Window_SetHiDpi(int val)     { Window_isHiDpi = val; }
void Window_SetFullscreen(int val){ Window_isFullscreen = val; }

void Window_RecreateSDL2Window()  {}
void Window_SdlVblank()           {}

int Window_ShowCursorUnwindowed(int a1) { return stdControl_ShowCursor(a1); }

int Window_DefaultHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, void* unused) { return 0; }

// ── N64 frame fencing ─────────────────────────────────────────────────────────
extern void n64_frame_begin(void);
extern void n64_frame_end(void);

// ── Main loop ─────────────────────────────────────────────────────────────────

void Window_Main_Loop(void)
{
    n64_frame_begin();
    mixer_try_play();       // pump audio — must be called frequently
    jkMain_GuiAdvance();
    Window_msg_main_handler(g_hWnd, WM_PAINT, 0, 0);
    mixer_try_play();       // second pump after frame work
    n64_frame_end();
}

extern void jkGuiRend_DebugTick();

int Window_MessageLoop(void)
{
    jkGuiRend_UpdateController();
    jkGuiRend_DebugTick();

    // stdControl_ReadControls() (called inside UpdateController) already polls
    // joypad. Read Start here for the Escape shortcut.
    joypad_buttons_t btn = joypad_get_buttons_pressed(JOYPAD_PORT_1);

    if (btn.start) {
        Window_msg_main_handler(g_hWnd, WM_KEYFIRST, VK_ESCAPE, 0);
        Window_msg_main_handler(g_hWnd, WM_CHAR, VK_ESCAPE, 0);
    }

    Window_msg_main_handler(g_hWnd, WM_PAINT, 0, 0);
    Window_SdlUpdate();
    
    // If we are in a menu loop, Window_Main_Loop's n64_frame_end won't be reached.
    // Force a frame end here to show the menu.
    n64_frame_end();
    n64_frame_begin();

    return 0;
}

void Window_SdlUpdate(void)
{
    if (Main_bHeadless) return;
    // N64: nothing to present — std3D handles display flip
}

// ── Entry point ───────────────────────────────────────────────────────────────

int Window_Main_Linux(int argc, char **argv)
{
    char cmdLine[1024];
    int result;

    strcpy(cmdLine, "");

    g_handler_count              = 0;
    g_thing_two_some_dialog_count = 0;
    g_should_exit                = 0;
    g_window_not_destroyed       = 0;
    g_hInstance                  = 0;
    g_nShowCmd                   = 0;

    for (int i = 1; i < argc; i++) {
        strcat(cmdLine, argv[i]);
        strcat(cmdLine, " ");
    }

    result = Main_Startup(cmdLine);
    debugf("[N64] Main_Startup returned %d\n", result);

    Window_SetFullscreen(1);
    Window_SetHiDpi(0);
    Window_resized = 1;
    Window_xSize   = 320;
    Window_ySize   = 240;

    if (!result) return result;

    std3D_FreeResources();

    g_window_not_destroyed = 1;

    Window_msg_main_handler(g_hWnd, WM_CREATE,      0, 0);
    Window_msg_main_handler(g_hWnd, WM_ACTIVATE,    2, 0);
    Window_msg_main_handler(g_hWnd, WM_ACTIVATEAPP, 1, 0);
    Window_msg_main_handler(g_hWnd, WM_SHOWWINDOW,  0, 0);
    Window_msg_main_handler(g_hWnd, WM_PAINT,       0, 0);

    while (1) {
        Window_Main_Loop();
        if (g_should_exit) break;
    }

    if (jkPlayer_bHasLoadedSettingsOnce) {
        jkPlayer_WriteConf(jkPlayer_playerShortName);
    }

    Main_Shutdown();
    return 1;
}
