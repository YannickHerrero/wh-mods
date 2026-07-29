# Windhawk Mods

Personal [Windhawk](https://windhawk.net/) mods.

## Mods

### Hide Activate Windows Watermark

Hides the "Activate Windows - Go to Settings to activate Windows" watermark that
Windows draws in the bottom-right of the desktop on unactivated installs.

On current Windows 11 (e.g. 23H2 / build 22631) the watermark text is rendered
with `DrawTextExW`. The mod hooks `DrawTextExW`/`DrawTextW` and drops the draw
when the text is the watermark (it contains "activate windows"), leaving all
other text untouched. Measurement passes (`DT_CALCRECT`) are left alone so
layout code isn't disturbed.

The match is on the English watermark text; on a non-English Windows the phrase
in `IsWatermarkText` would need adjusting.

> **Note:** This only hides the on-screen notice - it does not activate Windows.

Source: [`mods/hide-activate-windows-watermark.wh.cpp`](mods/hide-activate-windows-watermark.wh.cpp)

## Installing a mod

These mods are not published to the Windhawk marketplace - install them manually
from source:

1. Install [Windhawk](https://windhawk.net/) on your Windows machine.
2. Open Windhawk and click **Explore** -> **Create a new mod** (the `+` /
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

Toggle the mod off (or delete it) in Windhawk - hooks are removed cleanly on
unload.

## Troubleshooting

If the watermark is still showing:

1. **Restart Explorer.** The watermark is baked into the composed desktop until
   Explorer redraws with the mod active. Task Manager -> *Windows Explorer* ->
   **Restart**. This is the important step - re-injecting into a running Explorer
   isn't enough on its own.
2. **Non-English Windows.** The mod matches the English string "activate
   windows". For another language, change the phrase in `IsWatermarkText`.
3. **Check architecture.** The mod targets `x86-64`. On ARM64 Windows the
   `@architecture` line must match, or the mod won't inject into Explorer.

## Development notes

- Mods target `x86-64` and `explorer.exe`.
- The watermark is matched by its **text content** rather than by a specific
  paint function, so it keeps working regardless of which internal code path or
  DC (on 23H2 it's rendered into an offscreen memory DC) produces it.
