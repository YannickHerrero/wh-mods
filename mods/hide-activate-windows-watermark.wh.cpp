// ==WindhawkMod==
// @id              hide-activate-windows-watermark
// @name            Hide Activate Windows Watermark
// @description     Hides the "Activate Windows" desktop watermark
// @version         2.1.0
// @author          yh
// @include         explorer.exe
// @architecture    x86-64
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Hide Activate Windows Watermark

Hides the "Activate Windows - Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

On current Windows 11 (e.g. 23H2 / build 22631) the watermark text is rendered
with `DrawTextExW`. This mod hooks `DrawTextExW`/`DrawTextW` and drops the draw
when the text is the watermark (it contains "activate windows"), leaving all
other text untouched. Measurement passes (`DT_CALCRECT`) are left alone so
layout code isn't disturbed.

The match is on the English watermark text; on a non-English Windows the phrase
in `IsWatermarkText` below would need to be adjusted.

At load the mod logs the Windows build number and whether each hook installed.
These one-time lines make it easy to diagnose if the mod ever stops working:

- Hooks show `OK` and the build number is unchanged, but the watermark is back
  -> Windhawk failed to apply the hook to that Explorer instance. Recompile the
  mod (or toggle it off/on) to force a clean re-hook, then restart Explorer.
- A hook shows `FAILED`, or the build number changed -> a Windows update likely
  moved the render path and the mod needs re-targeting.

**Note:** This only hides the on-screen notice - it does not activate Windows.

To apply it, enable the mod and restart Windows Explorer (Task Manager ->
*Windows Explorer* -> Restart) so the desktop is composed with the mod active.
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

BOOL Wh_ModInit(void)
{
    Wh_Log(L"Init");
    LogWindowsBuild();

    HMODULE hUser32 = LoadLibraryW(L"user32.dll");
    if (!hUser32)
    {
        Wh_Log(L"Failed to load user32.dll");
        return FALSE;
    }

    BOOL exOk = FALSE, wOk = FALSE;
    void* pDrawTextExW = (void*)GetProcAddress(hUser32, "DrawTextExW");
    if (pDrawTextExW)
        exOk = Wh_SetFunctionHook(pDrawTextExW, (void*)DrawTextExW_hook,
                                  (void**)&DrawTextExW_orig);
    Wh_Log(L"Hook DrawTextExW: %s", exOk ? L"OK" : L"FAILED");

    void* pDrawTextW = (void*)GetProcAddress(hUser32, "DrawTextW");
    if (pDrawTextW)
        wOk = Wh_SetFunctionHook(pDrawTextW, (void*)DrawTextW_hook,
                                 (void**)&DrawTextW_orig);
    Wh_Log(L"Hook DrawTextW: %s", wOk ? L"OK" : L"FAILED");

    return exOk || wOk;
}
