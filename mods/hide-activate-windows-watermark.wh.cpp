// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         2.1.0-diag
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luxtheme
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark (diagnostic build)

Temporary diagnostic build. It still suppresses the watermark (English text
containing "activate windows"), but also LOGS the Windows build number, whether
each hook installs, and any text draw whose content contains "activate" - so we
can see if a Windows update changed the watermark wording or moved its render
path off DrawTextExW.

Logging is capped (about 60 lines) so it does not flood.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>
#include <uxtheme.h>

int g_logLeft = 60;

// Case-insensitive ASCII substring search. needleLower must be lowercase.
static bool ContainsCI(PCWSTR text, int len, const wchar_t* needleLower, int nlen)
{
    if (!text)
        return false;
    if (len < 0)
        len = lstrlenW(text);
    for (int i = 0; i + nlen <= len; i++)
    {
        int j = 0;
        for (; j < nlen; j++)
        {
            wchar_t c = text[i + j];
            if (c >= L'A' && c <= L'Z')
                c = (wchar_t)(c + 32);
            if (c != needleLower[j])
                break;
        }
        if (j == nlen)
            return true;
    }
    return false;
}

// Broad match for diagnostics: any text mentioning "activate".
static bool IsActivate(PCWSTR text, int len)
{
    return ContainsCI(text, len, L"activate", 8);
}

// Strict match used for actual suppression.
static bool IsWatermark(PCWSTR text, int len)
{
    return ContainsCI(text, len, L"activate windows", 16);
}

static void MaybeLog(PCWSTR fn, PCWSTR text, int len, UINT fmt)
{
    if (g_logLeft <= 0 || !IsActivate(text, len))
        return;
    g_logLeft--;
    if (len < 0)
        len = lstrlenW(text);
    if (len > 160)
        len = 160;
    Wh_Log(L"%s: fmt=0x%X text=\"%.*s\"", fn, fmt, len, text);
}

// ---- Hooked text renderers ----------------------------------------------

using DrawTextExW_t = int(WINAPI*)(HDC, LPWSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);
DrawTextExW_t DrawTextExW_orig;
int WINAPI DrawTextExW_hook(HDC hdc, LPWSTR text, int cch, LPRECT rc, UINT fmt,
                            LPDRAWTEXTPARAMS dtp)
{
    MaybeLog(L"DrawTextExW", text, cch, fmt);
    if (!(fmt & DT_CALCRECT) && IsWatermark(text, cch))
        return 0;
    return DrawTextExW_orig(hdc, text, cch, rc, fmt, dtp);
}

