# Fast partial refresh investigation — 2026-09-16

## Outcome

No verified fast partial-refresh implementation was found for the PaperColor's
ED2208-DOA / EL040EF1 400 x 600 panel. This is not proof that an undocumented mode
cannot exist. No experimental commands were sent to the device, and firmware
was unchanged during this investigation. A smaller framebuffer transfer alone does not meet the request
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

The user subsequently approved the accelerated full-refresh experiment. Firmware
0.5.0 implements the following opt-in workflow; physical validation is pending.

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

## Running the 0.5.0 experiment

1. Prefer USB power for consistent comparisons (adequate battery is also accepted),
   allow any current refresh to finish, then hold top C until
   the unlock sound. Install the 0.5.0 application through the existing OTA route.
2. After restart, open `http://DEVICE_IP/refresh-test` (also linked from Device lab).
3. Run **Normal chart**, wait for completion, and inspect or photograph the screen.
4. Acknowledge experimental timing and run **Accelerated chart (one shot)**.
5. Compare the timings shown in the browser, the two 12 pt fonts, clean white
   background, solid black dot, removal of the previous dot, and colour patches.
6. If output is poor, run Normal chart again. Resume the book through `/books`.

Nothing is permanently enabled. Normal PLL 0x08 is sent after every successful
test, including the fast 0x07 test. The reported restoration means that write was
issued after completed power-off; the panel register is not read back. A successful
visual normal refresh afterward is an additional useful check. Both test modes
use the same explicit power sequence and conversion, changing only PLL timing.
No auto-sequence optimization from the reference PR is included.

The test runs in the display task under the shared screen/SD lock and uses the
configured SPI bus. It sends all 120,000 packed pixel bytes, checks BUSY assertion
after refresh, waits up to 120 seconds for each busy phase, then restores timing.
On timeout or missing BUSY assertion, it sends no further panel commands, releases
the SPI transaction, and latches a fault that blocks display requests until reboot.
Do not repeatedly retry or interrupt an active waveform. The lab remains online
for diagnosis. Test results are held only in RAM; no book, bookmark or SD file is
changed. Reader page saving is bypassed for these test charts.

Host verification: firmware build, RGB565-to-native palette checks and
`tests/refresh_browser.py` against mocked HTTP endpoints. These are not evidence
that the experimental timing improves speed or quality on the physical panel.

## Expanded full-page checks in 0.6.0

The user reported good quality for the 0.5.0 accelerated chart. Measured totals
were 35.368 s normal and 18.627 s accelerated; the accelerated refresh BUSY interval
was 16.960 s. This justifies more image-transition testing, not a blanket claim
about all images, temperatures or panel lifetime.

Use the pattern and variant selectors on `/refresh-test`; the physical A/B/C
buttons do not control the test. Each browser click still runs one page only.
An initial completed normal test is required after boot before fast testing.
Suggested sequence, inspecting the completed screen after every step:

1. Serif variant 1 at normal timing; photograph as a reference.
2. Serif variant 2 accelerated, then variant 1 accelerated: look for old letters.
3. Sans text accelerated: compare small strokes and readability.
4. Inverse text accelerated, then ordinary serif: check the large black-to-white transition.
5. Checkerboard variant 1 then 2 accelerated: check both polarities across the screen.
6. Colour variant 1 then 2 accelerated: check colour residue and boundaries.
7. White cleanup accelerated, then normal if needed: look for lingering patterns.

For direct quality comparisons, render the same pattern and variant with both
timings. The last eight completed results include pattern/variant/mode, total time
and BUSY time and are cleared by reboot. The standalone last-normal/last-fast totals
may correspond to different patterns; do not treat them as a controlled pair.
Ordinary reader and gallery refresh timing remains unchanged.

USB was a test precaution, not a requirement of accelerated refresh. The power
guard accepts measured 4.6–5.5 V input OR at least 30% estimated battery with
3.6–4.35 V battery voltage. Stay powered and do not reset during an update.
