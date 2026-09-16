# Validation record — 2026-09-16

On the connected PaperColor at `192.168.50.7`, development firmware 0.2.0:

- USB flash and flash hashes verified; saved Wi-Fi reconnected across development uploads.
- FAT32 SD mounted (30,436 MiB reported); `/pictures` created.
- Battery estimate, battery voltage, SHT40 readings and RTC read successfully.
  Charging reports `unknown` from the current driver; RTC is not synchronised.
- Live HTTP checks passed for pages, tokens, invalid paths, Wi-Fi configuration
  isolation, malformed uploads and oversized picture rejection.
- Six-colour test card stored, readback verified, listed and loaded for display:
  `/pictures/2026/09/20260916_123643_76e8b5aa.p6`. This file is intentionally retained.
- Real Chromium browser checks passed at a mobile viewport: local scripts,
  PNG decode, aspect-preserving fit with white borders, quantized preview,
  dither controls, diagnostics and absence of horizontal overflow/script errors.
- Host converter and C++ format tests passed. Firmware build passed.

A malformed raw upload initially crashed the Arduino upload callback. The final
implementation explicitly rejects non-multipart requests before accessing the
file-upload object; the live regression check passes.

0.2.1 was uploaded successfully over Wi-Fi after physical C-button unlock. The
device rebooted into 0.2.1, reconnected using saved credentials, and reported a
software restart. It includes the button diagram and reset-cause diagnostics.
0.3.0 was installed over Wi-Fi and confirmed after restart. It adds sound/LED
motifs, persistent mute, previews, central versioning and the OTA power guard.
Live telemetry reported 4,758 mV input, 3,772 mV battery, 59% estimated charge,
and `otaPowerOk: true`; OTA correctly returned to locked after reboot.
Host policy tests cover threshold boundaries, missing/invalid readings, low
battery and loss of external power with/without a sufficient battery. Actual
low-battery and mid-upload power-loss experiments have not been performed.
Visual colour
accuracy, audible speaker output, LEDs, battery runtime, power-loss durability,
full-card handling and automatic rollback have not been established by these checks.

## 0.4.0 reader — host checks

- Firmware build passed with the pinned Arduino/M5 libraries (about 1.13 MB app).
- Address/undefined-behaviour-sanitized C++ pagination tests passed, including
  500 randomized books that preserve every non-whitespace character, wrapping,
  page offsets, oversized words and invalid text/IDs.
- Node import tests passed for paragraph reflow, poetry, Latin normalization,
  unsupported characters and size boundaries. Existing converter, image-format
  and OTA-power policy tests also passed.
- Real Chromium running the embedded Books HTML/JavaScript against mocked HTTP
  endpoints passed: import preview, multipart body, library read controls, font
  actions, busy state, unsupported text rejection and mobile layout.
- `tests/reader_device.py BASE --exercise` is an opt-in live acceptance test. It
  retains an original test book and tests actual SD upload/download, page changes,
  bookmark recall, font reflow and NVS reload, then displays the font sampler.

At this checkpoint, 0.4.0 has **not** been installed or hardware-validated: the
device is running 0.3.0 and OTA is locked pending a physical C-button unlock.
Physical typography, button gestures and restart restoration need device testing.