using DrawTextW_t = int(WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
DrawTextW_t DrawTextW_orig;
int WINAPI DrawTextW_hook(HDC hdc, LPCWSTR text, int cch, LPRECT rc, UINT fmt)
{
    MaybeLog(L"DrawTextW", text, cch, fmt);
    if (!(fmt & DT_CALCRECT) && IsWatermark(text, cch))
        return 0;
    return DrawTextW_orig(hdc, text, cch, rc, fmt);
}

using ExtTextOutW_t = BOOL(WINAPI*)(HDC, int, int, UINT, const RECT*, LPCWSTR, UINT, const INT*);
ExtTextOutW_t ExtTextOutW_orig;
BOOL WINAPI ExtTextOutW_hook(HDC hdc, int x, int y, UINT opt, const RECT* rc,
                             LPCWSTR str, UINT c, const INT* dx)
{
    MaybeLog(L"ExtTextOutW", str, (int)c, 0);
    if (IsWatermark(str, (int)c))
        return TRUE;
    return ExtTextOutW_orig(hdc, x, y, opt, rc, str, c, dx);
}

using DrawTextWithGlow_t = HRESULT(WINAPI*)(HDC, LPCWSTR, int, RECT*, DWORD,
                                            COLORREF, COLORREF, UINT, UINT, BOOL,
                                            void*, LPARAM);
DrawTextWithGlow_t DrawTextWithGlow_orig;
HRESULT WINAPI DrawTextWithGlow_hook(HDC hdc, LPCWSTR text, int cch, RECT* rc,
                                     DWORD flags, COLORREF crText, COLORREF crGlow,
                                     UINT radius, UINT intensity, BOOL premul,
                                     void* cb, LPARAM lp)
{
    MaybeLog(L"DrawTextWithGlow", text, cch, 0);
    if (IsWatermark(text, cch))
        return S_OK;
    return DrawTextWithGlow_orig(hdc, text, cch, rc, flags, crText, crGlow,
                                 radius, intensity, premul, cb, lp);
}

using DrawThemeTextEx_t = HRESULT(WINAPI*)(HTHEME, HDC, int, int, LPCWSTR, int,
                                           DWORD, LPRECT, const DTTOPTS*);
DrawThemeTextEx_t DrawThemeTextEx_orig;
HRESULT WINAPI DrawThemeTextEx_hook(HTHEME hTheme, HDC hdc, int part, int state,
                                    LPCWSTR text, int cch, DWORD flags, LPRECT rc,
                                    const DTTOPTS* opts)
{
    MaybeLog(L"DrawThemeTextEx", text, cch, flags);
    if (IsWatermark(text, cch))
        return S_OK;
    return DrawThemeTextEx_orig(hTheme, hdc, part, state, text, cch, flags, rc, opts);
}

// ---- Init ---------------------------------------------------------------

static void LogWindowsBuild(void)
{
    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    auto p = hNtdll ? (void(WINAPI*)(DWORD*, DWORD*, DWORD*))GetProcAddress(
                          hNtdll, "RtlGetNtVersionNumbers")
                    : nullptr;
    if (!p)
    {
        Wh_Log(L"Windows build: unknown");
        return;
    }
    DWORD major = 0, minor = 0, build = 0;
    p(&major, &minor, &build);
    Wh_Log(L"Windows version: %u.%u build %u", major, minor, build & 0x0FFFFFFF);
}

static BOOL HookApi(PCWSTR dll, PCSTR name, void* hook, void** orig)
{
    HMODULE mod = LoadLibraryW(dll);
    void* target = mod ? (void*)GetProcAddress(mod, name) : nullptr;
    BOOL ok = target && Wh_SetFunctionHook(target, hook, orig);
    Wh_Log(L"Hook %S: %s", name, ok ? L"OK" : L"FAILED");
    return ok;
}

BOOL Wh_ModInit(void)
{
    Wh_Log(L"Init (diagnostic build)");
    LogWindowsBuild();

    int hooked = 0;
    hooked += HookApi(L"user32.dll", "DrawTextExW", (void*)DrawTextExW_hook, (void**)&DrawTextExW_orig);
    hooked += HookApi(L"user32.dll", "DrawTextW", (void*)DrawTextW_hook, (void**)&DrawTextW_orig);
    hooked += HookApi(L"gdi32.dll", "ExtTextOutW", (void*)ExtTextOutW_hook, (void**)&ExtTextOutW_orig);

    HMODULE hUx = LoadLibraryW(L"uxtheme.dll");
    hooked += HookApi(L"uxtheme.dll", "DrawThemeTextEx", (void*)DrawThemeTextEx_hook, (void**)&DrawThemeTextEx_orig);
    // DrawTextWithGlow is exported by ordinal 126.
    {
        void* target = hUx ? (void*)GetProcAddress(hUx, MAKEINTRESOURCEA(126)) : nullptr;
        BOOL ok = target && Wh_SetFunctionHook(target, (void*)DrawTextWithGlow_hook,
                                               (void**)&DrawTextWithGlow_orig);
        Wh_Log(L"Hook DrawTextWithGlow: %s", ok ? L"OK" : L"FAILED");
        hooked += ok;
    }

    Wh_Log(L"Installed %d/5 hooks", hooked);
    return hooked > 0;
}
