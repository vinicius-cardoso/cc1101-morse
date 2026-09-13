# Firmware

Arduino sketch for the ESP32-C3 Supermini. Open
`cc1101-morse/cc1101-morse.ino` in the Arduino IDE, select **ESP32C3 Dev
Module**, and upload. No external libraries — the CC1101 is driven directly.

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

## What is carried from Sparks

`key.cpp` and `morse_tree.h`'s tree are essentially verbatim from
[Sparks](../../sparks), **including the pin assignments** — the key is on GPIO10
and the buzzer on GPIO20 on both boards. That is deliberate: a fix to the
debouncing can be copied across without reading it carefully first.

What changed:

- `KeyEvent` became `MorseEvent` in its own header, because the radio produces
  them too.
- `morse_tree.h` lost the LED addressing table (`MORSE_NODE_LED`) — there is no
  panel — and gained digits, for sending a call sign.
- `sidetone` gained a second pitch and an owner, so your keying and an incoming
  signal cannot fight over the buzzer.
- `panel.{h,cpp}` is gone entirely. `decoder.{h,cpp}` and `radio.{h,cpp}` are new.

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
