# Windhawk Mods

Personal [Windhawk](https://windhawk.net/) mods.

## Mods

### Hide Activate Windows Watermark

Hides the "Activate Windows — Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

It hooks `CDesktopWatermark::s_WantWatermark()` in `shell32.dll` (the gate
`explorer.exe` checks before painting the watermark) and forces it to return
`false`, so the watermark is never drawn.

> **Note:** This only hides the on-screen notice — it does not activate Windows.

Source: [`mods/hide-activate-windows-watermark.wh.cpp`](mods/hide-activate-windows-watermark.wh.cpp)

## Installing a mod

These mods are not published to the Windhawk marketplace — install them manually
from source:

1. Install [Windhawk](https://windhawk.net/) on your Windows machine.
2. Open Windhawk and click **Explore** → **Create a new mod** (the `+` /
   *Create Mod* button).
3. Delete the template code, then paste the full contents of the mod's
   `.wh.cpp` file from this repo.
4. Click **Compile mod** and wait for it to build with no errors.
5. Switch to the **Mod details** tab and toggle the mod **on**.

The mod hooks `explorer.exe`, so the change takes effect within a few seconds
(you may need to let Explorer refresh).

### Updating

Re-open the mod in the Windhawk editor, paste the newer version of the source,
and click **Compile mod** again.

### Uninstalling

Toggle the mod off (or delete it) in Windhawk — hooks are removed cleanly on
unload.

## Development notes

- Mods target `x86-64` and `explorer.exe`.
- Symbol hooks are resolved with `WindhawkUtils::HookSymbols` from
  `<windhawk_utils.h>`, which caches symbols across Windows builds. If a future
  Windows update moves or renames the hooked function, the symbol string in the
  source may need updating.
