// stubs_n64.c — stub implementations for systems not available/needed on N64
// Covers: Window dispatch, Video, stdComm/DirectPlay, stdConsole, stdHttp,
//         stdMci, stdMciGOG, stdEmbeddedRes, stdJSON, stdUpdater, yyparse.

#include "types.h"
#include "Win95/Window.h"
#include "Win95/Video.h"
#include "Win95/stdDisplay.h"

#include <string.h>
#include <stdlib.h>

// Forward declarations for functions defined elsewhere
extern void Main_Shutdown();

// ── getcwd (no filesystem on N64 yet) ────────────────────────────────────────

char* getcwd(char* buf, size_t size)
{
    if (buf && size > 0) buf[0] = '\0';
    return NULL; // signals failure — callers handle this
}

// ── Window message dispatch ───────────────────────────────────────────────────
// Copied from Win95/Window.c (portable; no SDL2/Win32 dependencies).

static int dword_855E98 = 0;
static int dword_855DE4 = 0;

int Window_AddMsgHandler(WindowHandler_t a1)
{
    int i;
    for (i = 0; i < 16; i++)
        if (Window_ext_handlers[i].exists && Window_ext_handlers[i].handler == a1)
            return 1;
    for (i = 0; i < 16; i++)
        if (!Window_ext_handlers[i].exists) break;
    if (i >= 16) return 1;
    Window_ext_handlers[i].handler = a1;
    Window_ext_handlers[i].exists  = 1;
    ++g_handler_count;
    return 1;
}

int Window_RemoveMsgHandler(WindowHandler_t a1)
{
    int i;
    for (i = 0; i < 16; i++) {
        if (Window_ext_handlers[i].handler == a1) {
            Window_ext_handlers[i].handler = 0;
            Window_ext_handlers[i].exists  = 0;
            g_handler_count -= 1;
            return 1;
        }
    }
    return 1;
}

int Window_msg_main_handler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    int handler_count;
    struct wm_handler *ext_handler;
    LRESULT v10;

    switch (Msg) {
        case WM_CREATE:
            g_app_active   = 0;
            g_window_active = 0;
            break;
        case WM_DESTROY:
            g_window_not_destroyed = 0;
            Main_Shutdown();
            break;
        case WM_ACTIVATE:
            if ((uint16_t)wParam == 2 || (uint16_t)wParam == 1)
                g_window_active = 1;
            else
                g_window_active = 0;
            break;
        case WM_ACTIVATEAPP:
            g_app_active = wParam != 0;
            break;
        default:
            break;
    }

    if (!g_app_active || (g_app_suspended = 1, !g_window_active))
        g_app_suspended = 0;
    handler_count = 0;

    if (g_handler_count <= 0)
        return Window_DefaultHandler(hWnd, Msg, wParam, lParam, NULL);

    for (ext_handler = Window_ext_handlers;
         !ext_handler->exists || !ext_handler->handler(hWnd, Msg, wParam, lParam, &v10);
         ++ext_handler) {
        if (++handler_count >= g_handler_count)
            return Window_DefaultHandler(hWnd, Msg, wParam, lParam, NULL);
    }
    return v10;
}

void Window_SetDrawHandlers(WindowDrawHandler_t drawAndFlip, WindowDrawHandler_t setCoopLevel)
{
    Window_drawAndFlip          = drawAndFlip;
    Window_setCooperativeLevel  = setCoopLevel;
}

void Window_GetDrawHandlers(WindowDrawHandler_t *drawAndFlip, WindowDrawHandler_t *setCoopLevel)
{
    if (drawAndFlip)  *drawAndFlip  = Window_drawAndFlip;
    if (setCoopLevel) *setCoopLevel = Window_setCooperativeLevel;
}

// ── Video (display device management — stubbed, handled by stdDisplay_N64) ───

int  Video_Startup()        { return 1; }
void Video_Shutdown()       {}
void Video_SwitchToGDI()    {}
int  Video_camera_related() { return 1; }
int  Video_SetVideoDesc(const void *color_buf) { return 1; }

// ── stdConsole (debug console — not available on N64) ─────────────────────────

#include "Win95/stdConsole.h"
int  stdConsole_Startup(LPCSTR lpConsoleTitle, uint32_t dwWriteCoord, int a3) { return 1; }
int  stdConsole_Shutdown() { return 1; }

// ── stdMci (CD audio — using wav64 on N64) ───────────────────────────────
#include "Win95/stdMci.h"
#include <wav64.h>
#include <stdio.h>
#include <libdragon.h>

