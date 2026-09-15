# CC1101 Morse

A handheld Morse transceiver. Key it and the characters go out over 433 MHz;
leave it idle and it decodes what another unit sends back.

Press the key. The device times your dots and dashes, sounds a sidetone so you
hear what you send, keys the CC1101 in step, and prints the decoded letter over
serial. Point a second one at it and the two talk to each other.

The design goal is a board simple enough to make at home: **single layer,
through-hole only, no jumper wires**. There is no display — output is the buzzer
and the serial monitor — and that constraint is what keeps everything else
small.

<p align="center">
  <img src="media/3d.png" alt="Rendered view of the assembled board" width="340">
</p>

## Why it is simple

What the board carries is one module on a socket, a button, a buzzer, one
capacitor and two test points — plus the MCU. None of it surface-mount, twelve
nets. That routes on a single face with no crossings to resolve.

| | |
|---|---|
| Layers | **1** |
| Vias | **0** |
| SMD parts | **0** |
| Jumper wires | **0** |
| Parts to solder | **8** |
| Display | serial + buzzer |

Zero SMD means the board can be hand-soldered from the underside with a plain
iron, and the fiber laser only ever etches one face — no two-sided
registration.

## Hardware

| Part | Role |
|---|---|
| ESP32-C3 Supermini | the brain — timing, decoding, radio, everything |
| CC1101 module | 433 MHz transceiver, socketed |
| Momentary key | Morse input |
| Passive buzzer | sidetone on transmit, audio on receive |

Everything runs at **3.3 V**. The CC1101 is natively 3.3 V and so is the
ESP32-C3, so the supply is just the Supermini's onboard regulator — no boost
converter, no level shifter, no charge circuit.

### Pin budget — 8 of 13 GPIOs

| Function | Pins |
|---|---|
| CC1101 (SCK, MISO, MOSI, CS, GDO0, GDO2) | 6 |
| Morse key | 1 |
| Buzzer | 1 |
| **Total** | **8 — 5 spare** |

See `docs/hardware.md` for the wiring detail.

## Transmit and receive

The CC1101 sends Morse as **OOK** — on-off keying, the carrier simply switched on
for a mark and off for a space. That is what Morse *is* on a radio, and it means
the transmitted signal is the key line itself: no packet format, no framing, no
preamble.

Receiving is the same idea backwards. The CC1101 is put in async serial mode,
where GDO0 goes high whenever the carrier is present, and the firmware times that
pin exactly the way it times the key. One decoder serves both directions.

> [!WARNING]
> **Never key the transmitter with the antenna unscrewed.** With nothing to
> radiate into, power reflects back into the PA and can damage it. For bench
> testing without emitting, use a 50 Ω dummy load in place of the whip.

> [!IMPORTANT]
> The legal position for 433 MHz short-range transmission in Brazil (ANATEL) has
> not been researched. Do that before transmitting outside a dummy load — see
> `docs/roadmap.md` phase 4.

## Power

USB-C, from the ESP32-C3 Supermini's own port. No battery, no charger, no boost,
no battery-sense divider — the board has **zero power components**.

Going portable later means wiring a cell and a TP4056 to the Supermini's
`5V`/`GND` pins. Nothing else changes, because nothing on the board wants more
than 3.3 V — so no boost converter is involved.

## The board

<p align="center">
  <img src="media/schematic.png" alt="Schematic: ESP32-C3 Supermini, CC1101 socket, key and buzzer" width="620">
</p>

The CC1101 plugs into `J1`; `SW1` is the key and `BZ1` the buzzer. `C1`
decouples the `+5V` input to the Supermini's regulator, `C2` the `+3V3` rail at
the radio itself, and `TP1`/`TP2` are probe points for `+5V` and ground.

<p align="center">
  <img src="media/pcb.png" alt="PCB layout: single layer, through-hole, ground pour" width="400">
</p>

Everything is on the front copper — **no vias, no back-side traces**. Ground is
the pour filling the board rather than a routed net, which is what makes single
layer work: the net that touches every part never has to be threaded past the
others. See `docs/decisions.md` §10.

Four M2 mounting holes, one per corner. Board is 40 × 60 mm.

## Repository layout

```text
.
├── bom/          # bill of materials
├── docs/         # design decisions, hardware notes
├── firmware/     # Arduino sketch (firmware/cc1101-morse/)
├── hardware/     # KiCad project, datasheets
├── mechanical/   # Onshape exports, case STEP/STL
└── media/        # reference images, photos
```

## Toolchain

- **KiCad** for the PCB (`hardware/kicad/`), exported to DXF for the laser —
  see `docs/fabrication.md`
- **Onshape** for the case, exported to `mechanical/`
- **Fiber laser** to engrave the copper
- **Arduino** for firmware (`firmware/cc1101-morse/`)

Keep editable sources as the source of truth: `.kicad_pro`, `.kicad_sch`,
`.kicad_pcb`, `.ino`, native CAD. Generated manufacturing output belongs in
`exports/` directories.

## Status

**Schematic done; PCB nearly.** Routed on one layer with no vias and ground
poured, but DRC is not clean yet — the zone needs refilling after the last
routing pass, and two nets (VCC and MOSI) lost their connection during rework.
See `docs/roadmap.md` phase 3. Nothing has been etched or assembled, so none of
this has met real hardware.

Firmware is written and runs without a radio attached: `radio.h` defaults to
`RADIO_SIMULATE 1`, which prints what it would transmit instead of driving the
chip, so key timing and decoding can be exercised on a bare Supermini.

Next is phase 1 in `docs/roadmap.md` — wire the CC1101 on a breadboard and
confirm SPI talks to it.

## Licence

GPL-3.0 — see `LICENSE`.
