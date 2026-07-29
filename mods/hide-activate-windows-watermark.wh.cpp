// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.7.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -luxtheme
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

On current Windows 11 the watermark is drawn by
`CWallpaperRenderer::PaintMonitor()` in `shell32.dll`. This mod wraps that
function and, while it runs, suppresses the text-drawing calls it makes
(GDI `DrawTextW`/`ExtTextOutW`/... and the themed `DrawTextWithGlow` /
`DrawThemeTextEx`). The only text `PaintMonitor` draws is the watermark, so the
wallpaper is left untouched. `DrawTextWithGlow` is also logged globally because
it is used almost exclusively for this watermark.

Every intercepted draw is logged with its text, so the mod log shows exactly
what was hidden and which draw primitive produced it.

**Important:** The watermark repaints only when its screen region is
invalidated. After enabling the mod, force a repaint — drag a window over the
bottom-right corner and away, or click the desktop and press F5.

**Note:** This only hides the on-screen notice — it does not activate Windows.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

// Set while CWallpaperRenderer::PaintMonitor runs on this thread.
thread_local int g_inPaintMonitor = 0;

void LogDraw(PCWSTR fn, PCWSTR text, int len)
{
    const wchar_t* scope = g_inPaintMonitor > 0 ? L" [in PaintMonitor]" : L"";
    if (!text)
    {
        Wh_Log(L"%s%s: (null text)", fn, scope);
        return;
    }
    if (len < 0)
        len = lstrlenW(text);
    if (len > 200)
        len = 200;
    Wh_Log(L"%s%s: \"%.*s\"", fn, scope, len, text);
}

// ---- CWallpaperRenderer::PaintMonitor -----------------------------------

using CWallpaperRenderer_PaintMonitor_t =
    void(__cdecl*)(UINT, HWND, HMONITOR, HMONITOR, HDC, LPCRECT, LPCRECT, LPCRECT);
CWallpaperRenderer_PaintMonitor_t CWallpaperRenderer_PaintMonitor_orig;

void __cdecl CWallpaperRenderer_PaintMonitor_hook(
    UINT a1, HWND a2, HMONITOR a3, HMONITOR a4, HDC a5,
    LPCRECT a6, LPCRECT a7, LPCRECT a8)
{
    Wh_Log(L"PaintMonitor ENTER");
    g_inPaintMonitor++;
    CWallpaperRenderer_PaintMonitor_orig(a1, a2, a3, a4, a5, a6, a7, a8);
    g_inPaintMonitor--;
    Wh_Log(L"PaintMonitor EXIT");
}

// ---- GDI text functions (suppressed only inside PaintMonitor) -----------