static wav64_t g_mci_wav;
static bool g_mci_open = false;
static int g_mci_ch = 0; // Use channel 0 for music
static float g_mci_vol = 1.0f;

int   stdMci_Startup()                              { return 1; }
void  stdMci_Shutdown()                             {
    if (g_mci_open) {
        wav64_close(&g_mci_wav);
        g_mci_open = false;
    }
}

int   stdMci_Play(uint8_t trackFrom, uint8_t trackTo) {
    if (g_mci_open) {
        mixer_ch_stop(g_mci_ch);
        wav64_close(&g_mci_wav);
        g_mci_open = false;
    }

    char path[64];
    // Map track number to file. JK usually starts at track 2 (track 1 is data).
    // Some mods might use different numbering.
    snprintf(path, sizeof(path), "rom:/music/track%02d.wav64", trackFrom);
    
    wav64_open(&g_mci_wav, path);
    // wav64_open doesn't return a value, but it sets wav64.handle to -1 or similar on error?
    // Actually, libdragon usually assets or debugfs errors.
    // Let's assume it works if we reached here, or check handle if available.
    g_mci_open = true;
    wav64_set_loop(&g_mci_wav, true);
    mixer_ch_set_vol_pan(g_mci_ch, g_mci_vol, 0.5f);
    wav64_play(&g_mci_wav, g_mci_ch);
    debugf("[N64] stdMci_Play: started %s\n", path);
    return 1;
}

void  stdMci_SetVolume(flex_t vol)                  {
    g_mci_vol = (float)vol;
    if (g_mci_vol < 0.0f) g_mci_vol = 0.0f;
    if (g_mci_vol > 1.0f) g_mci_vol = 1.0f;
    mixer_ch_set_vol_pan(g_mci_ch, g_mci_vol, 0.5f);
}

void  stdMci_Stop()                                 {
    if (g_mci_open) {
        mixer_ch_stop(g_mci_ch);
        wav64_close(&g_mci_wav);
        g_mci_open = false;
    }
}

int   stdMci_CheckStatus()                          { 
    return g_mci_open && mixer_ch_playing(g_mci_ch); 
}

int   stdMci_bIsGOG = 0;

// ── stdHttp (no network on N64) ───────────────────────────────────────────────

#include "Platform/Common/stdHttp.h"
void  stdHttp_Startup()  {}
void  stdHttp_Shutdown() {}

// ── stdEmbeddedRes (no embedded resources on N64) ─────────────────────────────

#include "Platform/Common/stdEmbeddedRes.h"
char* stdEmbeddedRes_LoadOnlyInternal(const char* path, size_t* outSize)
{
    if (outSize) *outSize = 0;
    return NULL;
}

// ── stdUpdater (no auto-updater on N64) ───────────────────────────────────────

#include "Platform/Common/stdUpdater.h"
void stdUpdater_StartupCvars() {}

// ── stdJSON (in-RAM no-op; no filesystem yet) ─────────────────────────────────
// Provides the stdJSON API used by jkPlayer, sithCvar, sithCommand.

#include "General/stdJSON.h"

int   stdJSON_EraseAll(const char* path)                                         { return 1; }
int   stdJSON_EraseKey(const char* path, const char* key)                        { return 1; }
int   stdJSON_SaveInt(const char* path, const char* key, int val)                { return 1; }
int   stdJSON_SaveFloat(const char* path, const char* key, flex_t val)           { return 1; }
int   stdJSON_SaveBool(const char* path, const char* key, int val)               { return 1; }
int   stdJSON_SetString(const char* path, const char* key, const char* val)      { return 1; }
int   stdJSON_GetInt(const char* path, const char* key, int def)                 { return def; }
flex_t stdJSON_GetFloat(const char* path, const char* key, flex_t def)           { return def; }
int   stdJSON_GetBool(const char* path, const char* key, int def)               { return def; }
int   stdJSON_GetString(const char* path, const char* key, char* out, int outSz, const char* def)
{
    if (out && outSz > 0) {
        if (def) strncpy(out, def, outSz - 1);
        else     out[0] = '\0';
        out[outSz - 1] = '\0';
    }
    return 1;
}
int   stdJSON_SaveBytes(const char* path, const char* key, uint8_t* data, uint32_t sz) { return 1; }
int   stdJSON_GetBytes(const char* path, const char* key, uint8_t* data, uint32_t sz)
{
    if (data) memset(data, 0, sz);
    return 0;
}
int   stdJSON_IterateKeys(const char* path, stdJSONCallback_t cb, void* pCtx) { return 0; }

