// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.4.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

The function that draws the watermark differs by Windows version, so this mod
hooks every known painter in `shell32.dll` and makes it do nothing:

- `CWallpaperRenderer::PaintDesktopWatermarkText()` and its private
  `_PaintDesktopWatermarkText()` — used when the watermark is composited into
  the wallpaper.
- `CDesktopBrowser::_PaintDesktopWatermarkText()` — used when the watermark is
  painted as an overlay on the desktop window.
- `CDesktopWatermark::s_DesktopBuildPaint()` — older builds.
- `DoesDesktopHaveWatermarkText()` — the gate; forced to report "no text".

It also hooks the watermark string providers and, as a fallback, shrinks their
text to a single character.

Each hook is installed independently; symbols missing on your build are skipped.
Every hook logs when it fires, so the mod log shows exactly which path draws the
watermark on your machine.

**Important:** To catch the watermark's initial paint, the mod must be loaded
*before* Explorer draws the desktop. After enabling the mod, **restart Windows
Explorer** (Task Manager → *Windows Explorer* → Restart). If the watermark is
still visible, force a desktop repaint (Win+D twice, or click the desktop and
press F5).

**Note:** This only hides the on-screen notice — it does not activate Windows.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

// ---- Painters -----------------------------------------------------------

using CWallpaperRenderer_PaintDesktopWatermarkText_t =
    void(__cdecl*)(HWND, HDC, UINT, HMONITOR*, LPCRECT, bool);
CWallpaperRenderer_PaintDesktopWatermarkText_t
    CWallpaperRenderer_PaintDesktopWatermarkText_orig;

using CWallpaperRenderer__PaintDesktopWatermarkText_t =
    void(__cdecl*)(UINT, HDC, HMONITOR*, LPCRECT, bool);
CWallpaperRenderer__PaintDesktopWatermarkText_t
    CWallpaperRenderer__PaintDesktopWatermarkText_orig;

using CDesktopBrowser__PaintDesktopWatermarkText_t = void(__cdecl*)(HWND, HDC);
CDesktopBrowser__PaintDesktopWatermarkText_t
    CDesktopBrowser__PaintDesktopWatermarkText_orig;

using CDesktopWatermark_s_DesktopBuildPaint_t = void(__cdecl*)(HDC, LPCRECT, HFONT);
CDesktopWatermark_s_DesktopBuildPaint_t CDesktopWatermark_s_DesktopBuildPaint_orig;

// ---- Gate ---------------------------------------------------------------

using DoesDesktopHaveWatermarkText_t = bool(__cdecl*)(void);
DoesDesktopHaveWatermarkText_t DoesDesktopHaveWatermarkText_orig;

// ---- String providers ---------------------------------------------------

using CDesktopWatermark_GetString_t = void(__cdecl*)(PWSTR, UINT);
CDesktopWatermark_GetString_t CDesktopWatermark_s_GetEvaluationString_orig;
CDesktopWatermark_GetString_t CDesktopWatermark_s_GetProductBuildString_orig;
CDesktopWatermark_GetString_t CDesktopWatermark_s_GetSafeModeString_orig;

void __cdecl CWallpaperRenderer_PaintDesktopWatermarkText_hook(
    HWND, HDC, UINT, HMONITOR*, LPCRECT, bool)
{
    Wh_Log(L"HIT CWallpaperRenderer::PaintDesktopWatermarkText -> suppressed");
}

void __cdecl CWallpaperRenderer__PaintDesktopWatermarkText_hook(
    UINT, HDC, HMONITOR*, LPCRECT, bool)
{
    Wh_Log(L"HIT CWallpaperRenderer::_PaintDesktopWatermarkText -> suppressed");
}

void __cdecl CDesktopBrowser__PaintDesktopWatermarkText_hook(HWND, HDC)
{
    Wh_Log(L"HIT CDesktopBrowser::_PaintDesktopWatermarkText -> suppressed");
}

void __cdecl CDesktopWatermark_s_DesktopBuildPaint_hook(HDC, LPCRECT, HFONT)
{
    Wh_Log(L"HIT CDesktopWatermark::s_DesktopBuildPaint -> suppressed");
}

bool __cdecl DoesDesktopHaveWatermarkText_hook(void)
{
    Wh_Log(L"HIT DoesDesktopHaveWatermarkText -> returning false");
    return false;
}

// Replace the provided string with a single character, so if the watermark
// text flows through here we both see the log and see it visibly change.
void ShrinkString(PWSTR buffer, UINT cch, PCWSTR which)
{
    Wh_Log(L"HIT %s -> shrinking text", which);
    if (buffer && cch >= 2)
    {
        buffer[0] = L'.';
        buffer[1] = L'\0';
    }
}

void __cdecl CDesktopWatermark_s_GetEvaluationString_hook(PWSTR buffer, UINT cch)
{
    ShrinkString(buffer, cch, L"s_GetEvaluationString");
}

void __cdecl CDesktopWatermark_s_GetProductBuildString_hook(PWSTR buffer, UINT cch)
{
    ShrinkString(buffer, cch, L"s_GetProductBuildString");
}

