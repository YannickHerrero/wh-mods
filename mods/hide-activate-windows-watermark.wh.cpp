// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.1.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

It works by hooking two functions of `CDesktopWatermark` in `shell32.dll`:

- `s_WantWatermark()` — the gate the shell checks before showing the watermark,
  forced to return `false`.
- `s_DesktopBuildPaint()` — the function that actually paints the watermark text
  (all watermarks — activation, evaluation and test-mode — go through it). The
  hook returns without painting anything. Hooking here is what suppresses the
  activation watermark, which does not respect the `s_WantWatermark` gate on all
  builds.

**Note:** This only hides the on-screen notice — it does not activate Windows.

After enabling the mod, restart Windows Explorer (Task Manager → *Windows
Explorer* → Restart) to force a fresh desktop paint.

If a future Windows build moves or renames the watermark code and the mod stops
working, check the mod log (each hook logs whether it resolved and when it
fires) and update the symbol strings below.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

using CDesktopWatermark_s_WantWatermark_t = bool(__cdecl*)(void);
CDesktopWatermark_s_WantWatermark_t CDesktopWatermark_s_WantWatermark_orig;

using CDesktopWatermark_s_DesktopBuildPaint_t = void(__cdecl*)(HDC, LPCRECT, HFONT);
CDesktopWatermark_s_DesktopBuildPaint_t CDesktopWatermark_s_DesktopBuildPaint_orig;

bool __cdecl CDesktopWatermark_s_WantWatermark_hook(void)
{
    Wh_Log(L"s_WantWatermark called -> returning false");
    return false;
}

void __cdecl CDesktopWatermark_s_DesktopBuildPaint_hook(HDC, LPCRECT, HFONT)
{
    Wh_Log(L"s_DesktopBuildPaint called -> suppressed (not painting)");
    // Intentionally do not call the original: this prevents the watermark
    // (activation / evaluation / test-mode) from being drawn.
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

    // Hook each symbol independently so one missing symbol does not disable the
    // other, and so the log tells us exactly which resolved.
    BOOL wantHooked = WindhawkUtils::HookSymbols(
        hShell32, wantWatermarkHook, ARRAYSIZE(wantWatermarkHook));
    Wh_Log(L"HookSymbols s_WantWatermark: %s", wantHooked ? L"OK" : L"FAILED");

    BOOL paintHooked = WindhawkUtils::HookSymbols(
        hShell32, buildPaintHook, ARRAYSIZE(buildPaintHook));
    Wh_Log(L"HookSymbols s_DesktopBuildPaint: %s", paintHooked ? L"OK" : L"FAILED");

    if (!wantHooked && !paintHooked)
    {
        Wh_Log(L"Failed to hook any watermark symbol in shell32.dll");
        return FALSE;
    }

    return TRUE;
}
