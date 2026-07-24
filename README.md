# esp32-stream-deck

An ESPHome external component for the ESP32-S3 that acts as a **USB host**
for a genuine Elgato Stream Deck, so it can be used standalone — driving
Home Assistant directly over WiFi, with no PC in the loop. Targets both the
**Mini** and the **Original/MK.2** family (see
[docs/protocol.md](docs/protocol.md)); which one a given build talks to is a
user-selectable option in the component's YAML config.

This is the reverse of how ESP32s normally touch USB: instead of showing up as
a device (keyboard, CDC-ACM, etc.), the ESP32-S3 enumerates the Stream Deck
*as its host* and speaks the same protocol Elgato's own software would.

## Usage

```yaml
external_components:
  - source: github://thatguy-za/esp32-stream-deck@main
    components: [stream_deck]

esp32:
  framework:
    type: esp-idf  # required - USB Host mode isn't available under Arduino

api:
  homeassistant_states: true
  homeassistant_services: true

font:
  - file: "gfonts://Roboto"
    id: stream_deck_font
    size: 14

stream_deck:
  model: mini # one of: mini, original, original_v2 (alias: mk2) - see docs/protocol.md
  font_id: stream_deck_font  # only needed if you configure keys: below
  keys:
    - key_index: 0
      entity_id: light.kitchen
    - key_index: 1
      entity_id: switch.fan
```

Each configured key shows a small icon (auto-picked from the entity's domain,
or overridden with `icon:`) colored by the entity's state, its friendly name
underneath, and toggles the entity on press. Icon/color selection has to
happen at compile time (see [docs/protocol.md](docs/protocol.md) for why),
so it's driven from YAML, not fetched live from HA. Image writes (the whole
`keys:` feature) currently only work for the Mini/Mini Mk2 family — see
Status below.

See [stream-deck-example.yaml](stream-deck-example.yaml) for a full real-device
config and [test-stream-deck.yaml](test-stream-deck.yaml) for a minimal
compile-only one, and [docs/hardware.md](docs/hardware.md) before wiring
anything up.

## Status

Confirmed working on real hardware (a Stream Deck Mini Mk2): USB host
enumeration, key-press detection, and now Home Assistant entity binding with
icon/color/name rendering and toggle-on-press. See
[docs/protocol.md](docs/protocol.md) for the reverse-engineered Stream Deck
USB protocol (Mini, Original, and Original V2/MK.2 families) and
[docs/hardware.md](docs/hardware.md) for the board wiring/power plan.

**Known gap**: the Original/Original V2/MK.2 family needs a JPEG encoder
running on the ESP32-S3 for image writes (per docs/protocol.md, that family
uses JPEG rather than the Mini's BMP) — not implemented yet, so `keys:`
currently only draws icons for the Mini/Mini Mk2 family. Key-press detection
and entity toggling work regardless of family.

## Hardware

- **MCU board**: Waveshare ESP32-S3-Zero (single USB-C, native USB pins,
  no USB-UART chip — see [docs/hardware.md](docs/hardware.md) for why that
  matters here)
- **Target device**: an Elgato Stream Deck (VID `0x0fd9`) — Mini, Mini Mk2,
  Original, Original V2, or MK.2; see [docs/protocol.md](docs/protocol.md)
  for the full PID table

## Roadmap

1. ~~**M1 — USB host bring-up**: enumerate the Stream Deck, log key presses.~~ Done.
2. ~~**M2 — Image upload**: push key icons to the LCDs (Mini family, BMP).~~ Done. Reset/brightness feature reports not yet implemented.
3. ~~**M3/M4 — Home Assistant integration**: bind keys to entities, render
   icon/color/name, toggle on press.~~ Done, for the Mini family.
4. **Next**: a JPEG encoder for the Original/Original V2/MK.2 family, so
   `keys:` works there too.

## Sources

The USB protocol was not derived from guesswork — it's cross-checked against
two independent open-source implementations:

- [python-elgato-streamdeck](https://github.com/abcminiuser/python-elgato-streamdeck)
- [`elgato-streamdeck` Rust crate](https://github.com/OpenActionAPI/rust-elgato-streamdeck)

[OpenDeck](https://github.com/nekename/OpenDeck) is a desktop app (Rust/Svelte)
that itself depends on the Rust crate above rather than implementing the
protocol directly — not directly reusable here, but useful for cross-checking.

The `usb_host_hid` ESP-IDF component this builds on lives in
[espressif/esp-usb](https://github.com/espressif/esp-usb).
