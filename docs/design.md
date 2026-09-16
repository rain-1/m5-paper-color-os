# Paper OS: hardware and direction

## Hardware inventory

Hardware and pin references: [M5Stack PaperColor specification](https://docs.m5stack.com/en/core/PaperColor)
and [SD example](https://docs.m5stack.com/en/arduino/papercolor/microsd).

| Component | Interface | Current software / intended use |
|---|---|---|
| ESP32-S3R8, dual-core 240 MHz | 16 MB flash, 8 MB octal PSRAM | FreeRTOS application; separate display task |
| 4-inch Spectra 6, 400×600 | SPI: CLK 15, MOSI 13, CS 44, DC 43, BUSY 11, RESET 12 | Static screens and preconverted pictures; slow physical refresh |
| Three buttons A/B/C | Upper-left GPIO 10 / lower-left GPIO 9 / top GPIO 1 | Status / Wi-Fi setup / update unlock; lab counters |
| Power/reset/download button | Board power system | Hardware-managed restart, off and USB download |
| Two RGB LEDs | GPIO 21, PMIC power | Bounded lab tests; later transient status feedback |
| 1 W speaker, ES8311 codec, AW8737A amp | I2S; amp enable 46, audio power 45 | Short tone test; later notifications; no separate beeper listed |
| MEMS mic, ES7210 ADC, AEC hardware | I2S + I2C | Later explicit recording/level tests; currently disabled |
| SHT40 temperature/humidity | I2C 0x44 | CRC-checked readings every ten seconds |
| RX8130CE RTC | I2C 0x32, IRQ 7 | Read-only lab view; later time sync and scheduled wakes |
| microSD | SPI CLK 15, MOSI 13, MISO 14, CS 47 | FAT32 dated pictures; display/SD mutex |
| IR emitter | GPIO 48 | Later explicit transmit tests, no unsolicited transmissions |
| Grove HY2.0-4P | GPIO 4/5, 5 V, ground | Later identified accessory drivers |
| M5PM1, 1250 mAh battery | I2C 0x6e; system SDA 3 / SCL 2 | Battery telemetry; later coordinated sleep and rail control |
| USB-C | USB serial/JTAG and power | Initial installation, logging and recovery |
| 2.4 GHz Wi-Fi | ESP32 radio | Setup hotspot, local web app and OTA |
| Bluetooth LE capability | ESP32-S3 radio, also reported by the flash tool | Not enabled by this firmware |

The device has no documented touchscreen. E-paper retains its image without
continuous refresh, but the current firmware keeps the CPU/radio awake. Battery
percentage is an estimate; do not promise runtime until measured on battery.

## Interaction design

Keep the physical device quiet: the current picture is the normal idle screen.
Use LEDs or short optional tones for transient feedback, avoiding full e-paper
refreshes for progress updates. A status button should show battery, address and
current mode. The browser is the richer surface for importing images, organising
content, inspecting hardware and installing firmware.

The first lab build assigns A to status, hold B to Wi-Fi setup, and hold C to a
two-minute update window. A later image mode can assign short B/C presses to
previous/next image while retaining the long-press system actions. Coalesce rapid
navigation into one pending display request.

## Software boundaries

- Network/configuration: internal NVS owns the tested Wi-Fi credentials. Keep it
  independent of removable storage. Add versioned settings before more preferences.
- Picture storage: SD owns prepared images. Browser owns original decode, fit,
  palette quantization and dithering. The device validates and renders a fixed
  pixel format. Add pagination, thumbnails and indexing for large collections.
- Display: one worker owns graphics. Serialize SD access with display SPI use.
  Network handlers should return promptly with a queued/busy result.
- Hardware services: main task owns buttons and I2C telemetry; tests have bounded
  duration. Microphone tests must start explicitly and indicate recording state.
- Updates: inactive app partition + physical enable window. USB is the current
  recovery path. Add signed releases and boot health/rollback before unattended
  deployment. An SD installer should explicitly select a versioned image, verify
  integrity/compatibility, and consume its manifest to avoid repeated installs.
- Power: add awake/setup/gallery/sleep modes, persist the selected picture,
  arrange RTC/button wake, and disable unused rails. Sleeping makes the web UI
  unavailable, so expose a clear “stay reachable” vs “save battery” choice.

## Next milestones

1. Evaluate palette and dithering on the real panel; support crop/rotate and
   browser thumbnails. Add last-picture restoration and button navigation.
2. Add explicit microphone metering, RTC time setting, IR test codes, and a Grove
   accessory page. Keep unsupported controls clearly labelled.
3. Measure battery use; implement timed sleep, wake scheduling and gallery mode.
4. Harden updates with signed artifacts, compatibility metadata, rollback and
   optional SD recovery. Add pairing/authentication if used on a shared LAN.
