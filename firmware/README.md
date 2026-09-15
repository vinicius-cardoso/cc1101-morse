# Firmware

Arduino sketch for the ESP32-C3 Supermini. Open
`cc1101-morse/cc1101-morse.ino` in the Arduino IDE, select **ESP32C3 Dev
Module**, and upload over USB.

> [!IMPORTANT]
> **Set `USB CDC On Boot` to `Enabled`.** It is *disabled* by default, and with
> it off `Serial` goes to the UART pins instead of the USB port — the sketch
> runs, prints into the void, and the serial monitor shows nothing at all. Every
> diagnostic in this firmware is a serial command, so without it you are blind.
>
> From the command line that is:
> ```sh
> arduino-cli compile --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc \
>   --libraries ~/hardware/arduino/libraries cc1101-morse
> arduino-cli upload -p /dev/ttyACM0 \
>   --fqbn esp32:esp32:esp32c3:CDCOnBoot=cdc cc1101-morse
> ```

The CC1101 is driven directly — no radio library. The **optional** OLED needs
`Adafruit_SSD1306` and `Adafruit_GFX`; if you do not have them, set
`DISPLAY_ENABLE 0` in `display.h` and the sketch builds without them.

## It runs with nothing attached

`radio.h` defaults to `RADIO_SIMULATE 1`: SPI is never touched and keying is
logged to the serial monitor instead of driving a chip. Key timing, decoding,
adaptive speed and the sidetone all work identically, so the whole firmware can
be developed and exercised on a bare Supermini — or on a Supermini with just a
button and a buzzer.

Set it to `0` once the CC1101 is wired.

You can also drive everything from the serial monitor with no key wired at all.
Type `help` for the command list.

## Files

| File | What it is |
|---|---|
| `cc1101-morse.ino` | main loop, serial commands, TX/RX arbitration |
| `morse_event.h` | the event vocabulary shared by both sources |
| `morse_tree.h` | the Morse tree, and letter ↔ code tables |
| `key.{h,cpp}` | debounce, dot/dash classification, adaptive speed |
| `radio.{h,cpp}` | CC1101 as an OOK carrier switch, both directions |
| `decoder.{h,cpp}` | events in, letters out |
| `sidetone.{h,cpp}` | the buzzer, at two pitches |
| `display.{h,cpp}` | optional SSD1306 OLED — absent by default |

## The shape of it

The design rests on one idea, and it is worth stating before reading the code:

**A dot is a dot whether a finger made it or a carrier did.**

`key.cpp` times a button. `radio.cpp` times a pin the CC1101 drives. Both emit
the same `MorseEvent` values, so `decoder.cpp` takes either one and never asks
which — the two decoder instances in the sketch are the same class with
different labels.

That is why receive cost so little to add. It is a second *producer*, not a
second decoder, and the tree walk, the letter commit, the gap handling and the
message buffer are all written once.

```
  key.cpp   ──┐
              ├──▶  MorseEvent  ──▶  decoder.cpp  ──▶  letters
  radio.cpp ──┘
```

## Why the event type has its own header

`morse_event.h` holds nothing but an enum, which looks like over-organisation
until you notice it is what lets `key.cpp` and `radio.cpp` stay strangers to
each other. Neither includes the other; both include the vocabulary. Putting
`MorseEvent` inside `key.h` would have made the radio depend on the key for no
reason other than where the type happened to be declared.

## The OLED is optional, and detected at runtime

The PCB has no display (`docs/decisions.md` §1), but GPIO8/GPIO9 were kept free
so one could be wired anyway (§7). `display.cpp` serves both cases from one
binary.

**It probes rather than assumes.** `Display::begin()` does a single-address I2C
probe at 0x3C. If nothing answers, `present_` stays false and every subsequent
call returns immediately — no retry loop, no timeout, no blocking. The buzzer and
serial output carry on exactly as before.

That is why there are no `#ifdef`s scattered through the sketch: the no-op path
is a runtime property of one object, not a compile-time variant of the whole
firmware. Plugging a screen into a finished board is a thing you can just do.

`DISPLAY_ENABLE 0` is a separate escape hatch, for building without the Adafruit
libraries installed. It compiles the module down to empty stubs.

**Wiring:** SDA to GPIO8, SCL to GPIO9, 3V3 and GND. Address 0x3C.

## Is your buzzer passive or active?

The firmware needs a **passive** buzzer — the kind that is just a piezo element
and plays whatever `tone()` drives it with. An active buzzer has its own
oscillator and plays one fixed note regardless, which collapses the 700 Hz and
550 Hz sidetones into the same sound and loses the distinction between what you
are sending and what is arriving.

They look identical, and listings rarely say which is which. Type `buzzer` at
the serial monitor:

| What you hear | What you have |
|---|---|
| pitch climbs 400 → 1200 Hz | **passive** — correct part |
| one steady note | **active** — wrong part |
| silence | not wired, or wired backwards |

It then plays 700 Hz and 550 Hz back to back. On a passive buzzer those are
obviously different notes; if they sound the same, it is active.

## Things worth knowing before changing it

**The radio stays in TX for `TX_HANG_MS` after you stop keying.** Switching modes
costs a calibration, so doing it per symbol would put that latency inside every
dot and smear the timing. The hang time trades a moment of deafness for clean
keying.

**The buzzer has an owner.** Whichever source keys it first holds it until it
releases. Letting your own key interrupt an incoming signal would chop up what
you are copying, which is backwards.

**Received marks shorter than `RADIO_RX_MIN_MARK_MS` are discarded.** 433 MHz is
full of garage doors and car fobs, and async mode has no sync word or CRC to
reject them. The threshold is a plausibility filter and nothing more — it cannot
tell a doorbell from a dash, only a 3 ms blip from a symbol. **It is a guess
until it has been tuned against the real band.**

**The message buffer is a fixed array, not a `String`.** This runs for hours
between resets, and heap fragmentation on a microcontroller is a slow, confusing
failure. When it fills, the oldest half is dropped.

## Testing without a second radio

Two units talking is the real test (see `docs/roadmap.md` phase 2), but before
that:

- `send SOS` transmits a known string, so a receiver — an SDR dongle, or a second
  unit — has something predictable to show.
- `status` reads `PARTNUM` / `VERSION` off the chip. **This is the first thing to
  run on newly wired hardware:** a sane answer (`0x00` / `0x14`) means SPI works,
  which separates wiring problems from radio problems. Anything else is almost
  always MISO/MOSI swapped or a module whose pin order differs from the assumed
  one.
- `power 0` drops TX power for bench work.

> [!WARNING]
> Fit a 50 Ω dummy load before any transmit test. Keying with nothing attached
> reflects power back into the PA and can damage it — and the whip cannot be used
> until the ANATEL question in `docs/roadmap.md` is settled.
