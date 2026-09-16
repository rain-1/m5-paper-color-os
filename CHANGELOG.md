# Firmware versions

Version lives in `include/version.h`. Use `MAJOR.MINOR.PATCH`: major changes break
compatibility, minor changes add features, patch changes fix or refine behaviour.
The current API and device screen use the same version. Release binaries are
application images for the existing 16 MB partition layout.

## 0.6.0

- Full-page refresh experiments: serif/sans text, inverse text, checkerboards,
  six-colour blocks and white cleanup, with alternate text/pattern variants.
- Last eight completed test timings retained in RAM with pattern and mode labels.
- Removed confusing A/B test labels; physical buttons remain reader controls.
- Clarified that USB power is recommended, not mandatory with adequate battery.
- Accelerated timing remains opt-in, one refresh per request, with default-clock
  restoration and the existing fault/power guards unchanged.

## 0.5.0

- Opt-in `/refresh-test` lab for one-shot accelerated **full-screen** refresh.
- Normal chart must complete before experimental PLL timing is selectable;
  acknowledgement and adequate measured power are required.
- Compare normal 0x08 and experimental 0x07 clock settings, with measured total
  and refresh BUSY times. Normal clock is written back after completion.
- Isolated test waits for actual BUSY completion with 120-second timeouts. Failure
  latches a display fault; no further display commands are allowed until restart.
- Normal reader/gallery behaviour remains unchanged. No partial-refresh claim,
  persistent fast-mode setting, waveform interruption or OTP/voltage changes.

## 0.4.0

- TXT book import, normalization preview, SD library and prepared-text download.
- Offline button-driven reader/menu, measured word wrapping and browser page jumps.
- Saved per-book positions/bookmarks in NVS and automatic last-book restoration.
- Five reading fonts, offset-preserving reflow and an on-device comparison sheet.
- Short actions on button release; hold A bookmarks, hold B opens Wi-Fi, hold C
  retains OTA unlock. Busy reader actions cannot queue accidental page turns.
- Active reading survives network state changes; existing picture and OTA features retained.

## 0.3.0

- Consistent sound/LED cues for connection, saved pictures, failures, busy state,
  update unlock and completed updates; persistent mute and lab previews.
- Central firmware version identifier.
- OTA power guard before/during upload and before activation, with power status in the lab.

## 0.2.1

- Physical button diagram and accurate A/B/C location labels.
- Last-reset reason in diagnostics. Verified installation over Wi-Fi.

## 0.2.0

- Browser image fitting, quantization/dithering, fixed-format SD storage and gallery.
- Device lab: battery, buttons, LEDs, tone, environmental/RTC/memory readings.
- Physically enabled browser OTA updates to the inactive app slot.
- Reject malformed raw upload requests safely.

## 0.1.0

- Splash, captive Wi-Fi setup, persistent internal credentials and connection fallback.
