// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.0.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

It works by hooking `CDesktopWatermark::s_WantWatermark()` in `shell32.dll` (the
gate `explorer.exe` calls before painting the watermark) and forcing it to
return `false`, so the watermark is never drawn.

**Note:** This only hides the on-screen notice — it does not activate Windows.

If a future Windows build moves or renames the watermark code and the mod stops
working, the symbol string in the source may need updating (or alternate symbol
strings added to the hook entry).
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

using CDesktopWatermark_s_WantWatermark_t = bool(__cdecl*)(void);
CDesktopWatermark_s_WantWatermark_t CDesktopWatermark_s_WantWatermark_orig;

bool __cdecl CDesktopWatermark_s_WantWatermark_hook(void)
{
    return false;
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

    const WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            {
                L"public: static bool __cdecl CDesktopWatermark::s_WantWatermark(void)"
            },
            &CDesktopWatermark_s_WantWatermark_orig,
            CDesktopWatermark_s_WantWatermark_hook,
            false
        },
    };

    if (!WindhawkUtils::HookSymbols(hShell32, hooks, ARRAYSIZE(hooks)))
    {
        Wh_Log(L"Failed to hook CDesktopWatermark::s_WantWatermark in shell32.dll");
        return FALSE;
    }

    return TRUE;
}
