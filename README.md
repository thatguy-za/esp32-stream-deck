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

stream_deck:
```

See [test-stream-deck.yaml](test-stream-deck.yaml) for a minimal compile-only
example, and [docs/hardware.md](docs/hardware.md) before wiring anything up.

## Status

Early bring-up. See [docs/protocol.md](docs/protocol.md) for the reverse-engineered
Stream Deck USB protocol (Mini, Original, and Original V2/MK.2 families) and
[docs/hardware.md](docs/hardware.md) for the board wiring/power plan. The
current milestone (M1, [esphome/components/stream_deck](esphome/components/stream_deck))
enumerates the device, auto-detects which family it belongs to, and logs key
presses — no image-writing yet. Verified to build cleanly via `esphome
compile` against the esp32s3 target; not yet validated against real
hardware.

## Hardware

- **MCU board**: Waveshare ESP32-S3-Zero (single USB-C, native USB pins,
  no USB-UART chip — see [docs/hardware.md](docs/hardware.md) for why that
  matters here)
- **Target device**: an Elgato Stream Deck (VID `0x0fd9`) — Mini, Mini Mk2,
  Original, Original V2, or MK.2; see [docs/protocol.md](docs/protocol.md)
  for the full PID table

## Roadmap

1. **M1 — USB host bring-up**: enumerate the Stream Deck, log key presses.
   *(current)*
2. **M2 — Image upload**: push key icons to the LCDs, brightness/reset.
3. **M3 — Home Assistant integration**: expose key presses as ESPHome
   triggers/binary_sensors and a service for HA to push key images.
4. **M4 — Home Assistant polish**: reflect entity state on key icons.

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
