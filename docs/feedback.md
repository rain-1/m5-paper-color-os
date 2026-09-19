# Sound and light language

Use short, learnable motifs rather than long startup tunes. Sound confirms a
completed transition, not every button press or polling request. Muting is stored
in internal NVS and leaves LED feedback enabled. The lab offers previews without
performing the corresponding action.

| Event | Notes (Hz) | Meaning | LED |
|---|---|---|---|
| Wi-Fi connected | 523 → 659 | Small upward movement: ready | Blue |
| Picture saved and verified | 659 → 880 | Upward confirmation: done | Green |
| Action failed | 659 → 440 → 330 | Downward movement: needs attention | Red |
| Device busy | 440, 440 | Same note repeated: wait and retry | Amber |
| Update mode unlocked | 523 → 659 → 1047 | Three upward steps: release the button | Green |
| Firmware accepted | 523 → 659 → 784 → 1047 | Longer upward completion before restart | Green |

Notes last 70–180 ms with 65 ms gaps. LEDs clear after one second. Playback runs
from the main loop without blocking sleeps. A new cue replaces any previous cue;
the e-paper task can request feedback through a queue without touching audio/I2C.
No repeated low-battery alarms or progress jingles. Calibrate loudness on the real
speaker before adding volume presets. Future power/sleep cues should reuse these
directions and rhythms instead of introducing a new tune for every feature.

The explicit speaker hardware test is separate from muted event cues. Device lab
LED tests last five seconds; automatic event flashes last one second.

Since 0.11.0, menu selection owns the LEDs while a menu is idle: steady red,
yellow, green, blue or white, matching the labelled screen blocks. Event sounds
still play but their LED flashes cannot obscure the selection. Lab LED tests
are rejected while choosing from a menu. Exit the menu to test individual LEDs.
The pinned M5Unified IDF4 encoder was unimplemented; a two-pixel GRB driver now
uses the Arduino legacy RMT API on GPIO21 at bounded brightness. The main task
owns all LED writes. `/api/device` reports `ledReady`; physical colour/order
and visibility still need checking on installation.

From 0.12.0, menu LEDs turn off after 30 seconds without navigation or a physical
menu-button press. The choice is retained. The next press wakes the light and
retains its normal single/double/hold behaviour. Transient notification flashes
do not override the idle-off menu state. The ESP32 remains awake.
