// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         1.10.0
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

This build intercepts the text-drawing functions (`DrawTextW`, `DrawTextExW`,
`ExtTextOutW`, `TextOutW`, `DrawTextWithGlow`, `DrawThemeText`,
`DrawThemeTextEx`) and drops any draw whose text is the watermark (matched by
content: it contains "activate windows"). Matching by content — rather than by
which function or code path draws it — makes it robust across the various ways
Windows renders the watermark, and avoids touching any other text.

Every dropped draw is logged with its text.

**Important:** The watermark repaints only when its region is invalidated. After
enabling the mod, force a repaint — drag a window over the bottom-right corner
and away, or click the desktop and press F5. If it persists, changing the screen
resolution (and back) forces Windows to rebuild it.

**Note:** This only hides the on-screen notice — it does not activate Windows.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>
#include <uxtheme.h>

// True if the text contains the watermark phrase "activate windows"
// (case-insensitive), which appears in both watermark lines:
//   "Activate Windows"
//   "Go to Settings to activate Windows"
static bool IsWatermarkText(PCWSTR text, int len)
{
    if (!text)
        return false;
    if (len < 0)
        len = lstrlenW(text);

    static const wchar_t needle[] = L"activate windows";
    const int nlen = (int)(ARRAYSIZE(needle) - 1);

    for (int i = 0; i + nlen <= len; i++)
    {
        int j = 0;
        for (; j < nlen; j++)
        {
            wchar_t c = text[i + j];
            if (c >= L'A' && c <= L'Z')
                c = (wchar_t)(c + 32);
            if (c != needle[j])
                break;
        }
        if (j == nlen)
            return true;
    }
    return false;
}

static bool DropIfWatermark(PCWSTR fn, PCWSTR text, int len)
{
    if (!IsWatermarkText(text, len))
        return false;
    if (len < 0)
        len = lstrlenW(text);
    if (len > 200)
        len = 200;
    Wh_Log(L"%s -> DROPPED watermark: \"%.*s\"", fn, len, text);
    return true;
}

// ---- PaintMonitor (context only) ----------------------------------------

using CWallpaperRenderer_PaintMonitor_t =
    void(__cdecl*)(UINT, HWND, HMONITOR, HMONITOR, HDC, LPCRECT, LPCRECT, LPCRECT);
CWallpaperRenderer_PaintMonitor_t CWallpaperRenderer_PaintMonitor_orig;

void __cdecl CWallpaperRenderer_PaintMonitor_hook(
    UINT a1, HWND a2, HMONITOR a3, HMONITOR a4, HDC a5,
    LPCRECT a6, LPCRECT a7, LPCRECT a8)
{
    Wh_Log(L"PaintMonitor ENTER");
    CWallpaperRenderer_PaintMonitor_orig(a1, a2, a3, a4, a5, a6, a7, a8);
    Wh_Log(L"PaintMonitor EXIT");
}

// ---- GDI text functions -------------------------------------------------

// Log a watermark DrawText* call with its flags/rect. Returns true if this is
// a real draw that should be suppressed; false if it's a measurement pass
// (DT_CALCRECT) that must run normally.
static bool ShouldSuppressDraw(PCWSTR fn, PCWSTR text, int len, UINT fmt, LPRECT rc)
{
    if (!IsWatermarkText(text, len))
        return false;
    bool calc = (fmt & DT_CALCRECT) != 0;
    Wh_Log(L"%s watermark: fmt=0x%08X%s rect=(%ld,%ld,%ld,%ld) text=\"%s\"",
           fn, fmt, calc ? L" [CALCRECT/measure]" : L" [DRAW -> suppressed]",
           rc ? rc->left : 0, rc ? rc->top : 0, rc ? rc->right : 0,
           rc ? rc->bottom : 0, text);
    return !calc;
}

