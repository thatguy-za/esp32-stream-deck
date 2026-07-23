# Hardware plan — ESP32-S3-Zero + Stream Deck Mini

## Why this needs a plan at all

The [Waveshare ESP32-S3-Zero](https://mischianti.org/waveshare-esp32-s3-zero-high-resolution-datasheet-pinout-and-specs/)
has exactly **one** USB-C port, wired straight to the S3's native USB pins
(GPIO19/20) — there's no separate USB-UART bridge chip on this board.

Internally, the ESP32-S3 has two controllers that can both use those same
GPIO19/20 pins, but only one at a time:

- **USB-Serial-JTAG** — used for flashing and the normal `idf.py monitor`
  console. This is what the port does out of the box.
- **USB-OTG (host/device)** — needed for this project, since the ESP32-S3
  must act as a USB *host* to the Stream Deck.

Once firmware switches that PHY over to host mode, the single USB-C port
stops being a serial console. That has four consequences:

1. **Reflashing** requires putting the board back in bootloader mode (hold
   BOOT, tap RESET, release BOOT) with the Stream Deck unplugged, so
   USB-Serial-JTAG can reclaim the pins.
2. **Runtime logging** can't use the USB-C port while host mode is active —
   see the UART0 plan below.
3. **Powering the board** can't come through the USB-C port at the same time
   it's acting as host (a host has to *supply* 5V VBUS to the downstream
   device, not receive it) — see the power plan below.
4. **Right after flashing, the board is still tethered to your computer on
   that same port** — and the computer is still actively acting as *its own*
   USB host on that link. If firmware switched to host mode immediately at
   boot, that's an instant conflict (two hosts contending for the same
   bus/VBUS), and it reliably brownout-reboots the board in a loop. The
   `stream_deck` component defers starting USB Host mode for 8 seconds after
   boot for exactly this reason — **unplug the board from your computer and
   move it to the 5V-pad power setup below within that window**, every time
   you reflash it, or it'll boot-loop.

## Debug console: UART0 on GPIO43/44

The S3-Zero breaks out UART0 separately from the USB pins:

- **GPIO43 = TX**
- **GPIO44 = RX**

Wire these to an external USB-to-serial adapter (any 3.3V FTDI/CP2102-style
board — TX→RX, RX→TX, GND→GND) to get a live console throughout, independent
of whatever the USB-C port is doing. Set `logger: hardware_uart: UART0` in
the ESPHome YAML to route log output here instead of USB-Serial-JTAG (see
[test-stream-deck.yaml](../test-stream-deck.yaml)).

## Power: the 5V pad, not the USB-C port

At runtime, USB-C is dedicated to talking to the Stream Deck as host, so the
board needs power from elsewhere:

- The S3-Zero has a **5V solder pad** for external power input (3.7–6V per
  Waveshare's docs).
- Feed that pad from a bench supply or a USB power brick with a cable
  soldered to +5V/GND (not going through the board's own USB-C port).

**Open item — verify at the bench, don't assume:** whether that 5V pad is on
the same net as the USB-C connector's VBUS pin. If it is (likely, since small
boards like this usually don't have host-mode circuitry — VBUS is often just
a straight trace to the 5V rail), powering the board from the pad
automatically presents 5V VBUS to the Stream Deck too, which is exactly what
a host needs to provide. If it's isolated (e.g. there's a diode or load
switch in between), the Stream Deck won't power up even though the board
does, and VBUS will need to be injected some other way (e.g. a short USB
extension cable with the red wire tapped into the 5V pad directly).

**Action for Milestone 1 bring-up**: before wiring anything permanently, use
a multimeter in continuity/diode mode between the 5V pad and the USB-C
connector's VBUS pin to confirm which case applies.

## Connecting the Stream Deck

The Stream Deck Mini ships as a USB device (it expects to plug into a PC), so
it needs a **USB-C-to-USB-A host/OTG adapter cable** to connect to the
S3-Zero's port while the ESP32-S3 acts as host.

## Bring-up checklist (Milestone 1)

- [ ] Multimeter check: 5V pad ↔ USB-C VBUS pin continuity
- [ ] Solder external supply leads to the 5V pad
- [ ] Wire UART0 (GPIO43 TX, GPIO44 RX, GND) to a USB-serial adapter, confirm
      console output before doing anything USB-host related
- [ ] Flash the ESPHome build (`esphome run <your-config>.yaml`) over the
      native USB-C port (device mode, normal flashing) — do this **before**
      the Stream Deck is connected and host mode takes over the port
- [ ] Within 8 seconds of it resetting after flashing, **unplug the board
      from your computer** — leaving it tethered while USB Host mode starts
      will boot-loop it (see above)
- [ ] Power the board from the 5V pad, connect the Stream Deck via the OTG
      adapter cable, watch the UART0 console for enumeration + descriptor
      dump + key-press logs