// stdJSON wide-string variants (rarely used, safe to no-op)
int stdJSON_SetWString(const char* path, const char* key, const char16_t* val)  { return 1; }
int stdJSON_GetWString(const char* path, const char* key, char16_t* out, int outSz, const char16_t* def)
{
    if (out && outSz > 0) out[0] = 0;
    return 1;
}

// ── COG parser stubs (yyparse never called with COG_USE_PRECOMPILED) ──────────

#include <stdio.h>
int yyparse() { return 1; } // error = 1, meaning "parse failed"
void yyrestart(FILE *input_file) {}

// ── DirectPlay stubs (no network on N64) ──────────────────────────────────────

#include "Win95/DirectX.h"
void DirectPlay_EnumPlayers(int a) {}
void DirectPlay_SetSessionDesc(const char* a1, DWORD maxPlayers) {}
BOOL DirectPlay_SetSessionFlagidk(int a1) { return 0; }
int  DirectPlay_IdkSessionDesc(jkMultiEntry* pEntry) { return 0; }

// ── stdComm stubs (no network on N64) ─────────────────────────────────────────

#include "Win95/stdComm.h"
int  stdComm_Startup()                         { return 1; }
void stdComm_Shutdown()                        {}
int  stdComm_EarlyInit()                       { return 1; }
int  stdComm_Open(int idx, wchar_t* pwPassword){ return 0; }
int  stdComm_OpenConnection(void* a)           { return 0; }
void stdComm_CloseConnection()                 {}
int  stdComm_EnumSessions(int a, void* b)      { return 0; }
HRESULT stdComm_EnumSessions2(void)            { return 0; }
int  stdComm_Recv(sithCogMsg *msg)             { return 0; }
int  stdComm_SendToPlayer(sithCogMsg *msg, int sendto_id) { return 0; }
int  stdComm_cogMsg_HandleEnumPlayers(sithCogMsg *msg)    { return 0; }

// ── stdSound_IA3D_idk stub (A3D positional audio — no-op on N64) ─────────────

void stdSound_IA3D_idk(flex_t a) {}

// ── sithMulti stubs (multiplayer — stripped for N64) ─────────────────────────

#include "Dss/sithMulti.h"
int     sithMulti_Startup()                                              { return 1; }
void    sithMulti_Shutdown()                                             {}
int     sithMulti_ServerLeft(int32_t a, sithEventInfo* b)               { return 0; }
void    sithMulti_InitTick(uint32_t tickrate)                            {}
int     sithMulti_LobbyMessage()                                         { return 0; }
void    sithMulti_SendWelcome(int a, int playerIdx, int sendtoId)        {}
int     sithMulti_ProcessJoinLeave(sithCogMsg *msg)                      { return 0; }
int     sithMulti_ProcessLeaveJoin(sithCogMsg *msg)                      { return 0; }
int     sithMulti_SendJoinRequest(int sendto_id)                         { return 0; }
int     sithMulti_ProcessJoinRequest(sithCogMsg *msg)                    { return 0; }
void    sithMulti_SendChat(const char *pStr, int a, int b)               {}
int     sithMulti_ProcessChat(sithCogMsg *msg)                           { return 0; }
int     sithMulti_SendPing(int sendtoId)                                 { return 0; }
int     sithMulti_ProcessPing(sithCogMsg *msg)                           { return 0; }
int     sithMulti_ProcessPingResponse(sithCogMsg *msg)                   { return 0; }
void    sithMulti_SendQuit(int idx)                                      {}
int     sithMulti_ProcessQuit(sithCogMsg *msg)                           { return 0; }
void    sithMulti_HandleTimeLimit(int deltaMs)                           {}
void    sithMulti_SyncScores()                                           {}
uint32_t sithMulti_IterPlayersnothingidk(int net_id)                     { return 0; }
void    sithMulti_SetHandleridk(sithMultiHandler_t a1)                   {}
void    sithMulti_HandleDeath(sithPlayerInfo *pPlayerInfo, sithThing *pKilledThing, sithThing *pKilledByThing) {}
void    sithMulti_EndLevel(uint32_t a1, int a2)                          {}
int     sithMulti_GetSpawnIdx(sithThing *pPlayerThing)                   { return 0; }
void    sithMulti_FreeThing(int a1)                                      {}
HRESULT sithMulti_CreatePlayer(const wchar_t *a1, const wchar_t *a2, const char *a3, const char *a4, int a5, int a6, int multiModeFlags, int rate, int a9) { return 0; }
