// wuRegistry_N64.c — in-RAM registry stub for Nintendo 64 port
// No filesystem available yet; returns compile-time defaults and discards writes.

#include "Platform/wuRegistry.h"
#include "jk.h"

#include <string.h>

LSTATUS wuRegistry_Startup(HKEY hKey, LPCSTR lpSubKey, BYTE *lpData)
{
    wuRegistry_bInitted = 1;
    return 0;
}

void wuRegistry_Shutdown()
{
    wuRegistry_bInitted = 0;
}

int wuRegistry_SaveInt(LPCSTR lpValueName, int val)
{
    return 1; // no-op
}

int wuRegistry_SaveFloat(LPCSTR lpValueName, flex_t val)
{
    return 1; // no-op
}

int wuRegistry_GetInt(LPCSTR lpValueName, int defaultVal)
{
    return defaultVal;
}

flex_t wuRegistry_GetFloat(LPCSTR lpValueName, flex_t defaultVal)
{
    return defaultVal;
}

int wuRegistry_SaveBool(LPCSTR lpValueName, int bVal)
{
    return 1; // no-op
}

int wuRegistry_GetBool(LPCSTR lpValueName, int bDefaultVal)
{
    return bDefaultVal;
}

int wuRegistry_SaveBytes(LPCSTR lpValueName, BYTE *lpData, DWORD cbData)
{
    return 1; // no-op
}

int wuRegistry_GetBytes(LPCSTR lpValueName, BYTE *lpData, DWORD cbData)
{
    memset(lpData, 0, cbData);
    return 0;
}

int wuRegistry_SetString(LPCSTR lpValueName, const char *lpData)
{
    return 1; // no-op
}

int wuRegistry_GetString(LPCSTR lpValueName, char* lpData, int outSize, const char *outDefault)
{
    if (lpData && outSize > 0)
    {
        if (outDefault)
            strncpy(lpData, outDefault, outSize - 1);
        else
            lpData[0] = '\0';
        lpData[outSize - 1] = '\0';
    }
    return 1;
}

int wuRegistry_SetWString(LPCSTR lpValueName, const wchar_t *lpData)
{
    return 1; // no-op
}

int wuRegistry_GetWString(LPCSTR lpValueName, wchar_t* lpData, int outSize, const wchar_t *outDefault)
{
    if (lpData && outSize > 0)
    {
        if (outDefault)
            _wcsncpy(lpData, outDefault, outSize - 1);
        else
            lpData[0] = L'\0';
        lpData[outSize - 1] = L'\0';
    }
    return 1;
}
