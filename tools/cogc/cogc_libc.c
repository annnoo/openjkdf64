/*
 * cogc_libc.c -- thin wrappers around standard C library functions
 *
 * In the normal JK binary build, _strlen, _strcmp etc. are resolved via
 * memory-mapped function pointers to the game executable.  For cogc (a
 * standalone Linux tool) we simply forward to the system libc.
 */

#include "jk.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <wchar.h>

int    _strlen(const char *str)                              { return (int)strlen(str); }
int    _strcmp(const char *a, const char *b)                 { return strcmp(a, b); }
int    _strncmp(const char *a, const char *b, size_t n)      { return strncmp(a, b, n); }
char*  _strcpy(char *dst, const char *src)                   { return strcpy(dst, src); }
char*  _strncpy(char *dst, const char *src, size_t n)        { return strncpy(dst, src, n); }
char*  _strcat(char *dst, const char *src)                   { return strcat(dst, src); }
char*  _strncat(char *dst, const char *src, size_t n)        { return strncat(dst, src, n); }
char*  _strchr(char *s, char c)                              { return strchr(s, (int)c); }
char*  _strrchr(char *s, char c)                             { return strrchr(s, (int)c); }
char*  _strtok(char *s, const char *d)                       { return strtok(s, d); }
char*  _strstr(const char *hay, const char *needle)          { return (char*)strstr(hay, needle); }
size_t _strspn(const char *s, const char *accept)            { return strspn(s, accept); }
const char* _strpbrk(const char *s, const char *accept)     { return strpbrk(s, accept); }

void*  _memcpy(void *dst, const void *src, size_t n)         { return memcpy(dst, src, n); }
void*  _memset(void *ptr, int val, size_t n)                 { return memset(ptr, val, n); }
void*  _malloc(size_t n)                                     { return malloc(n); }
void   _free(void *p)                                        { free(p); }
void*  _realloc(void *p, size_t n)                           { return realloc(p, n); }
void*  _calloc(size_t n, size_t sz)                          { return calloc(n, sz); }
void*  _memset32(void *ptr, uint32_t val, size_t n)
{
    uint32_t *p = (uint32_t*)ptr;
    for (size_t i = 0; i < n; i++) p[i] = val;
    return ptr;
}
int    _memcmp(const void *a, const void *b, size_t n)       { return memcmp(a, b, n); }

int    _sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}
int    _sprintf(char *s, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsprintf(s, fmt, ap);
    va_end(ap);
    return r;
}
int    _printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
size_t _fwrite(const void *a, size_t b, size_t c, FILE *d)   { return fwrite(a, b, c, d); }
int    _fputs(const char *a, FILE *b)                        { return fputs(a, b); }

double _atof(const char *str)   { return atof(str); }
int    _atoi(const char *str)   { return atoi(str); }
uint32_t _atol(const char *str) { return (uint32_t)atol(str); }
int    _rand()                  { return rand(); }

void   _strtolower(char *str)
{
    for (; *str; str++) *str = (char)tolower((unsigned char)*str);
}

int    __strcmpi(const char *a, const char *b)
{
    return strcasecmp(a, b);
}
int    __strnicmp(const char *a, const char *b, size_t n)
{
    return strncasecmp(a, b, n);
}

#ifndef MACOS
char   __tolower(char c) { return (char)tolower((unsigned char)c); }
#endif

int    __isspace(int c)  { return isspace(c); }
int    _iswspace(int c)  { return isspace(c); }

void   jk_exit(int a)    { exit(a); }
int    jk_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

void   jk_fatal() { abort(); }
void   jk_init()  {}

flex_t _frand() { return (flex_t)((double)rand() / RAND_MAX); }

