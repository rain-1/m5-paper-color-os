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

0.4.0 was installed over Wi-Fi after physical C-button unlock. The device
reconnected with its saved credentials and reported version 0.4.0 and a software
restart. OTA returned to locked. At installation the device reported no USB input,
96% battery and 4,074 mV battery voltage, satisfying the battery-only update guard.

Live reader checks established:

- Original sample book `/books/c69ab9f5.txt` uploaded to SD and downloaded unchanged.
- Sample opened as nine pages in default 12 pt serif; next/previous and page jump
  worked. A bookmark was stored and recalled at byte offset 432.
- Changing to 18 pt serif produced 17 pages and kept byte offset 432 (page 3);
  changing back returned to page 2 of 9 at the same offset.
- Menu/continue and reopening the book restored page 2 and the bookmark. Reopen
  reads the actual per-book NVS record. The full live acceptance script passed,
  and the font comparison sheet was left on screen for visual inspection.
- Live Chromium loaded the device's Books page, read reader state, prepared a
  Latin-accented TXT preview correctly, and passed mobile layout/script checks.
- Startup display refresh can temporarily block the SD bus even while reader
  `busy` is false. The acceptance script now retries that expected HTTP 409.

Physical typography, button gestures and restart restoration still need user/device
testing; HTTP state alone does not establish what the panel looks like.

### Known LED failure

The user reports no visible output from LED tests. Inspection of the pinned
M5Unified `LED_Strip_Class.cpp` found the legacy RMT (ESP-IDF 4.x) initialization
branch is empty and returns false. This project uses Arduino 2.0.17 / ESP-IDF 4.x,
so the current `M5.Led` calls silently fail. The lab endpoint does not check that
failure and incorrectly returns success. Firmware 0.4.0 does **not** fix this;
it needs a compatible LED backend and truthful test status before LED feedback
can be considered implemented on hardware. Audible cues remain independent.

## 0.5.0 accelerated full-refresh experiment — normal baseline established

- Firmware build passed. Native palette tests passed for RGB565 inputs and all
  65,536 possible input values; existing converter/import tests passed.
- Chromium test of the embedded experiment page with mocked HTTP passed:
  baseline/acknowledgement requirements, one-shot requests, busy/fault controls,
  timing display and mobile layout. This is not a hardware timing test.
- Source review checked the display bus's D/C polarity, canvas RGB565 reads,
  full-frame byte count, normal power sequence, BUSY waits and default-clock write.
- Installed 0.5.0 over OTA after physical unlock; saved Wi-Fi reconnected, reader
  place/bookmark restored and OTA relocked. USB input was 4,746 mV.
- One normal-clock comparison chart completed: 35,368 ms total, 33,800 ms refresh
  BUSY interval. No fault was latched; default-clock restoration write completed.
- User-triggered accelerated chart completed: 18,627 ms total and 16,960 ms BUSY,
  versus 35,368 ms total at normal timing (about 47% less elapsed time).
  Telemetry reported no fault and the default-clock restoration write completed.
  The user reports that image quality looked fine. This is a single chart, not
  a long-term reliability or temperature-range validation.

## 0.6.0 expanded refresh tests — deployed, normal serif baseline

- Build, native palette/pattern bounds tests, and mocked Chromium UI checks pass.
- UI tests cover pattern/variant payloads, timing history, busy selection locks,
  baseline requirement, acknowledgement and fault handling.
- Added full-page serif/sans/inverse text, checkerboards, colour blocks, white
  cleanup and alternate variants. No new hardware timing or voltage setting;
  the same measured PLL 0x07 experiment remains one-shot and off by default.
- Installed 0.6.0 over OTA after physical unlock; confirmed version, Wi-Fi
  reconnection and relocked OTA. Live API rejected invalid patterns/variants and
  accelerated testing before the first completed normal reference.
- Live Chromium passed seven-pattern/two-variant controls, state polling, script
  checks and mobile layout without triggering additional refreshes.
- Normal serif variant 1 completed in 35,412 ms total / 33,839 ms BUSY, no fault,
  default-clock restoration reported. The timing history correctly identifies
  pattern, variant and mode. This page is left displayed for visual comparison.
- Accelerated transitions between the new full-page patterns await user testing;
  no claim of visual quality is made from timing/status telemetry alone.

## 0.12.0 picture menus and LED idle-off — host verified, pending installation

- Firmware build and host menu dispatcher tests cover the additional reader,
  picture-month and picture-list menus. Selection moves schedule no screen work.
- ASan/UBSan tests cover bounded newest-first ordering, month order, every preview
  coordinate, existing picture format validation and menu pagination/gestures.
