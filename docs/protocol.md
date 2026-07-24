# Stream Deck USB protocol

Elgato doesn't publish this protocol. Everything below was pulled from the
source of two independent open-source implementations, not from memory or
guesswork:

- [python-elgato-streamdeck](https://github.com/abcminiuser/python-elgato-streamdeck)
  — `src/StreamDeck/Devices/{StreamDeckMini,StreamDeckOriginal,StreamDeckOriginalV2}.py`
  and `DeviceManager.py` (for the PID→class mapping)
- [`elgato-streamdeck` Rust crate](https://github.com/OpenActionAPI/rust-elgato-streamdeck)
  — `src/info.rs`

This project targets three device families, selectable by the user at
ESPHome setup time (Milestone 3):

| Family | Models | PIDs |
|---|---|---|
| **Mini** | Stream Deck Mini, Mini Mk2 | `0x0063`, `0x0090` |
| **Original** | Stream Deck (1st gen, 2017) | `0x0060` |
| **Original V2** | Stream Deck (2019/"V2"), MK.2, MK.2 Scissor, MK.2 Module | `0x006D`, `0x0080`, `0x00A5`, `0x00B9` |

**Original V2 and MK.2 are the same protocol** — the python library maps
every one of those PIDs to a single `StreamDeckOriginalV2` class, since MK.2
is a cosmetic refresh of the 2019 "V2" board, not a protocol change. This is
what "the normal Stream Deck" (as opposed to the Mini) means in this
project's code: the Original V2 family, since that's what's still sold new.
1st-gen Original (`0x0060`) is kept as a separate profile because its image
format and feature-report bytes genuinely differ (see below) — support for
it is best-effort since it's an older/rarer unit.

Where a detail wasn't visible in the excerpts fetched, it's marked
"TBD — confirm on hardware" rather than filled in with a guess.

## Identification

| | Value |
|---|---|
| Vendor ID (Elgato) | `0x0FD9` |

Other models for reference (not targeted by this project, but useful if a
unit turns out to be a different revision than expected):

| Model | PID |
|---|---|
| Stream Deck XL (Rev 1) | `0x006C` |
| Stream Deck XL (Rev 2) | `0x008F` |
| Stream Deck XL V2 Module | `0x00BA` |
| Stream Deck Mini Discord Edition | `0x00B3` |
| Stream Deck Mini Mk2 Module | `0x00B8` |
| Stream Deck Neo | `0x009A` |
| Stream Deck Pedal | `0x0086` |
| Stream Deck Plus | `0x0084` |
| Stream Deck Plus XL | `0x00C6` |

## Per-family layout and image format

| | **Mini** | **Original** | **Original V2 / MK.2** |
|---|---|---|---|
| Keys | 6 | 15 | 15 |
| Grid | 3 cols × 2 rows | 5 cols × 3 rows | 5 cols × 3 rows |
| Key size | 80×80 px | 72×72 px | 72×72 px |
| Image format | 24-bit BMP | 24-bit BMP | JPEG |
| Flip | vertical only | horizontal + vertical | horizontal + vertical |
| Rotation | 90° | 0° | 0° |

## Key-press report (device → host, interrupt IN)

**Mini**: 7 bytes — offset 0 is a status byte, offsets 1–6 are one
boolean byte per key.

**Original**: 16 bytes — offset 0 is a status/report-id byte (discarded
by the reference implementation), offsets 1–15 are one boolean byte per key.

**Original V2 / MK.2**: 19 bytes — a 4-byte header, then offsets 4–18 are
one boolean byte per key (15 keys).

TBD — confirm on hardware for whichever unit(s) you actually test: exact
meaning of the leading header byte(s), and which interrupt IN endpoint/report
ID this arrives on. Confirm during M1's live report capture.

## Image upload report (host → device)

All three send the image as one or more fixed-size "pages" per key, each
with a small header followed by a chunk of image payload — but the header
layout, page size, and command byte differ per family:

**Mini** — 1024-byte reports, 16-byte header, 1008-byte payload:

| Offset | Length | Meaning |
|---|---|---|
| 0 | 1 | Report ID — `0x02` |
| 1 | 1 | Command — `0x01` |
| 2 | 1 | Page number (0-indexed) |
| 3 | 1 | `0x00` (reserved) |
| 4 | 1 | `0x01` if final page for this key, else `0x00` |
| 5 | 1 | Key index **+ 1** |
| 6–15 | 10 | `0x00` (reserved) |
| 16–1023 | 1008 | BMP payload chunk |

**Original** — 8191-byte reports, 16-byte header (same layout as Mini,
page number stored as `page+1`), remaining bytes are BMP payload padded to
fill the report.

**Original V2 / MK.2** — 1024-byte reports, **8-byte** header, 1016-byte
payload, different field order:

| Offset | Length | Meaning |
|---|---|---|
| 0 | 1 | Report ID — `0x02` |
| 1 | 1 | Command — `0x07` |
| 2 | 1 | Key index (not +1 for this family) |
| 3 | 1 | `0x01` if final page, else `0x00` |
| 4–5 | 2 | Payload length, little-endian |
| 6–7 | 2 | Page number, little-endian |
| 8–1023 | 1016 | JPEG payload chunk |

**Resolved for Mini/Mini Mk2**: image writes are sent via `hid_class_request_set_report()`
(a control-transfer `SET_REPORT` on EP0), the same mechanism confirmed
working for enumeration and used for reset/brightness below — deliberately
chosen over the interrupt endpoint, since that endpoint failed to claim on
a real Mini Mk2 unit tested (512-byte MPS exceeding the ESP32-S3's USB Host
FIFO limit — see the hardware note in `esphome/components/stream_deck/`'s
git history). Not yet confirmed whether the *image actually renders
correctly* on hardware (the flip/rotation transform in
`stream_deck_canvas.cpp`'s `encode_bmp()` is a best-effort implementation of
"flip vertical then rotate 90°" — direction unverified).

Original/Original V2/MK.2 image writes are still genuinely untested — no
JPEG encoder exists in this project yet, so nothing sends them.

## Feature reports (host → device, control transfer SET_REPORT)

**Mini and Original** (identical, 17-byte feature reports):

| | Reset | Set brightness |
|---|---|---|
| Byte 0 | Report ID `0x0B` | Report ID `0x05` |
| Byte 1 | `0x63` | `0x55` |
| Byte 2 | — | `0xAA` |
| Byte 3 | — | `0xD1` |
| Byte 4 | — | `0x01` |
| Byte 5 | — | Brightness percent, `0`–`100` |

**Original V2 / MK.2** (32-byte feature reports, both under report ID
`0x03`):

| | Reset | Set brightness |
|---|---|---|
| Byte 0 | Report ID `0x03` | Report ID `0x03` |
| Byte 1 | `0x02` | `0x08` |
| Byte 2 | — | Brightness percent, `0`–`100` |

## Open questions

1. ~~Real endpoint layout of the physical unit(s) tested~~ — confirmed on a
   real Mini Mk2 (PID `0x0090`): its single HID interface's IN endpoint
   declares a 512-byte Max Packet Size, which the ESP32-S3's USB Host
   controller can't allocate (~408-byte ceiling regardless of FIFO bias
   settings — Full-Speed-only hardware, confirmed via an
   [espressif/esp-idf issue](https://github.com/espressif/esp-idf/issues/14941)
   closed "won't do"). Key-press reports still work despite this (the
   `hid_host_hid` driver's own claim failure is handled non-fatally); image
   writes route around it entirely via control transfers (see above).
2. Leading header byte(s) meaning in each family's key-press report — still
   open for Original/Original V2/MK.2 (untested).
3. Image flip/rotation direction for Mini's `encode_bmp()` — best-effort
   guess, unverified on hardware (see above).