void __cdecl CDesktopWatermark_s_GetSafeModeString_hook(PWSTR buffer, UINT cch)
{
    ShrinkString(buffer, cch, L"s_GetSafeModeString");
}

// Hook one symbol (with optional alternates) and log the outcome.
static BOOL HookOne(HMODULE mod, PCWSTR label,
                    const WindhawkUtils::SYMBOL_HOOK* hook)
{
    BOOL ok = WindhawkUtils::HookSymbols(mod, hook, 1);
    Wh_Log(L"HookSymbols %s: %s", label, ok ? L"OK" : L"FAILED");
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

    const WindhawkUtils::SYMBOL_HOOK wallpaperPaint = {
        { L"public: void __cdecl CWallpaperRenderer::PaintDesktopWatermarkText(struct HWND__ *,struct HDC__ *,unsigned int,struct HMONITOR__ * *,struct tagRECT const *,bool)" },
        &CWallpaperRenderer_PaintDesktopWatermarkText_orig,
        CWallpaperRenderer_PaintDesktopWatermarkText_hook, false };

    const WindhawkUtils::SYMBOL_HOOK wallpaperPaintPriv = {
        { L"private: void __cdecl CWallpaperRenderer::_PaintDesktopWatermarkText(unsigned int,struct HDC__ *,struct HMONITOR__ *,struct tagRECT const *,bool)" },
        &CWallpaperRenderer__PaintDesktopWatermarkText_orig,
        CWallpaperRenderer__PaintDesktopWatermarkText_hook, false };

    const WindhawkUtils::SYMBOL_HOOK desktopBrowserPaint = {
        { L"protected: void __cdecl CDesktopBrowser::_PaintDesktopWatermarkText(struct HWND__ *,struct HDC__ *)" },
        &CDesktopBrowser__PaintDesktopWatermarkText_orig,
        CDesktopBrowser__PaintDesktopWatermarkText_hook, false };

    const WindhawkUtils::SYMBOL_HOOK buildPaint = {
        { L"private: static void __cdecl CDesktopWatermark::s_DesktopBuildPaint(struct HDC__ *,struct tagRECT const *,struct HFONT__ *)",
          L"private: static void __cdecl CDesktopWatermark::s_DesktopBuildPaint(struct HDC__ *,struct tagRECT const *,struct HFONT__ *,bool)" },
        &CDesktopWatermark_s_DesktopBuildPaint_orig,
        CDesktopWatermark_s_DesktopBuildPaint_hook, false };

    const WindhawkUtils::SYMBOL_HOOK gate = {
        { L"DoesDesktopHaveWatermarkText" },
        &DoesDesktopHaveWatermarkText_orig,
        DoesDesktopHaveWatermarkText_hook, false };

    const WindhawkUtils::SYMBOL_HOOK evalStr = {
        { L"private: static void __cdecl CDesktopWatermark::s_GetEvaluationString(unsigned short *,unsigned int)" },
        &CDesktopWatermark_s_GetEvaluationString_orig,
        CDesktopWatermark_s_GetEvaluationString_hook, false };

    const WindhawkUtils::SYMBOL_HOOK buildStr = {
        { L"private: static void __cdecl CDesktopWatermark::s_GetProductBuildString(unsigned short *,unsigned int)" },
        &CDesktopWatermark_s_GetProductBuildString_orig,
        CDesktopWatermark_s_GetProductBuildString_hook, false };

    const WindhawkUtils::SYMBOL_HOOK safeStr = {
        { L"private: static void __cdecl CDesktopWatermark::s_GetSafeModeString(unsigned short *,unsigned int)" },
        &CDesktopWatermark_s_GetSafeModeString_orig,
        CDesktopWatermark_s_GetSafeModeString_hook, false };

    int hooked = 0;
    hooked += HookOne(hShell32, L"CWallpaperRenderer::PaintDesktopWatermarkText", &wallpaperPaint);
    hooked += HookOne(hShell32, L"CWallpaperRenderer::_PaintDesktopWatermarkText", &wallpaperPaintPriv);
    hooked += HookOne(hShell32, L"CDesktopBrowser::_PaintDesktopWatermarkText", &desktopBrowserPaint);
    hooked += HookOne(hShell32, L"CDesktopWatermark::s_DesktopBuildPaint", &buildPaint);
    hooked += HookOne(hShell32, L"DoesDesktopHaveWatermarkText", &gate);
    hooked += HookOne(hShell32, L"CDesktopWatermark::s_GetEvaluationString", &evalStr);
    hooked += HookOne(hShell32, L"CDesktopWatermark::s_GetProductBuildString", &buildStr);
    hooked += HookOne(hShell32, L"CDesktopWatermark::s_GetSafeModeString", &safeStr);

    Wh_Log(L"Installed %d/8 hooks", hooked);

    if (hooked == 0)
    {
        Wh_Log(L"Failed to hook any watermark symbol in shell32.dll");
        return FALSE;
    }

    return TRUE;
}