- LED timer tests cover exactly 30 seconds, waking the unchanged selection,
  menu re-entry and millis rollover. Browser converter/reader regressions pass.
- On-device SD browsing, preview legibility and physical LED timeout remain to
  be verified after installation. No display timing changes or hardware writes.

## 0.11.0 LED menu selection — host verified, pending installation

- Host tests exercise delayed singles/double select/back, long-hold cancellation,
  timer rollover, wraparound and all library sizes through 64 entries.
- The actual Reader::request dispatcher is compiled against a host scheduler fake:
  repeated moves in every menu schedule zero screen requests; choose/back and
  reading-page turns schedule rendering; busy/fault guards remain effective.
- Firmware build and reader/converter regression tests pass. Hardware LED colour,
  colour-block legibility, gesture feel and physical menu behaviour await testing.
- Replaced all M5.Led calls with a single main-task-owned legacy RMT implementation;
  no changes to display refresh clocks, voltage commands or waveform tables.

## 0.10.0 five Wi-Fi profiles — installed

- OTA installed successfully; live API confirmed 0.10.0, reconnection using
  existing credentials and OTA relocked. Multi-location failover remains untested.

- Firmware build passed (1,161,737-byte application); converter regression passes.
- ASan/UBSan profile tests cover the legacy 97-byte credential layout, migration
  into the versioned store, five-slot limit, explicit replacement, in-place password
  updates preserving labels, forgetting/last-index adjustment, malformed records,
  duplicate SSIDs and strongest-untried-visible selection.
- Reviewed asynchronous scan/connection transitions, scan cancellation, refreshed
  timeout timestamp, bounded attempt set, and setup-hotspot/token gating of edits.
- Migration on actual NVS, real multi-access-point failover, full-list management
  and captive-browser rendering still require device testing after installation.
- No Wi-Fi settings changed on the running device during host development.

## 0.9.1 prepared-picture dithering fix — host verified, pending installation

- Shared SD/RAM picture branch now selects no-dither conversion for transfer,
  waits for completion, then restores quality mode. Driver mode selection only
  affects pixel conversion, not the physical refresh clock/waveform.
- Host test compiles the pinned driver's actual palette/no-dither functions:
  all 36 ordered pairs of the six prepared colours preserve the exact expected
  native indices across every position of a 400×600 frame.
- Converter tests and firmware build passed (1,153,609-byte application).
- Physical solid-picture comparison awaits installation and user inspection.

## 0.9.0 temporary display — installed

- OTA accepted after physical unlock; live API confirmed 0.9.0, reconnection
  and `otaSeconds: 0` after restart.

- Firmware build passed (1,153,561-byte application); converter tests pass.
- Chromium mocked-device tests verify Display only sends the exact prepared
  bytes with `target=display`, adds no gallery entry, reports no SD save, and
  remains usable after reload with a missing card. Existing save tests pass.
- Reviewed backend branch: validated upload transfers RAM ownership under the
  display mutex and returns before all SD operations. Draw consumes/frees RAM.
- No physical display-only request or firmware installation performed yet.

## 0.8.0 gallery and colour treatment — installed

- OTA accepted after physical unlock; live API confirmed version 0.8.0, Wi-Fi
  reconnection and `otaSeconds: 0`. No experimental refresh triggered.

- Firmware build passed (1,152,237-byte application). No device write or refresh.
- Converter tests cover OKLab reference colours, neutral/saturation/lightness
  controls, deterministic six-colour packing, dithering and preview parity.
- Real Chromium with mocked routes verifies controls/reset/RGB comparison,
  preserved white borders, rotation, lazy thumbnails, new-upload local previews,
  queued versus saved-but-not-displayed feedback, retry buttons and mobile layout.
- Sanitized C++ tests verify every thumbnail sample and existing file validation.
- Hardware SD/display integration and physical colour quality remain unverified.

## 0.7.0 landscape uploads — host verified, pending installation

- Firmware build and existing converter tests passed. Sanitized C++ picture
  validation tests accept legacy and new portrait/landscape paths, reject invalid
  markers/date directories/suffixes, and cover orientation parameter validation.
- Real Chromium with mocked device routes verified clockwise red/blue pixel
  placement, aspect-preserving fit, white borders, repeated checkbox toggles,
  120,016-byte packed upload contents, matching `p`/`l` request markers, legacy
  gallery entries and mobile layout. These checks do not write the physical SD.
- No firmware upload or display operation was performed for this change yet.
  Normal versus accelerated display timing was not changed.
