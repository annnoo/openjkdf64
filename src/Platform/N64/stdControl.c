#include "Platform/stdControl.h"

#include "Devices/sithControl.h"
#include "Win95/Window.h"
#include "stdPlatform.h"
#include "Main/jkQuakeConsole.h"
#include "Main/jkDev.h"

#include "jk.h"

#include <libdragon.h>
#include <string.h>

// ── Key / axis state ──────────────────────────────────────────────────────────
// Sparse arrays indexed by the KEY_JOY1_* and AXIS_* constants.
// KEY indices go up to ~0x10F; 512 entries is safe.

#define N64_MAX_KEYS  512
#define N64_MAX_AXES  16

static int   n64_key_state[N64_MAX_KEYS];   // 1 = held, 0 = released
static float n64_axis_val[N64_MAX_AXES];

// ── Helpers ───────────────────────────────────────────────────────────────────

static inline void set_key(int k, int v) {
    if (k >= 0 && k < N64_MAX_KEYS) n64_key_state[k] = v;
}
static inline void set_axis(int a, float v) {
    if (a >= 0 && a < N64_MAX_AXES) n64_axis_val[a] = v;
}

// ── Mandatory stubs ───────────────────────────────────────────────────────────

// N64 has no keyboard
const stdControlDikStrToNum stdControl_aDikNumToStr[JK_TOTAL_NUM_KEYS] = {0};
const char *stdControl_aAxisNames[JK_NUM_AXES+1] = {0};

int  stdControl_Startup()  { return 1; }
void stdControl_Shutdown() {}
int  stdControl_Open()     { return 1; }
int  stdControl_Close()    { return 1; }
void stdControl_Flush()    {}
void stdControl_Reset()    { memset(n64_key_state, 0, sizeof(n64_key_state)); memset(n64_axis_val, 0, sizeof(n64_axis_val)); }

int  stdControl_EnableAxis(unsigned int idx)      { return 1; }
int  stdControl_ReadAxisAsKey(int axisNum)         { return n64_axis_val[axisNum > 0.5f || n64_axis_val[axisNum] < -0.5f]; }
int  stdControl_ReadAxisRaw(int axisNum)           { return (int)(n64_axis_val[axisNum] * 32767.0f); }
void stdControl_FinishRead()                       {}
int  stdControl_MessageHandler(HWND hWnd, UINT Msg, WPARAM wParam, HWND lParam, LRESULT* unused) { return 0; }
void stdControl_SetMouseSensitivity(flex_t x, flex_t y) {}
void stdControl_SetKeydown(int k, int bDown, uint32_t t) { set_key(k, bDown); }
void stdControl_SetSDLKeydown(int k, int bDown, uint32_t t) { set_key(k, bDown); }
void stdControl_InitAxis(int idx, int mn, int mx, flex_t mul) {}
void stdControl_ToggleCursor(int a)        {}

static int n64_cursor_count = 0;
int  stdControl_ShowCursor(int a) {
    if (a) n64_cursor_count++;
    else n64_cursor_count--;
    return n64_cursor_count;
}

void stdControl_ToggleMouse()              {}
void stdControl_ReadMouse()                {}
void stdControl_ShowSystemKeyboard()       {}
void stdControl_HideSystemKeyboard()       {}
BOOL stdControl_IsSystemKeyboardShowing()  { return 0; }

// ── Axis / key reads ──────────────────────────────────────────────────────────

flex_t stdControl_ReadAxis(int axisNum) {
    if (axisNum < 0 || axisNum >= N64_MAX_AXES) return 0.0f;
    return n64_axis_val[axisNum];
}

flex_t stdControl_ReadKeyAsAxis(int keyNum) {
    if (keyNum < 0 || keyNum >= N64_MAX_KEYS) return 0.0f;
    return n64_key_state[keyNum] ? 1.0f : 0.0f;
}

int stdControl_ReadKey(int keyNum, int *pOut) {
    if (keyNum < 0 || keyNum >= N64_MAX_KEYS) { if (pOut) *pOut = 0; return 0; }
    int v = n64_key_state[keyNum];
    if (pOut) *pOut += v;
    return v;
}

// ── Main poll — called every frame by jkGuiRend_UpdateController ──────────────
// Maps N64 controller buttons to JK joystick key/axis constants.
//
// N64 layout → JK mapping:
//   Analog stick  → AXIS_JOY1_X / AXIS_JOY1_Y
//   D-pad         → KEY_JOY1_HLEFT / HUP / HRIGHT / HDOWN
//   A             → KEY_JOY1_B1   (confirm / primary fire)
//   B             → KEY_JOY1_B2   (cancel / duck)
//   Z             → KEY_JOY1_B16  (secondary fire / strafe modifier)
//   L             → KEY_JOY1_B10  (prev weapon / inv)
//   R             → KEY_JOY1_B11  (next weapon)
//   C-right       → KEY_JOY1_B3   (activate)
//   C-up/down/left→ KEY_JOY1_B4/5/6

void stdControl_ReadControls(void) {
    // joypad_poll(); // REMOVED: Now handled by N64_PumpIdle
    joypad_inputs_t  inp = joypad_get_inputs(JOYPAD_PORT_1);
    joypad_buttons_t held = joypad_get_buttons(JOYPAD_PORT_1);

    // Analog stick → axes  (N64 range ~±80; normalise to ±1)
    float ax = (float)inp.stick_x / 80.0f;
    float ay = (float)inp.stick_y / 80.0f;
    if (ax >  1.0f) ax =  1.0f;
    if (ax < -1.0f) ax = -1.0f;
    if (ay >  1.0f) ay =  1.0f;
    if (ay < -1.0f) ay = -1.0f;
    set_axis(AXIS_JOY1_X, ax);
    set_axis(AXIS_JOY1_Y, -ay);   // N64 Y+ is up; JK Y+ is down

    // D-pad → hat keys
    set_key(KEY_JOY1_HLEFT,  held.d_left);
    set_key(KEY_JOY1_HRIGHT, held.d_right);
    set_key(KEY_JOY1_HUP,    held.d_up);
    set_key(KEY_JOY1_HDOWN,  held.d_down);

    // Face / shoulder buttons
    set_key(KEY_JOY1_B1,  held.a);
    set_key(KEY_JOY1_B2,  held.b);
    set_key(KEY_JOY1_B3,  held.c_right);
    set_key(KEY_JOY1_B4,  held.c_up);
    set_key(KEY_JOY1_B5,  held.c_down);
    set_key(KEY_JOY1_B6,  held.c_left);
    set_key(KEY_JOY1_B10, held.l);
    set_key(KEY_JOY1_B11, held.r);
    set_key(KEY_JOY1_B16, held.z);
}
