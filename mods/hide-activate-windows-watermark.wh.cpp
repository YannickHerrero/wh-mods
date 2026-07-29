// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         2.0.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

On current Windows 11 (e.g. 23H2 / build 22631) the watermark text is rendered
with `DrawTextExW`. This mod hooks `DrawTextExW`/`DrawTextW` and drops the draw
when the text is the watermark (it contains "activate windows"), leaving all
other text untouched. Measurement passes (`DT_CALCRECT`) are left alone so
layout code doesn't misbehave.

The match is on the English watermark text; on a non-English Windows the phrase
in `IsWatermarkText` below would need to be adjusted.

**Note:** This only hides the on-screen notice — it does not activate Windows.

To apply it, enable the mod and restart Windows Explorer (Task Manager →
*Windows Explorer* → Restart) so the desktop is composed with the mod active.
*/
// ==/WindhawkModReadme==

#include <windhawk_utils.h>

// True if the text contains the watermark phrase "activate windows"
// (case-insensitive). Present in both watermark lines:
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

// Suppress only the actual draw; let DT_CALCRECT measurement pass through.
static bool ShouldDrop(PCWSTR text, int len, UINT fmt)
{
    return !(fmt & DT_CALCRECT) && IsWatermarkText(text, len);
}

using DrawTextExW_t = int(WINAPI*)(HDC, LPWSTR, int, LPRECT, UINT, LPDRAWTEXTPARAMS);
DrawTextExW_t DrawTextExW_orig;
int WINAPI DrawTextExW_hook(HDC hdc, LPWSTR text, int cch, LPRECT rc, UINT fmt,
                            LPDRAWTEXTPARAMS dtp)
{
    if (ShouldDrop(text, cch, fmt))
        return 0;
    return DrawTextExW_orig(hdc, text, cch, rc, fmt, dtp);
}

using DrawTextW_t = int(WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
DrawTextW_t DrawTextW_orig;
int WINAPI DrawTextW_hook(HDC hdc, LPCWSTR text, int cch, LPRECT rc, UINT fmt)
{
    if (ShouldDrop(text, cch, fmt))
        return 0;
    return DrawTextW_orig(hdc, text, cch, rc, fmt);
}

BOOL Wh_ModInit(void)
{
    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (!hUser32)
        return FALSE;

    BOOL ok = FALSE;
    void* pDrawTextExW = (void*)GetProcAddress(hUser32, "DrawTextExW");
    if (pDrawTextExW)
        ok |= Wh_SetFunctionHook(pDrawTextExW, (void*)DrawTextExW_hook,
                                 (void**)&DrawTextExW_orig);

    void* pDrawTextW = (void*)GetProcAddress(hUser32, "DrawTextW");
    if (pDrawTextW)
        ok |= Wh_SetFunctionHook(pDrawTextW, (void*)DrawTextW_hook,
                                 (void**)&DrawTextW_orig);

    return ok;
}
