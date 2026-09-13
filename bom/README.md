# Bill of materials

`bom.csv` is the working BOM. Columns:

- **status** — `have` / `to buy` / `to make`
- **notes** — anything that would otherwise be lost between sessions

## Eight parts

That is the headline. [Sparks](../../sparks) has 35 parts, 29 of them SMD; this
board has 8, none of them SMD. The difference is almost entirely the LED panel
and the power chain it dragged along with it — see `docs/decisions.md` §1 for the
full cascade.

There is no `conditional` status in this BOM and no `phase 2` status either.
Every part is needed for the only version of the board that exists, which is a
consequence worth noticing: Sparks carried an unpopulated radio footprint and a
maybe-needed level shifter because its design had open questions the board had to
accommodate. This one does not.

## Sourcing: Brazil

Parts are chosen to be buyable locally (Mercado Livre, national distributors)
rather than imported. This is a selection criterion, not an afterthought — a part
that is optimal on paper but import-only costs weeks and customs hassle.

This BOM makes that easy in a way Sparks' did not. Every item is either already
on hand or a generic through-hole part sold in any Brazilian electronics shop:
a tact switch, a buzzer, a ceramic cap, two headers. Nothing here has a specific
part number worth arguing about, and nothing is SMD-only.

## Things to get right

1. **The buzzer must be PASSIVE.** This is the single most likely ordering
   mistake, because listings rarely say which they are and the two look
   identical. An active buzzer has its own oscillator and plays one fixed note
   whenever powered — `tone()` does nothing to it. The device needs two
   distinguishable pitches (700 Hz for your keying, 550 Hz for received signal),
   so an active buzzer breaks a feature rather than just sounding different.

   *How to tell them apart:* put a multimeter in resistance mode across the pins.
   A passive piezo reads a few ohms to a few hundred; an active module reads much
   higher or looks like a diode. If in doubt, buy one of each — they cost almost
   nothing.

2. **CC1101 module pin order.** Varies between module revisions. Check the
   silkscreen against the table in `docs/hardware.md` **before routing the
   board**, not before soldering — a wrong table means a wrong PCB. Crossing VCC
   and GND destroys the module.

3. **Buy the dummy load before the first transmit test.** It is listed as a part
   rather than as an accessory deliberately. Keying the radio with nothing
   attached reflects power back into the PA and can damage it, and the whip
   cannot be used until the ANATEL question is settled — so the dummy load is the
   only legal way to test transmit at all. It is a cheap SMA part; order it with
   everything else rather than discovering it is missing.

4. **Tact switch height.** The `_H5mm` variant sits taller, which matters once
   there is a case. Decide when the enclosure is modelled; the footprint is the
   same either way, so this is not blocking.

## Not on this board, and why

Worth recording so these are not re-added by reflex when someone reads the Sparks
BOM alongside this one:

| Part | Why it is absent |
|---|---|
| MAX7219, 26 LEDs, `RSET` | no LED panel — `docs/decisions.md` §1 |
| Level shifter | nothing runs at 5 V |
| HW-357 boost module | nothing needs 5 V, so nothing needs boosting |
| LiPo cell, TP4056 | USB-C powers the board; `J3` keeps the option open |
| Battery divider (2× 220k) | no battery to sense |
| All 0805 passives | through-hole throughout — `docs/decisions.md` §2 |

The HW-357 is the one genuinely worth being glad about: it ships with an
uncalibrated trimmer that can output 10 V or more, and setting it to 5.0 V with a
multimeter *before connecting anything* was a mandatory, destructive-if-skipped
assembly step. That failure mode does not exist here.