using DrawTextW_t = int(WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
DrawTextW_t DrawTextW_orig;
int WINAPI DrawTextW_hook(HDC hdc, LPCWSTR text, int cch, LPRECT rc, UINT fmt)
{
    if (ShouldSuppressDraw(L"DrawTextW", text, cch, fmt, rc))
        return 0;
    return DrawTextW_orig(hdc, text, cch, rc, fmt);
}

using DrawTextExW_t = int(WINAPI*)(HDC, LPWSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);
DrawTextExW_t DrawTextExW_orig;
int WINAPI DrawTextExW_hook(HDC hdc, LPWSTR text, int cch, LPRECT rc, UINT fmt,
                            LPDRAWTEXTPARAMS dtp)
{
    if (ShouldSuppressDraw(L"DrawTextExW", text, cch, fmt, rc))
        return 0;
    return DrawTextExW_orig(hdc, text, cch, rc, fmt, dtp);
}

using ExtTextOutW_t = BOOL(WINAPI*)(HDC, int, int, UINT, const RECT*, LPCWSTR, UINT, const INT*);
ExtTextOutW_t ExtTextOutW_orig;
BOOL WINAPI ExtTextOutW_hook(HDC hdc, int x, int y, UINT opt, const RECT* rc,
                             LPCWSTR str, UINT c, const INT* dx)
{
    if (DropIfWatermark(L"ExtTextOutW", str, (int)c))
        return TRUE;
    return ExtTextOutW_orig(hdc, x, y, opt, rc, str, c, dx);
}

using TextOutW_t = BOOL(WINAPI*)(HDC, int, int, LPCWSTR, int);
TextOutW_t TextOutW_orig;
BOOL WINAPI TextOutW_hook(HDC hdc, int x, int y, LPCWSTR str, int c)
{
    if (DropIfWatermark(L"TextOutW", str, c))
        return TRUE;
    return TextOutW_orig(hdc, x, y, str, c);
}

// ---- Themed text functions ----------------------------------------------

using DrawTextWithGlow_t = HRESULT(WINAPI*)(HDC, LPCWSTR, int, RECT*, DWORD,
                                            COLORREF, COLORREF, UINT, UINT, BOOL,
                                            void*, LPARAM);
DrawTextWithGlow_t DrawTextWithGlow_orig;
HRESULT WINAPI DrawTextWithGlow_hook(HDC hdc, LPCWSTR text, int cch, RECT* rc,
                                     DWORD flags, COLORREF crText, COLORREF crGlow,
                                     UINT radius, UINT intensity, BOOL premul,
                                     void* cb, LPARAM lp)
{
    if (DropIfWatermark(L"DrawTextWithGlow", text, cch))
        return S_OK;
    return DrawTextWithGlow_orig(hdc, text, cch, rc, flags, crText, crGlow,
                                 radius, intensity, premul, cb, lp);
}

using DrawThemeText_t = HRESULT(WINAPI*)(HTHEME, HDC, int, int, LPCWSTR, int,
                                         DWORD, DWORD, LPCRECT);
DrawThemeText_t DrawThemeText_orig;
HRESULT WINAPI DrawThemeText_hook(HTHEME hTheme, HDC hdc, int part, int state,
                                  LPCWSTR text, int cch, DWORD flags, DWORD flags2,
                                  LPCRECT rc)
{
    if (DropIfWatermark(L"DrawThemeText", text, cch))
        return S_OK;
    return DrawThemeText_orig(hTheme, hdc, part, state, text, cch, flags, flags2, rc);
}

using DrawThemeTextEx_t = HRESULT(WINAPI*)(HTHEME, HDC, int, int, LPCWSTR, int,
                                           DWORD, LPRECT, const DTTOPTS*);
DrawThemeTextEx_t DrawThemeTextEx_orig;
HRESULT WINAPI DrawThemeTextEx_hook(HTHEME hTheme, HDC hdc, int part, int state,
                                    LPCWSTR text, int cch, DWORD flags, LPRECT rc,
                                    const DTTOPTS* opts)
{
    if (DropIfWatermark(L"DrawThemeTextEx", text, cch))
        return S_OK;
    return DrawThemeTextEx_orig(hTheme, hdc, part, state, text, cch, flags, rc, opts);
}

// ---- String sources (renderer-agnostic) ---------------------------------

using LoadStringW_t = int(WINAPI*)(HINSTANCE, UINT, LPWSTR, int);
LoadStringW_t LoadStringW_orig;
int WINAPI LoadStringW_hook(HINSTANCE hInst, UINT id, LPWSTR buf, int cch)
{
    int r = LoadStringW_orig(hInst, id, buf, cch);
    if (buf && cch > 0 && r > 0 && IsWatermarkText(buf, r))
    {
        Wh_Log(L"LoadStringW id=%u -> blanked: \"%.*s\"", id, r > 200 ? 200 : r, buf);
        buf[0] = L'\0';
        return 0;
    }
    return r;
}

using SHLoadIndirectString_t = HRESULT(WINAPI*)(PCWSTR, PWSTR, UINT, void**);
SHLoadIndirectString_t SHLoadIndirectString_orig;
HRESULT WINAPI SHLoadIndirectString_hook(PCWSTR src, PWSTR out, UINT cch, void** rsv)
{
    HRESULT hr = SHLoadIndirectString_orig(src, out, cch, rsv);
    if (SUCCEEDED(hr) && out && cch > 0 && IsWatermarkText(out, -1))
    {
        Wh_Log(L"SHLoadIndirectString -> blanked: \"%s\"", out);
        out[0] = L'\0';
    }
    return hr;
}

// ---- Window enumeration (is the watermark a separate window?) ------------

static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM)
{
    WCHAR text[256] = {0};
    GetWindowTextW(hwnd, text, ARRAYSIZE(text));
    if (IsWatermarkText(text, -1))
    {
        WCHAR cls[128] = {0};
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        RECT rc = {0};
        GetWindowRect(hwnd, &rc);
        Wh_Log(L"WINDOW MATCH (child) hwnd=%p class=%s rect=(%d,%d,%d,%d) text=\"%s\"",
               hwnd, cls, rc.left, rc.top, rc.right, rc.bottom, text);
    }
    return TRUE;
}

