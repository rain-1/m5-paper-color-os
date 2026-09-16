# Paper OS

Firmware for **M5Stack PaperColor (C151)**: splash/Wi-Fi setup, browser image
conversion, an SD gallery, a TXT book reader, a device lab, and browser firmware updates. Built with Arduino/FreeRTOS,
M5Unified and M5GFX. This is a foundation for an appliance-style OS, not Linux.

## Build

```sh
python3 -m venv .venv
.venv/bin/pip install platformio==6.2.0
.venv/bin/pio run
```

The platform and hardware libraries are pinned in `platformio.ini`. First build
downloads the compiler and libraries. Firmware output is
`.pio/build/papercolor/firmware.bin`; uploading also needs the generated bootloader
and partition table, so use PlatformIO rather than flashing that file at address 0.

## Flash

Connect a USB data cable. Hold the side power/reset button for approximately three
seconds to enter download mode, as described in the [hardware guide](https://docs.m5stack.com/en/core/PaperColor).

```sh
.venv/bin/pio device list
.venv/bin/pio run -t upload --upload-port /dev/ttyACM0
.venv/bin/pio device monitor --port /dev/ttyACM0
```

Replace the port with the actual device. Restart using the power button if the
device stays in download mode. Upload replaces the factory application; M5Stack
links its factory restore tooling from the hardware guide.

For remote development, forward USB to the build machine so the device appears
as a local serial port. USB re-enumeration during reset may require reattaching
the forwarded device. A serial bridge must support esptool traffic; TCP forwarding
alone does not provide USB reset controls. Manual download mode may be needed.
Alternatively, run the upload command on the computer physically connected to it.

## First boot

1. Wait for the Paper OS setup screen. Full colour refresh takes about 15–30 seconds.
2. Join `PaperOS-XXXXXX` using the random password shown on the screen.
3. Open the captive portal, or browse explicitly to **http://192.168.4.1**.
4. Enter the exact network name and password for a 2.4 GHz network.
5. Stay on the setup network while the status page checks the connection. Some
   phones ask whether to remain on a network without internet; choose to stay.
6. Once connected, credentials are saved and the hotspot closes after 20 seconds.

An empty password selects an open network. WPA/WPA2 personal passphrases of 8–63
bytes are supported; enterprise authentication and raw 64-character PSKs are not
part of this milestone. Network names are entered manually, including hidden SSIDs.
No internet or cloud account is required. Successful connection means a Wi-Fi
association with an assigned IP address, not a check of internet availability.

On subsequent boots, a splash appears while connecting. Failure after 30 seconds
opens setup. Hold the **lower-left button (B)** for 2.5 seconds to reopen setup, or hold
it during boot to skip the saved network. Failed replacement credentials do not
overwrite the previous working configuration. Sustained connection loss also
opens setup. The display updates only on state changes; HTTP/DNS continue on a
separate task from display refreshes.

## Pictures, device lab and wireless updates

Once connected, open the IP address shown on the screen in your browser. `/gallery`
contains the converter, `/books` the TXT library, and `/device` diagnostics and updates.
Press top **C** to enter the reader menu. Outside the reader, **A** refreshes status.
Inside the reader, **A/B** turn pages or navigate and **C** chooses. With the display
upright and USB at the bottom, **A** is upper-left, **B** is lower-left, and **C** is
the single button on the top edge. Hold **B** to reconfigure Wi-Fi.
Hold **C** for 2.5 seconds to unlock firmware updates for two
minutes; a short beep and green LED flash confirm that you can release it.
The lab page shows the remaining time.

The [reader guide](docs/reader.md) covers TXT imports, five fonts and a font test
sheet, automatic saved positions, bookmarks, offline reading and physical controls.
The first reader normalizes Latin text to ASCII and has a 1 MiB prepared-book limit.
Page turns still require the colour panel's full refresh; fast partial refresh and
deep-sleep battery optimisation are not implemented.

Firmware 0.5.0 adds an opt-in `/refresh-test` comparison lab for experimental
accelerated **full-screen** refresh. It does not enable fast timing in normal
reading or pictures. See the [experiment instructions and limits](docs/partial-refresh-investigation.md).

The image converter accepts JPEG/PNG, up to 12 MiB, 24 million pixels, and 12,000
pixels on either side. It checks dimensions before decoding, fits the entire image
to 400×600 with white borders, and offers solid colour quantization or
Floyd–Steinberg dithering. Conversion happens in your browser. Only the packed
120,016-byte file is uploaded. Dates use the browser's local time:
`/pictures/YYYY/MM/YYYYMMDD_HHMMSS_p_randomhex.p6` (portrait) or
`YYYYMMDD_HHMMSS_l_randomhex.p6` (landscape). The **Landscape** checkbox rotates
the picture 90° clockwise before fitting and quantization; the preview shows the
stored orientation. Both modes keep aspect ratio and use the same 400×600 format.
Older filenames without a `p`/`l` marker remain supported. Clients omitting the
upload `orientation` parameter default to `p`. Files are verified by readback
before renaming a temporary file. Interrupted writes may leave `.tmp` files,
which are not shown in the gallery. The gallery lists up to 100 files per month.
Existing files are never overwritten. Originals are not stored.

The `.p6` format is a 16-byte header followed by two palette indices per byte,
high nibble first, in row order. Header: ASCII `P6I1`, little-endian uint16 width
400 and height 600, then eight reserved zero bytes. Indices 0–5 mean black,
white, yellow, red, blue and green. Firmware checks the exact length, header and
every pixel index before saving or displaying. Palette calibration is future work.

SD and display share SPI, so storage operations return a retry message while the
screen is refreshing. Insert/remove the card with power off, then restart. SD
mount failure leaves networking and diagnostics available; firmware never formats
the card. Saved pictures survive reboot, but automatic restoration of the last
displayed picture is not yet implemented.

For OTA, build with `pio run`, open `/device`, hold C and upload
`.pio/build/papercolor/firmware.bin`. No SD card is needed. The firmware writes the
inactive 6.25 MiB app slot, checks completion via the ESP32 updater, switches the
boot slot only after validation, and restarts. Use only Paper OS builds with this
partition layout. NVS Wi-Fi credentials and SD files are preserved. This is not
signed firmware or automatic rollback: a valid but faulty app may require USB
recovery. SD-card firmware installation is a proposed recovery feature, not yet
implemented. Keep sufficient battery charge or connect power during updates.
The device enforces a power guard: measured input of 4.6–5.5 V, or both at least
30% estimated battery and 3.6–4.35 V battery voltage. Without a confirmed adequate
source, it refuses to write. Three samples are checked before starting and before
activation, and power is checked once per second while receiving firmware. Loss
of adequate power aborts the inactive-slot update and keeps the existing boot
selection. These conservative thresholds reduce risk but cannot guarantee against
sudden power loss or incorrect sensor readings. Charging status is not used.

After unlocking C, the development machine can also upload with:
`python3 scripts/ota_upload.py http://DEVICE_IP .pio/build/papercolor/firmware.bin`.

The device lab shows battery estimate/voltage/charging, button press counts,
temperature/humidity, RTC, Wi-Fi and free memory. LED tests last five seconds;
the speaker tone lasts 200 ms at low volume. It does not record microphone audio.
Event sounds can be muted persistently and previewed in the lab. See the
[sound and light language](docs/feedback.md) and [firmware changelog](CHANGELOG.md).

## Development boundaries

- Credentials are stored as one NVS blob and never logged or sent back in pages.
  NVS/flash encryption is not enabled: physical flash access can recover them.
- Wi-Fi credential configuration is restricted to the hotspot interface. Forms
  use a fresh session token. The hotspot
  has a new random WPA password each time setup starts.
- Captive detection uses wildcard DNS and HTTP redirects. HTTPS interception is
  not supported; the explicit HTTP address is the fallback.
- The gallery and lab are HTTP services for a trusted local network, without user
  accounts or TLS. Mutation requests require a page token; anyone on the LAN can
  load the page. OTA additionally requires the physical C-button unlock. Do not
  expose these services to the internet.
- This milestone stays awake for Wi-Fi and buttons. Battery optimisation and
  more peripheral controls are future work.
- Long refreshes mean the screen can lag behind network state by one refresh.

## Hardware acceptance checks

Build verification does not replace these checks on a real PaperColor:

- Fresh boot: colours/orientation/text are correct and hotspot credentials work.
- Phone captive detection and explicit HTTP fallback both show the form.
- Wrong password: timeout and retry work without restarting the device.
- Correct password: success page, connected screen and hotspot shutdown occur.
- Power cycle: saved network reconnects without entering credentials.
- Hold B: setup reopens; failed replacement preserves the old credentials on reboot.
- Turn the router off: setup opens after the connection-loss timeout.
- Exercise open and hidden networks; verify the portal remains responsive during
  a screen refresh and credential configuration remains inaccessible via the router-facing address.
- Save an image, display it, restart and reload its month. Check rejected malformed
  uploads, missing cards, full cards, interrupted uploads and display-busy retries.
- Compare battery readings with/without USB power; check buttons, LEDs and tone.
- Reject locked, oversized and incomplete OTA uploads. With C unlocked, upload a
  known build and confirm reboot, Wi-Fi reconnect and SD file retention.

Host checks: `node tests/converter.test.cjs` and
`g++ -std=c++11 -Iinclude tests/picture_format.cpp -o /tmp/paper-format-test && /tmp/paper-format-test`.
Power guard: `g++ -std=c++11 -Iinclude tests/ota_power.cpp -o /tmp/paper-ota-power-test && /tmp/paper-ota-power-test`.
See [hardware inventory and design](docs/design.md) for the longer-term direction.
See [validation record](docs/validation.md) for performed checks and remaining hardware tests.

Hardware references: [PaperColor](https://docs.m5stack.com/en/core/PaperColor) and
[official Arduino example](https://docs.m5stack.com/en/arduino/papercolor/program).
