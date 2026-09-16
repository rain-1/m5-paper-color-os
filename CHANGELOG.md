# Firmware versions

Version lives in `include/version.h`. Use `MAJOR.MINOR.PATCH`: major changes break
compatibility, minor changes add features, patch changes fix or refine behaviour.
The current API and device screen use the same version. Release binaries are
application images for the existing 16 MB partition layout.

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