static BOOL CALLBACK EnumTopProc(HWND hwnd, LPARAM)
{
    WCHAR text[256] = {0};
    GetWindowTextW(hwnd, text, ARRAYSIZE(text));
    if (IsWatermarkText(text, -1))
    {
        WCHAR cls[128] = {0};
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        RECT rc = {0};
        GetWindowRect(hwnd, &rc);
        Wh_Log(L"WINDOW MATCH (top) hwnd=%p class=%s rect=(%d,%d,%d,%d) text=\"%s\"",
               hwnd, cls, rc.left, rc.top, rc.right, rc.bottom, text);
    }
    EnumChildWindows(hwnd, EnumChildProc, 0);
    return TRUE;
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
    HMODULE hShlwapi = LoadLibraryW(L"shlwapi.dll");

    int hooked = 0;

    if (hShell32)
    {
        const WindhawkUtils::SYMBOL_HOOK paintMonitor = {
            { L"public: void __cdecl CWallpaperRenderer::PaintMonitor(unsigned int,struct HWND__ *,struct HMONITOR__ *,struct HMONITOR__ *,struct HDC__ *,struct tagRECT const *,struct tagRECT const *,struct tagRECT const *)" },
            &CWallpaperRenderer_PaintMonitor_orig,
            CWallpaperRenderer_PaintMonitor_hook, false };
        hooked += HookOne(hShell32, L"CWallpaperRenderer::PaintMonitor", &paintMonitor);
    }

    hooked += HookApiPtr(hUser32 ? (void*)GetProcAddress(hUser32, "DrawTextW") : nullptr,
                         L"DrawTextW", (void*)DrawTextW_hook, (void**)&DrawTextW_orig);
    hooked += HookApiPtr(hUser32 ? (void*)GetProcAddress(hUser32, "DrawTextExW") : nullptr,
                         L"DrawTextExW", (void*)DrawTextExW_hook, (void**)&DrawTextExW_orig);
    hooked += HookApiPtr(hGdi32 ? (void*)GetProcAddress(hGdi32, "ExtTextOutW") : nullptr,
                         L"ExtTextOutW", (void*)ExtTextOutW_hook, (void**)&ExtTextOutW_orig);
    hooked += HookApiPtr(hGdi32 ? (void*)GetProcAddress(hGdi32, "TextOutW") : nullptr,
                         L"TextOutW", (void*)TextOutW_hook, (void**)&TextOutW_orig);

    hooked += HookApiPtr(hUxtheme ? (void*)GetProcAddress(hUxtheme, MAKEINTRESOURCEA(126)) : nullptr,
                         L"DrawTextWithGlow", (void*)DrawTextWithGlow_hook,
                         (void**)&DrawTextWithGlow_orig);
    hooked += HookApiPtr(hUxtheme ? (void*)GetProcAddress(hUxtheme, "DrawThemeText") : nullptr,
                         L"DrawThemeText", (void*)DrawThemeText_hook,
                         (void**)&DrawThemeText_orig);
    hooked += HookApiPtr(hUxtheme ? (void*)GetProcAddress(hUxtheme, "DrawThemeTextEx") : nullptr,
                         L"DrawThemeTextEx", (void*)DrawThemeTextEx_hook,
                         (void**)&DrawThemeTextEx_orig);

    hooked += HookApiPtr(hUser32 ? (void*)GetProcAddress(hUser32, "LoadStringW") : nullptr,
                         L"LoadStringW", (void*)LoadStringW_hook, (void**)&LoadStringW_orig);
    hooked += HookApiPtr(hShlwapi ? (void*)GetProcAddress(hShlwapi, "SHLoadIndirectString") : nullptr,
                         L"SHLoadIndirectString", (void*)SHLoadIndirectString_hook,
                         (void**)&SHLoadIndirectString_orig);

    Wh_Log(L"Enumerating windows for watermark text...");
    EnumWindows(EnumTopProc, 0);
    Wh_Log(L"Window enumeration done");

    Wh_Log(L"Installed %d/10 hooks", hooked);

    if (hooked == 0)
    {
        Wh_Log(L"Failed to hook anything");
        return FALSE;
    }

    return TRUE;
}
