// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.3.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

The watermark text is painted directly on the desktop. Which function does the
painting depends on the Windows version, so this mod hooks every known painter
and makes it do nothing:

- `CWallpaperRenderer::PaintDesktopWatermarkText()` — the painter used on
  current Windows 11 (e.g. 23H2 / build 22631). **This is the one that matters
  on modern builds.**
- `CDesktopWatermark::s_DesktopBuildPaint()` — the painter on older builds.
- `CDesktopWatermark::s_WantWatermark()` — the visibility gate on older builds,
  forced to return `false`.

Each hook is installed independently; symbols that don't exist on your build are
skipped, and the mod logs which ones resolved.

**Note:** This only hides the on-screen notice — it does not activate Windows.

After enabling the mod, restart Windows Explorer (Task Manager → *Windows
Explorer* → Restart) to force a fresh desktop paint.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

// Older builds: visibility gate.
using CDesktopWatermark_s_WantWatermark_t = bool(__cdecl*)(void);
CDesktopWatermark_s_WantWatermark_t CDesktopWatermark_s_WantWatermark_orig;

// Older builds: watermark painter.
using CDesktopWatermark_s_DesktopBuildPaint_t = void(__cdecl*)(HDC, LPCRECT, HFONT);
CDesktopWatermark_s_DesktopBuildPaint_t CDesktopWatermark_s_DesktopBuildPaint_orig;

// Current Windows 11 (23H2 / build 22631, etc.): watermark painter.
using CWallpaperRenderer_PaintDesktopWatermarkText_t =
    void(__cdecl*)(HWND, HDC, UINT, HMONITOR*, LPCRECT, bool);
CWallpaperRenderer_PaintDesktopWatermarkText_t
    CWallpaperRenderer_PaintDesktopWatermarkText_orig;

bool __cdecl CDesktopWatermark_s_WantWatermark_hook(void)
{
    Wh_Log(L"s_WantWatermark -> false");
    return false;
}

void __cdecl CDesktopWatermark_s_DesktopBuildPaint_hook(HDC, LPCRECT, HFONT)
{
    Wh_Log(L"s_DesktopBuildPaint -> suppressed");
    // Do not call the original: this prevents the watermark from being drawn.
}

void __cdecl CWallpaperRenderer_PaintDesktopWatermarkText_hook(
    HWND, HDC, UINT, HMONITOR*, LPCRECT, bool)
{
    Wh_Log(L"PaintDesktopWatermarkText -> suppressed");
    // Do not call the original: this prevents the watermark from being drawn.
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

    const WindhawkUtils::SYMBOL_HOOK wallpaperPainterHook[] = {
        {
            {
                L"public: void __cdecl CWallpaperRenderer::PaintDesktopWatermarkText(struct HWND__ *,struct HDC__ *,unsigned int,struct HMONITOR__ * *,struct tagRECT const *,bool)"
            },
            &CWallpaperRenderer_PaintDesktopWatermarkText_orig,
            CWallpaperRenderer_PaintDesktopWatermarkText_hook,
            false
        },
    };

    const WindhawkUtils::SYMBOL_HOOK buildPaintHook[] = {
        {
            {
                L"private: static void __cdecl CDesktopWatermark::s_DesktopBuildPaint(struct HDC__ *,struct tagRECT const *,struct HFONT__ *)",
                L"private: static void __cdecl CDesktopWatermark::s_DesktopBuildPaint(struct HDC__ *,struct tagRECT const *,struct HFONT__ *,bool)"
            },
            &CDesktopWatermark_s_DesktopBuildPaint_orig,
            CDesktopWatermark_s_DesktopBuildPaint_hook,
            false
        },
    };

    const WindhawkUtils::SYMBOL_HOOK wantWatermarkHook[] = {
        {
            {
                L"public: static bool __cdecl CDesktopWatermark::s_WantWatermark(void)"
            },
            &CDesktopWatermark_s_WantWatermark_orig,
            CDesktopWatermark_s_WantWatermark_hook,
            false
        },
    };

    // Install each hook independently: builds only have a subset of these
    // symbols, and a missing one must not disable the others.
    BOOL wallpaperHooked = WindhawkUtils::HookSymbols(
        hShell32, wallpaperPainterHook, ARRAYSIZE(wallpaperPainterHook));
    Wh_Log(L"HookSymbols PaintDesktopWatermarkText: %s",
           wallpaperHooked ? L"OK" : L"FAILED");

    BOOL paintHooked = WindhawkUtils::HookSymbols(
        hShell32, buildPaintHook, ARRAYSIZE(buildPaintHook));
    Wh_Log(L"HookSymbols s_DesktopBuildPaint: %s", paintHooked ? L"OK" : L"FAILED");

    BOOL wantHooked = WindhawkUtils::HookSymbols(
        hShell32, wantWatermarkHook, ARRAYSIZE(wantWatermarkHook));
    Wh_Log(L"HookSymbols s_WantWatermark: %s", wantHooked ? L"OK" : L"FAILED");

    if (!wallpaperHooked && !paintHooked && !wantHooked)
    {
        Wh_Log(L"Failed to hook any watermark symbol in shell32.dll");
        return FALSE;
    }

    return TRUE;
}
