# KiCad

The KiCad project lives in `cc1101-morse/`, alongside `libraries/`.

`libraries/` holds the ESP32-C3 Supermini symbol and footprint, which the stock
KiCad libraries do not carry. Nothing else is needed from a custom library: the
CC1101 is a
socketed module represented by a generic `Conn_02x04_Odd_Even`, and everything
else is a stock KiCad part.

## Before drawing anything

Two things from `docs/hardware.md` that are easy to get wrong and expensive to
discover late:

1. **Check the CC1101 module's silkscreen** against the pin table in
   `docs/hardware.md` before routing. Pin order varies by module revision, and
   crossing VCC and GND destroys the module — on a board that is already etched.
   Check its **gender** too: the module on hand has male pins, so the board
   carries a female `PinSocket_2x04`. A `PinHeader` footprint has a different
   drill size and will not fit.

2. **This board is single-layer and through-hole only.** That is a design
   constraint, not a default to relax when routing gets awkward. If a net seems
   to need a jumper, the placement is wrong — see `docs/decisions.md` §3. The
   placement rules that make it work:

   - CC1101 socket at the board edge, SMA pointing outward.
   - Supermini directly beside it, so the six-trace SPI bundle is short.
   - Every ground-connected part at the perimeter; ground routes around the edge
     as a trace with short stubs inward, **not** as a pour.

## Footprints

All through-hole. The full table is in `docs/hardware.md` § KiCad reference —
`SW_PUSH_6mm`, `Buzzer_12x9.5RM7.6`, `PinSocket_2x04_P2.54mm_Vertical`,
`PinHeader_1x02_P2.54mm_Vertical`, `C_Disc_D5.0mm_W2.5mm_P5.00mm`.

## What to commit

Sources, not derivatives: `.kicad_pro`, `.kicad_sch`, `.kicad_pcb`, and any local
library files. The `.gitignore` already excludes backups, autosaves, the KiCad 10
`.history/` store, and generated gerbers — put manufacturing output in an
`exports/` directory if it needs keeping.
