# CC1101 Morse

A handheld Morse transceiver. Key it and the characters go out over 433 MHz;
leave it idle and it decodes what another unit sends back.

Press the key. The device times your dots and dashes, sounds a sidetone so you
hear what you send, keys the CC1101 in step, and prints the decoded letter over
serial. Point a second one at it and the two talk to each other.

This is the stripped-down sibling of [Sparks](../sparks) — same key, same
sidetone, same Morse tree, but **no LED panel and no MAX7219**. What is left is
the radio, and that lets the board be **single-layer, through-hole only**.

## Why it is simpler

Sparks' 26-LED panel is what forced a double-sided board: the matrix is organised
by tree *level* while the panel is organised by tree *geometry*, and those two
organisations need about 56 crossings to reconcile. Remove the panel and the
whole conflict disappears — what remains is one module, one socket, a button and
a buzzer, which routes on a single face with no jumpers.

The consequences are worth stating plainly, because they are the point of this
board:

| | Sparks | CC1101 Morse |
|---|---|---|
| Layers | 2, with vias | **1** |
| Parts to place | 35 | **8** |
| SMD parts | 29 | **0** |
| Jumper wires | 0 (vias instead) | **0** |
| Display | 26-LED tree panel | serial + buzzer |

Zero SMD means the board can be hand-soldered from the underside with a plain
iron, and the fiber laser only ever etches one face — no two-sided registration.

## Hardware

| Part | Role |
|---|---|
| ESP32-C3 Supermini | the brain — timing, decoding, radio, everything |
| CC1101 module | 433 MHz transceiver, socketed |
| Momentary key | Morse input |
| Passive buzzer | sidetone on transmit, audio on receive |

Everything runs at **3.3 V**. This is the other thing the panel was costing: the
MAX7219 is a 5 V part, so Sparks needed a boost converter and, probably, a level
shifter. The CC1101 is natively 3.3 V, matching the ESP32-C3, so the supply is
just the Supermini's onboard regulator.

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

A 2-pin header (`J3`) brings the `VIN` / `GND` nets to the board edge, so a cell
and a charge module can be bolted on later without a respin. It stays
unpopulated for now.

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

- **KiCad** for the PCB (`hardware/kicad/`)
- **Onshape** for the case, exported to `mechanical/`
- **Fiber laser** to engrave the copper
- **Arduino** for firmware (`firmware/cc1101-morse/`)

Keep editable sources as the source of truth: `.kicad_pro`, `.kicad_sch`,
`.kicad_pcb`, `.ino`, native CAD. Generated manufacturing output belongs in
`exports/` directories.

## Status

Early. Firmware is written and runs without a radio attached — `radio.h` defaults
to `RADIO_SIMULATE 1`, which prints what it would transmit instead of driving the
chip, so key timing and decoding can be exercised on a bare Supermini. The PCB is
not drawn yet.

## Licence

GPL-3.0 — see `LICENSE`.
