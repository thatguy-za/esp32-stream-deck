# Stream Deck Mini USB protocol

Elgato doesn't publish this protocol. Everything below was pulled from the
source of two independent open-source implementations, not from memory or
guesswork:

- [python-elgato-streamdeck](https://github.com/abcminiuser/python-elgato-streamdeck)
  — `src/StreamDeck/Devices/StreamDeckMini.py`
- [`elgato-streamdeck` Rust crate](https://github.com/OpenActionAPI/rust-elgato-streamdeck)
  — `src/info.rs`

Where the two disagree or a detail wasn't visible in the excerpts fetched,
it's marked "TBD — confirm on hardware" rather than filled in with a guess.

## Identification

| | Value |
|---|---|
| Vendor ID (Elgato) | `0x0FD9` |
| Product ID — Stream Deck Mini (original) | `0x0063` |
| Product ID — Stream Deck Mini Mk2 | `0x0090` |

Other models for reference (not used by this project, but useful if the unit
turns out to be a different revision than expected):

| Model | PID |
|---|---|
| Stream Deck Original (Rev 1) | `0x0060` |
| Stream Deck Original (Rev 2) | `0x006D` |
| Stream Deck XL (Rev 1) | `0x006C` |
| Stream Deck XL (Rev 2) | `0x008F` |
| Stream Deck Mk2 | `0x0080` |
| Stream Deck Mk2 Scissor Keys | `0x00A5` |
| Stream Deck Mini Discord Edition | `0x00B3` |
| Stream Deck Neo | `0x009A` |
| Stream Deck Pedal | `0x0086` |
| Stream Deck Plus | `0x0084` |
| Stream Deck Plus XL | `0x00C6` |

## Key layout

- 6 keys, arranged **3 columns × 2 rows**
- Each key is an independent LCD tile, **80×80 px**
- Image format: **24-bit BMP**
- Rendering: image is flipped vertically and rotated 90° before upload (i.e.
  the tile's native scan order doesn't match a naively-oriented source image —
  apply the same transform the Python lib does before sending pixel data)

## Key-press report (device → host, interrupt IN)

7 bytes total:

| Offset | Meaning |
|---|---|
| 0 | Status byte |
| 1–6 | One byte per key (index 0–5), boolean pressed/released |

TBD — confirm on hardware: exact meaning of the status byte (offset 0) and
which interrupt IN endpoint/report ID this arrives on. Confirm during M1's
descriptor dump + first live report capture.

## Image upload report (host → device)

1024-byte reports, sent as one or more "pages" per key (a full 80×80 24-bit
BMP is larger than 1008 bytes, so it's split across several of these):

| Offset | Length | Meaning |
|---|---|---|
| 0 | 1 | Report ID — `0x02` |
| 1 | 1 | Command — `0x01` |
| 2 | 1 | Page number (0-indexed, increments per page of this image) |
| 3 | 1 | `0x00` (reserved/unused) |
| 4 | 1 | `0x01` if this is the final page for this key, else `0x00` |
| 5 | 1 | Key index **+ 1** (i.e. key 0 → byte value `1`) |
| 6–15 | 10 | `0x00` (reserved/unused) |
| 16–1023 | 1008 | BMP payload chunk for this page |

TBD — confirm on hardware: whether this 1024-byte report actually rides an
**interrupt OUT** endpoint or a **control-transfer SET_REPORT** (the Python
library's `write_feature()`/`write()` split isn't fully visible from the
excerpts pulled) — this determines which ESP-IDF `usb_host_hid` call to use
in the M2 image-writer, and is exactly what M1's descriptor dump is for.

## Feature reports (host → device, control transfer SET_REPORT)

**Reset:**

| Offset | Value |
|---|---|
| 0 | Report ID `0x0B` |
| 1 | `0x63` |

**Set brightness:**

| Offset | Value |
|---|---|
| 0 | Report ID `0x05` |
| 1 | `0x55` |
| 2 | `0xAA` |
| 3 | `0xD1` |
| 4 | `0x01` |
| 5 | Brightness percent, `0`–`100` |

Both are sent as 17-byte feature reports via `write_feature()` in the Python
library (HID class `SET_REPORT`, report type `FEATURE`).

## Open questions for M1

1. Real endpoint layout of the physical unit (interrupt IN/OUT vs.
   control-only) — dump full device/config/interface/endpoint descriptors on
   enumeration.
2. Status byte meaning in the key-press report.
3. Whether this specific unit is PID `0x0063` or `0x0090` (both are called
   "Stream Deck Mini" but may differ slightly in protocol/firmware revision).
