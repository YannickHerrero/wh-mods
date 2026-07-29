// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.6.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

On current Windows 11 (e.g. 23H2 / build 22631) the watermark is drawn by
`CWallpaperRenderer::PaintMonitor()` in `shell32.dll`. This mod wraps that
function and, only while it is running, suppresses the text-drawing GDI calls
(`DrawTextW` / `DrawTextExW` / `ExtTextOutW` / `TextOutW`) — the only text
`PaintMonitor` draws is the watermark, so the wallpaper is left untouched. The
dedicated watermark painters are also no-oped as a fallback for other builds.

Every suppressed draw is logged with its text, so the mod log shows exactly what
was hidden.

**Important:** The watermark repaints only when its screen region is
invalidated. After enabling the mod, force a repaint — drag a window over the
bottom-right corner and away, or click the desktop and press F5.

**Note:** This only hides the on-screen notice — it does not activate Windows.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

// Set while CWallpaperRenderer::PaintMonitor runs on this thread. Any text
// drawn during that window is the watermark, so we suppress it.
thread_local int g_inPaintMonitor = 0;

// ---- CWallpaperRenderer::PaintMonitor -----------------------------------

using CWallpaperRenderer_PaintMonitor_t =
    void(__cdecl*)(UINT, HWND, HMONITOR, HMONITOR, HDC, LPCRECT, LPCRECT, LPCRECT);
CWallpaperRenderer_PaintMonitor_t CWallpaperRenderer_PaintMonitor_orig;

void __cdecl CWallpaperRenderer_PaintMonitor_hook(
    UINT a1, HWND a2, HMONITOR a3, HMONITOR a4, HDC a5,
    LPCRECT a6, LPCRECT a7, LPCRECT a8)
{
    g_inPaintMonitor++;
    CWallpaperRenderer_PaintMonitor_orig(a1, a2, a3, a4, a5, a6, a7, a8);
    g_inPaintMonitor--;
}

// ---- Dedicated painters (fallback for other builds) ---------------------

using CWallpaperRenderer_PaintDesktopWatermarkText_t =
    void(__cdecl*)(HWND, HDC, UINT, HMONITOR*, LPCRECT, bool);
CWallpaperRenderer_PaintDesktopWatermarkText_t
    CWallpaperRenderer_PaintDesktopWatermarkText_orig;

void __cdecl CWallpaperRenderer_PaintDesktopWatermarkText_hook(
    HWND, HDC, UINT, HMONITOR*, LPCRECT, bool)
{
    Wh_Log(L"HIT PaintDesktopWatermarkText -> suppressed");
}

using CWallpaperRenderer__PaintDesktopWatermarkText_t =
    void(__cdecl*)(UINT, HDC, HMONITOR*, LPCRECT, bool);
CWallpaperRenderer__PaintDesktopWatermarkText_t
    CWallpaperRenderer__PaintDesktopWatermarkText_orig;

void __cdecl CWallpaperRenderer__PaintDesktopWatermarkText_hook(
    UINT, HDC, HMONITOR*, LPCRECT, bool)
{
    Wh_Log(L"HIT _PaintDesktopWatermarkText -> suppressed");
}

// ---- GDI text functions -------------------------------------------------

void LogText(PCWSTR fn, PCWSTR text, int len)
{
    if (!text)
    {
        Wh_Log(L"%s (in PaintMonitor) -> suppressed (null text)", fn);
        return;
    }
    if (len < 0)
        len = lstrlenW(text);
    if (len > 200)
        len = 200;
    Wh_Log(L"%s (in PaintMonitor) -> suppressed: \"%.*s\"", fn, len, text);
}

using DrawTextW_t = int(WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
DrawTextW_t DrawTextW_orig;
int WINAPI DrawTextW_hook(HDC hdc, LPCWSTR text, int cch, LPRECT rc, UINT fmt)
{
    if (g_inPaintMonitor > 0)
    {
        LogText(L"DrawTextW", text, cch);
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
        LogText(L"DrawTextExW", text, cch);
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
        LogText(L"ExtTextOutW", str, (int)c);
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
        LogText(L"TextOutW", str, c);
        return TRUE;
    }
    return TextOutW_orig(hdc, x, y, str, c);
}

// ---- Init ---------------------------------------------------------------

static BOOL HookOne(HMODULE mod, PCWSTR label,
                    const WindhawkUtils::SYMBOL_HOOK* hook)
{
    BOOL ok = WindhawkUtils::HookSymbols(mod, hook, 1);
    Wh_Log(L"HookSymbols %s: %s", label, ok ? L"OK" : L"FAILED");
    return ok;
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
    Wh_Log(L"Init");

    HMODULE hShell32 = LoadLibraryW(L"shell32.dll");
    if (!hShell32)
    {
        Wh_Log(L"Failed to load shell32.dll");
        return FALSE;
    }

    const WindhawkUtils::SYMBOL_HOOK paintMonitor = {
        { L"public: void __cdecl CWallpaperRenderer::PaintMonitor(unsigned int,struct HWND__ *,struct HMONITOR__ *,struct HMONITOR__ *,struct HDC__ *,struct tagRECT const *,struct tagRECT const *,struct tagRECT const *)" },
        &CWallpaperRenderer_PaintMonitor_orig,
        CWallpaperRenderer_PaintMonitor_hook, false };

    const WindhawkUtils::SYMBOL_HOOK wallpaperPaint = {
        { L"public: void __cdecl CWallpaperRenderer::PaintDesktopWatermarkText(struct HWND__ *,struct HDC__ *,unsigned int,struct HMONITOR__ * *,struct tagRECT const *,bool)" },
        &CWallpaperRenderer_PaintDesktopWatermarkText_orig,
        CWallpaperRenderer_PaintDesktopWatermarkText_hook, false };

    const WindhawkUtils::SYMBOL_HOOK wallpaperPaintPriv = {
        { L"private: void __cdecl CWallpaperRenderer::_PaintDesktopWatermarkText(unsigned int,struct HDC__ *,struct HMONITOR__ *,struct tagRECT const *,bool)" },
        &CWallpaperRenderer__PaintDesktopWatermarkText_orig,
        CWallpaperRenderer__PaintDesktopWatermarkText_hook, false };

    int hooked = 0;
    hooked += HookOne(hShell32, L"CWallpaperRenderer::PaintMonitor", &paintMonitor);
    hooked += HookOne(hShell32, L"CWallpaperRenderer::PaintDesktopWatermarkText", &wallpaperPaint);
    hooked += HookOne(hShell32, L"CWallpaperRenderer::_PaintDesktopWatermarkText", &wallpaperPaintPriv);

    hooked += HookApi(L"user32.dll", "DrawTextW", (void*)DrawTextW_hook, (void**)&DrawTextW_orig);
    hooked += HookApi(L"user32.dll", "DrawTextExW", (void*)DrawTextExW_hook, (void**)&DrawTextExW_orig);
    hooked += HookApi(L"gdi32.dll", "ExtTextOutW", (void*)ExtTextOutW_hook, (void**)&ExtTextOutW_orig);
    hooked += HookApi(L"gdi32.dll", "TextOutW", (void*)TextOutW_hook, (void**)&TextOutW_orig);

    Wh_Log(L"Installed %d/7 hooks", hooked);

    if (hooked == 0)
    {
        Wh_Log(L"Failed to hook anything");
        return FALSE;
    }

    return TRUE;
}