using DrawTextW_t = int(WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
DrawTextW_t DrawTextW_orig;
int WINAPI DrawTextW_hook(HDC hdc, LPCWSTR text, int cch, LPRECT rc, UINT fmt)
{
    if (g_inPaintMonitor > 0)
    {
        LogDraw(L"DrawTextW", text, cch);
        return 0;
    }
    return DrawTextW_orig(hdc, text, cch, rc, fmt);
}

using DrawTextExW_t = int(WINAPI*)(HDC, LPWSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);
DrawTextExW_t DrawTextExW_orig;
int WINAPI DrawTextExW_hook(HDC hdc, LPWSTR text, int cch, LPRECT rc, UINT fmt,
                            LPDRAWTEXTPARAMS dtp)
{
    if (g_inPaintMonitor > 0)
    {
        LogDraw(L"DrawTextExW", text, cch);
        return 0;
    }
    return DrawTextExW_orig(hdc, text, cch, rc, fmt, dtp);
}

using ExtTextOutW_t = BOOL(WINAPI*)(HDC, int, int, UINT, const RECT*, LPCWSTR, UINT, const INT*);
ExtTextOutW_t ExtTextOutW_orig;
BOOL WINAPI ExtTextOutW_hook(HDC hdc, int x, int y, UINT opt, const RECT* rc,
                             LPCWSTR str, UINT c, const INT* dx)
{
    if (g_inPaintMonitor > 0)
    {
        LogDraw(L"ExtTextOutW", str, (int)c);
        return TRUE;
    }
    return ExtTextOutW_orig(hdc, x, y, opt, rc, str, c, dx);
}

using TextOutW_t = BOOL(WINAPI*)(HDC, int, int, LPCWSTR, int);
TextOutW_t TextOutW_orig;
BOOL WINAPI TextOutW_hook(HDC hdc, int x, int y, LPCWSTR str, int c)
{
    if (g_inPaintMonitor > 0)
    {
        LogDraw(L"TextOutW", str, c);
        return TRUE;
    }
    return TextOutW_orig(hdc, x, y, str, c);
}

// ---- Themed text functions ----------------------------------------------

// uxtheme ordinal 126. Logged globally (it is watermark-specific), suppressed
// inside PaintMonitor.
using DrawTextWithGlow_t = HRESULT(WINAPI*)(HDC, LPCWSTR, int, RECT*, DWORD,
                                            COLORREF, COLORREF, UINT, UINT, BOOL,
                                            void*, LPARAM);
DrawTextWithGlow_t DrawTextWithGlow_orig;
HRESULT WINAPI DrawTextWithGlow_hook(HDC hdc, LPCWSTR text, int cch, RECT* rc,
                                     DWORD flags, COLORREF crText, COLORREF crGlow,
                                     UINT radius, UINT intensity, BOOL premul,
                                     void* cb, LPARAM lp)
{
    LogDraw(L"DrawTextWithGlow", text, cch);
    if (g_inPaintMonitor > 0)
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
    if (g_inPaintMonitor > 0)
    {
        LogDraw(L"DrawThemeTextEx", text, cch);
        return S_OK;
    }
    return DrawThemeTextEx_orig(hTheme, hdc, part, state, text, cch, flags, rc, opts);
}

// ---- Init ---------------------------------------------------------------

static BOOL HookOne(HMODULE mod, PCWSTR label,
                    const WindhawkUtils::SYMBOL_HOOK* hook)
{
    BOOL ok = WindhawkUtils::HookSymbols(mod, hook, 1);
    Wh_Log(L"HookSymbols %s: %s", label, ok ? L"OK" : L"FAILED");
    return ok;
}

static BOOL HookApiPtr(void* target, PCWSTR label, void* hook, void** orig)
{
    BOOL ok = target && Wh_SetFunctionHook(target, hook, orig);
    Wh_Log(L"Hook %s: %s", label, ok ? L"OK" : L"FAILED");
    return ok;
}

BOOL Wh_ModInit(void)
{
    Wh_Log(L"Init");

    HMODULE hShell32 = LoadLibraryW(L"shell32.dll");
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    HMODULE hGdi32 = LoadLibraryW(L"gdi32.dll");
    HMODULE hUxtheme = LoadLibraryW(L"uxtheme.dll");
    if (!hShell32)
    {
        Wh_Log(L"Failed to load shell32.dll");
        return FALSE;
    }

    const WindhawkUtils::SYMBOL_HOOK paintMonitor = {
        { L"public: void __cdecl CWallpaperRenderer::PaintMonitor(unsigned int,struct HWND__ *,struct HMONITOR__ *,struct HMONITOR__ *,struct HDC__ *,struct tagRECT const *,struct tagRECT const *,struct tagRECT const *)" },
        &CWallpaperRenderer_PaintMonitor_orig,
        CWallpaperRenderer_PaintMonitor_hook, false };

    int hooked = 0;
    hooked += HookOne(hShell32, L"CWallpaperRenderer::PaintMonitor", &paintMonitor);

    hooked += HookApiPtr(hUser32 ? (void*)GetProcAddress(hUser32, "DrawTextW") : nullptr,
                         L"DrawTextW", (void*)DrawTextW_hook, (void**)&DrawTextW_orig);
    hooked += HookApiPtr(hUser32 ? (void*)GetProcAddress(hUser32, "DrawTextExW") : nullptr,
                         L"DrawTextExW", (void*)DrawTextExW_hook, (void**)&DrawTextExW_orig);
    hooked += HookApiPtr(hGdi32 ? (void*)GetProcAddress(hGdi32, "ExtTextOutW") : nullptr,
                         L"ExtTextOutW", (void*)ExtTextOutW_hook, (void**)&ExtTextOutW_orig);
    hooked += HookApiPtr(hGdi32 ? (void*)GetProcAddress(hGdi32, "TextOutW") : nullptr,
                         L"TextOutW", (void*)TextOutW_hook, (void**)&TextOutW_orig);

    // DrawTextWithGlow is exported by ordinal 126.
    hooked += HookApiPtr(hUxtheme ? (void*)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(126)) : nullptr,
                         L"DrawTextWithGlow", (void*)DrawTextWithGlow_hook,
                         (void**)&DrawTextWithGlow_orig);
    hooked += HookApiPtr(hUxtheme ? (void*)GetProcAddress(hUxtheme, "DrawThemeTextEx") : nullptr,
                         L"DrawThemeTextEx", (void*)DrawThemeTextEx_hook,
                         (void**)&DrawThemeTextEx_orig);

    Wh_Log(L"Installed %d/7 hooks", hooked);

    if (hooked == 0)
    {
        Wh_Log(L"Failed to hook anything");
        return FALSE;
    }

    return TRUE;
}