/* wchar stubs -- not needed by the COG parser but declared in jk.h */
size_t   _wcslen(const wchar_t *s)                              { return wcslen(s); }
size_t   __wcslen(const wchar_t *s)                             { return wcslen(s); }
wchar_t* _wcscpy(wchar_t *d, const wchar_t *s)                  { return wcscpy(d, s); }
wchar_t* _wcsncpy(wchar_t *d, const wchar_t *s, size_t n)       { return wcsncpy(d, s, n); }
wchar_t* __wcsncpy(wchar_t *d, const wchar_t *s, size_t n)      { return wcsncpy(d, s, n); }
wchar_t* __wcscat(wchar_t *d, const wchar_t *s)                 { return wcscat(d, s); }
wchar_t* __wcschr(const wchar_t *s, wchar_t c)                  { return (wchar_t*)wcschr(s, c); }
wchar_t* __wcsrchr(const wchar_t *s, wchar_t c)                 { return (wchar_t*)wcsrchr(s, c); }
int      __wcscmp(const wchar_t *a, const wchar_t *b)           { return wcscmp(a, b); }
int      __wcsicmp(const wchar_t *a, const wchar_t *b)          { return wcscasecmp(a, b); }
long     jk_wcstol(const wchar_t *n, wchar_t **e, int b)        { return wcstol(n, e, b); }
int      jk_snwprintf(wchar_t *a, size_t n, const wchar_t *f, ...) { return 0; }
int      jk_MessageBeep(int a)                                   { return 0; }
int      msvc_sub_512D30(int a, int b)                           { return 0; }
int      _string_modify_idk(int c)                               { return c; }

/* Windows stubs (never called in cogc) */
int  stdGdi_GetHwnd()    { return 0; }
void jk_PostMessageA()   {}
void jk_GetCursorPos(LPPOINT p) {}
int  jk_GetUpdateRect(HWND a, LPRECT b, BOOL c) { return 0; }
void jk_BeginPaint(int a, struct tagPAINTSTRUCT *b) {}
void jk_EndPaint(HWND a, const PAINTSTRUCT *b) {}
HINSTANCE stdGdi_GetHInstance() { return 0; }
int  jk_LoadCursorA(HINSTANCE h, LPCSTR n) { return 0; }
void jk_SetCursor(int c) {}
void jk_SetFocus(int a)  {}
void jk_SetActiveWindow(int a) {}
void jk_ShowCursor(int a) {}
void jk_ValidateRect(HWND a, const RECT *b) {}
void jk_InvalidateRect(HWND a, const RECT *b, BOOL c) {}
void jk_ChangeDisplaySettingsA(int a, int b) {}
uint32_t jk_DirectDrawEnumerateA(void *a, void **b) { return 0; }
uint32_t jk_DirectDrawCreate(GUID *a, LPDIRECTDRAW *b, IUnknown *c) { return 0; }
uint32_t jk_DirectSoundCreate(LPGUID a, LPDIRECTSOUND *b, LPUNKNOWN c) { return 0; }
uint32_t jk_DirectPlayLobbyCreateA(GUID *a, void **b, IUnknown *c, LPVOID d, int e) { return 0; }
uint32_t jk_DirectInputCreateA(int a, int b, void **c, void *d) { return 0; }
uint32_t jk_CreateFileA()         { return 0; }
uint32_t jk_CreateFileMappingA()  { return 0; }
void*    jk_LocalAlloc()          { return NULL; }
uint32_t jk_MapViewOfFile()       { return 0; }
void     jk_UnmapViewOfFile(LPCVOID p) {}
void     jk_CloseHandle(HANDLE h)  {}
uint32_t jk_GetDesktopWindow()    { return 0; }
uint32_t jk_GetDC(int h)          { return 0; }
uint32_t jk_GetDeviceCaps(int h, int i) { return 0; }
uint32_t jk_WinExec(const char *a, int b) { return 0; }
void     jk_ReleaseDC(int a, int b) {}
int      jk_MessageBoxW(HWND a, LPCWSTR b, LPCWSTR c, UINT d) { return 0; }
int      jk_vsnwprintf(wchar_t *a, size_t b, const wchar_t *f, va_list l) { return 0; }
int      __vsnprintf(char *a, size_t b, const char *f, va_list l) { return vsnprintf(a, b, f, l); }
int      __snprintf(char *a, size_t b, const char *f, ...) {
    va_list ap; va_start(ap, f); int r = vsnprintf(a, b, f, ap); va_end(ap); return r;
}
