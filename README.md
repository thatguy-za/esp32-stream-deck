# esp32-stream-deck

An ESP32-S3 firmware project that acts as a **USB host** for a genuine Elgato
Stream Deck, so it can be used standalone — driving Home Assistant directly
over WiFi, with no PC in the loop. Targets both the **Mini** and the
**Original/MK.2** family (see [docs/protocol.md](docs/protocol.md)); which
one a given build talks to will be a user-selectable option once the
ESPHome component (M3) exists.

This is the reverse of how ESP32s normally touch USB: instead of showing up as
a device (keyboard, CDC-ACM, etc.), the ESP32-S3 enumerates the Stream Deck
*as its host* and speaks the same protocol Elgato's own software would.

## Status

Early bring-up. See [docs/protocol.md](docs/protocol.md) for the reverse-engineered
Stream Deck USB protocol (Mini, Original, and Original V2/MK.2 families) and
[docs/hardware.md](docs/hardware.md) for the board wiring/power plan. The
current milestone (M1) is enumerating the device, auto-detecting which family
it belongs to, and logging key presses from bare ESP-IDF — before any
image-writing or ESPHome integration is attempted.

## Hardware

- **MCU board**: Waveshare ESP32-S3-Zero (single USB-C, native USB pins,
  no USB-UART chip — see [docs/hardware.md](docs/hardware.md) for why that
  matters here)
- **Target device**: an Elgato Stream Deck (VID `0x0fd9`) — Mini, Mini Mk2,
  Original, Original V2, or MK.2; see [docs/protocol.md](docs/protocol.md)
  for the full PID table

## Roadmap

1. **M1 — USB host bring-up** (bare ESP-IDF): enumerate the Stream Deck,
   dump its descriptors, log key presses.
2. **M2 — Image upload**: push key icons to the LCDs, brightness/reset.
3. **M3 — ESPHome external component**: wrap the driver so keys can trigger
   Home Assistant automations and HA can push key images.
4. **M4 — Home Assistant polish**: reflect entity state on key icons.

## Sources

The USB protocol was not derived from guesswork — it's cross-checked against
two independent open-source implementations:

- [python-elgato-streamdeck](https://github.com/abcminiuser/python-elgato-streamdeck)
- [`elgato-streamdeck` Rust crate](https://github.com/OpenActionAPI/rust-elgato-streamdeck)

[OpenDeck](https://github.com/nekename/OpenDeck) is a desktop app (Rust/Svelte)
that itself depends on the Rust crate above rather than implementing the
protocol directly — not directly reusable here, but useful for cross-checking.
