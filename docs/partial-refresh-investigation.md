# Fast partial refresh investigation — 2026-09-16

## Outcome

No verified fast partial-refresh implementation was found for the PaperColor's
ED2208-DOA / EL040EF1 400 x 600 panel. This is not proof that an undocumented mode
cannot exist. No experimental commands were sent to the device, and firmware
remains unchanged. A smaller framebuffer transfer alone does not meet the request
for a quickly moving menu selection pip.

## Evidence

1. The pinned M5GFX driver `Panel_ED2208.cpp` accepts a dirty rectangle in
   `display()`, but `_exec_transfer()` sends the full configured panel dimensions.
   Every update then follows the same power-on / refresh / power-off sequence.
   Its `epd_fastest` option selects conversion without dithering; it does not
   select a faster panel waveform. `Panel_ED2208.hpp` initializes register 0x30
   to 0x08.

2. [GxEPD2's maintainer discussion](https://github.com/ZinggJM/GxEPD2/discussions/161)
   describes newer regional-update support for GDEP073E01, but not universal
   support across six-colour panels. The
   [GDEP073E01 header](https://github.com/ZinggJM/GxEPD2/blob/master/src/epd7c/GxEPD2_730c_GDEP073E01.h)
   explicitly reports `hasFastPartialUpdate = false` and identical full/partial
   refresh time estimates. This is an 800 x 480 panel, not ours; its 0x83 window
   command is not established as compatible with ED2208-DOA.

3. The [EL040EF1 manual supplied by Waveshare](https://files.waveshare.com/wiki/4inch-e-Paper-HAT%2B-%28E%29/4-inch-e-paper-user-manual.pdf)
   documents the module, electrical limits and power sequencing, but does not
   provide a usable fast partial-refresh command/waveform. Note 5-4 on printed
   page 6 says not to interrupt operation or issue commands while BUSY is low.
   Resetting the panel partway through its waveform is therefore not an acceptable
   default implementation. The long-term effects have not been established.

4. [Waveshare repository PR #387](https://github.com/waveshareteam/e-Paper/pull/387)
   is a concrete experimental lead for **accelerated full refresh**. Its author
   reports about 9.8 seconds for a 4-inch Spectra 6 panel using PLL register 0x30
   value 0x07; the patch also uses automatic power/refresh sequencing. These are
   the author's measurements, not measurements on our device or a manufacturer
   guarantee. This does not produce fast regional selection-marker updates.

## Proposed next experiment, requiring user choice

Offer an explicitly experimental, opt-in **accelerated full refresh** test,
separate from normal reading and picture display. Explain before installation
that contrast, ghosting, colour and temperature sensitivity may change, and
long-term panel behaviour is unverified. Do not promise the reported timing.

If chosen, retain the known-good default, limit the test to one completed refresh,
measure the actual busy interval, and restore the default timing before the next
normal refresh. Keep normal power sequencing and BUSY completion; do not cut
off a waveform or alter drive-voltage/OTP settings. Compare black/white text and
colour test patches visually before considering wider use. Keep the feature named
"accelerated full refresh" unless a separately verified regional mode exists.

For true fast partial refresh, the missing input is a panel-specific supported
command sequence and waveform, or a reproducible ED2208-DOA implementation with
credible electrical constraints. Commands from unrelated monochrome or larger
Spectra panels should not be substituted blindly.
