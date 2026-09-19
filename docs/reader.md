# TXT reader and colour menus — firmware 0.12.0

Open `/books` on the device's local address. Import a `.txt`, inspect the prepared
preview, save it, then choose **Read**. No internet or cloud service is involved.
The device reads from SD; reading and physical navigation work without Wi-Fi.

## Controls

With the display upright and USB at the bottom:

| Button | Short press, on release | Hold 2.5 seconds |
| --- | --- | --- |
| A, upper-left | Previous page / previous menu item | Bookmark current page |
| B, lower-left | Next page / next menu item | Wi-Fi setup |
| C, top edge | Open reader menu / choose item | Unlock OTA for 120 seconds |

The separate lower side power/reset button is **not C**. Long presses do not also
trigger short-press actions. While a reader refresh is pending, new reader actions
are rejected, preventing accidental queued page turns. Bookmarking does not need
a screen refresh; the saved sound confirms it. Sounds can be muted in `/device`.

Menus are fixed coloured blocks with a colour name and row number; there is no
moving on-screen highlight. Both RGB LEDs show the selected row. Root menu:
red Pictures, yellow Reader, green Library, blue Device information, white Voice notes.
Reader opens Continue reading / Fonts / Bookmarks. Fonts opens Choose reading
font / Font comparison sheet; Bookmarks opens Save / Recall.
Voice notes opens red Record a note / yellow Saved recordings. Its dated saved
note list uses the same selection colours and More pages. C or double-A starts
speaker playback without a display refresh; any user-button release stops audio.
Red blinking means capture; blue blinking means playback. Double-B returns to
the parent menu when idle. Recording failures show a short on-screen explanation.

In menus, single A/B moves up/down without scheduling any display refresh.
Double A chooses; double B goes back. C also chooses immediately. A single tap
is delayed 350 ms to distinguish doubles; long holds cancel pending taps.
The LED is steady while choosing and takes priority over notification flashes.
After 30 seconds without menu activity it switches off; pressing a menu button
wakes the current colour. The existing press action still applies (single moves,
double chooses/back). This is LED power saving, not whole-device sleep.
Selecting or exiting a menu may refresh the screen. Back from the root returns
to the root itself; nested menus return to their parent.

Lists of up to five items use all five colour rows. Larger libraries show four
books plus an explicit More choice, which refreshes to the next group (wrapping
after the last). Moving across the last row only wraps the LED, not the screen.
Browser previous/next actions use the same no-refresh menu dispatch. Reading
page turns remain immediate single A/B actions, stopping at the book boundaries.
C returns from the comparison sheet or information screen to the root menu.

## Typography and pagination

Five bundled GFX fonts: FreeSerif 9/12/18 pt, FreeSans 12 pt, FreeMono 12 pt.
Default is FreeSerif 12 pt. The on-device font comparison sheet is the useful
reference; browser typography is only an import preview. Page layout measures
the actual selected device font, wraps words, splits overlong words, preserves
blank paragraphs, and reserves space for title, page count and battery estimate.

Positions and one optional bookmark **per book** are byte offsets, not page
numbers. Changing font reflows the book and selects the page containing the old
offset. This may move back a few lines. Each completed reading refresh saves the
current position in internal NVS, along with the last book. Normal startup with
saved Wi-Fi credentials opens the colour menu, not the book. Continue reading
opens the saved book and position on demand. Network connection
changes do not replace an active reader page. Hold B explicitly to see setup.
Power loss during refresh can return to the preceding saved page. This is not
a power-failure guarantee for either NVS or the FAT filesystem.

## Import and limits

- Source file: up to 2 MiB; prepared text: up to 1 MiB; library: 64 books.
- UTF-8, Windows-1252, UTF-16LE and UTF-16BE decoding happens in the browser.
- Optional paragraph reflow joins single hard-wrapped lines. Disable for poetry
  or deliberately formatted text. Blank lines separate paragraphs.
- This first font set supports printable ASCII. Common Latin accents are
  transliterated; smart quotes/dashes/ellipsis are simplified. Unsupported scripts
  and control characters are rejected explicitly, not silently dropped.
- The normalized preview is what is stored. Original files stay on the computer.
- The firmware independently validates size, allowed characters and nonempty text.
- Pagination is capped at 16,000 pages. Pathological files with huge numbers of
  blank lines may import successfully but be refused when opened. Split volumes.
- No EPUB/PDF, illustrations, chapter detection, search or annotations yet.

Files are immutable `/books/<8-hex-id>.txt` plus a `.title` metadata file. Uploads
use temporary files, text readback verification, and rename on completion. IDs
are random and existing files are not overwritten. Interrupted writes can leave
ignored `.tmp` files. Download the prepared TXT from the library. To archive or
remove books, power off before removing the SD card, and move/delete matching
`.txt`/`.title` pairs on a computer. There is no browser delete action in this release.

## Refresh and battery reality

The pinned M5GFX `Panel_ED2208` implementation sends the entire framebuffer and
performs the panel refresh even for rectangle display requests. Its fast/fastest
modes change conversion, not a separate fast partial-refresh waveform. The reader
uses no-dither conversion, as do prepared pictures. Colour menu blocks use the
panel's native red/yellow/green/blue/white palette. No accelerated timing is enabled.
Allow roughly 15–30 seconds per physical update. Smaller rectangles do **not**
currently produce fast page turns. This is a colour picture panel, not a typical
fast monochrome e-reader panel.

The screen and SD share a mutex because they share SPI. SD operations return a
retry response during refresh; networking and button sampling stay responsive.
One display task owns rendering and reader state, with short mutex-protected
snapshots for HTTP. The book lives in PSRAM; only page offsets are indexed.
The current firmware stays awake. Deep sleep and long e-reader battery life are
not implemented; unplugging works, but does not imply weeks of runtime.

## Validation

```sh
node tests/reader.test.cjs
g++ -std=c++11 -fsanitize=address,undefined -Iinclude tests/book_layout.cpp -o /tmp/paper-book-layout-test
/tmp/paper-book-layout-test
.venv/bin/pio run
```

Host tests cover normalization, encoding-independent text rules, bounds, word
wrapping, paragraphs, oversized words, offset lookup and randomized preservation
of every non-whitespace character. Hardware acceptance still includes subjective
font legibility, long-press gestures, restart restoration, SD failure cases,
and battery runtime. See `docs/validation.md` for checks actually performed.
