# Fabrication — getting the board onto the fiber laser

The laser software (BSLApp, the EZCAD family that ships with JPT/Raycus
markers) imports **DXF**. KiCad exports DXF directly, so there is no
intermediate tool.

The files are committed in `hardware/kicad/cc1101-morse/exports/`. They are
generated, not source — but the point of this repo is that the board can be
made, and that means the laser's input should be here without needing KiCad
installed first. **Regenerate them after any board change**, with the commands
below.

## Generating the files

From `hardware/kicad/cc1101-morse/`:

```sh
# copper — what the laser removes. B.Cu: the copper is on the BACK.
kicad-cli pcb export dxf --mode-single -l B.Cu \
  --uc --erd --ev --ou mm --drill-shape-opt 0 \
  -o exports/copper-B_Cu.dxf cc1101-morse.kicad_pcb

# board outline — cut or scribe
kicad-cli pcb export dxf --mode-single -l Edge.Cuts \
  --uc --erd --ev --ou mm --drill-shape-opt 0 \
  -o exports/outline-Edge_Cuts.dxf cc1101-morse.kicad_pcb

# drill positions
kicad-cli pcb export drill --format excellon --excellon-units mm \
  -o exports/ cc1101-morse.kicad_pcb

# optional: a DXF picture of the hole positions, if drilling by eye.
# Not committed — it is 300 KB and says nothing the .drl does not.
#   ... --generate-map --map-format dxf ...
```

The flags that matter:

| Flag | Why |
|---|---|
| `--uc` (use contours) | plots shapes as **closed outlines** rather than centre lines. Without it the laser gets a stroke down the middle of each trace instead of its true edges, and every trace comes out the wrong width. |
| `--ou mm` | KiCad defaults DXF to **inches**. Import at the wrong unit and the board is 25.4× out. |
| `--erd --ev` | drops reference designators and values. They are silkscreen text, not copper — engraving them would eat into the traces. |
| `--drill-shape-opt 0` | no drill marks in the copper layer; holes are drilled mechanically, from the separate drill file. |
| `--mode-single` | one file at the path given, rather than a directory of per-layer plots. |

## What the three files are for

| File | Operation |
|---|---|
| `copper-B_Cu.dxf` | **engrave** — the copper to remove, on the back face |
| `outline-Edge_Cuts.dxf` | **cut** the 40 × 60 mm board out, or scribe it for snapping |
| `cc1101-morse.drl` | **drill** — 46 holes |

## The copper is on the back — mind the mirror

The stock is single-sided: copper on one face, bare substrate on the other.
Components sit on the **bare** face, their leads pass through, and they are
soldered on the **copper** face. So all copper lives on `B.Cu`, and `B.Cu` is
what gets engraved.

**KiCad draws every layer as seen from the top.** A `B.Cu` export is therefore
a view *through* the board. Put the copper face up on the laser bed and you are
looking at it from the other side — so the artwork has to be **mirrored**, or
every footprint lands reversed and nothing fits.

**Use the `-MIRRORED` files.** The plain exports are kept beside them for
checking against the KiCad view; they are *not* what goes on the laser.

| File | |
|---|---|
| `cc1101-morse-B_Cu-MIRRORED.dxf` | **engrave this** |
| `cc1101-morse-B_Cu.dxf` | reference, matches the KiCad top view |
| `cc1101-morse-Edge_Cuts.dxf` | **cut this** — unmirrored; the outline is symmetric, so it needs no flip |
| `cc1101-morse.drl` | drill, 46 holes |
| `cc1101-morse-DRILL-MARKS.dxf` | **drill by hand from this** — 1 mm dots at every hole |

Produce the mirrored pair with:

```sh
python3 scripts/mirror-dxf.py \
  exports/cc1101-morse-B_Cu.dxf exports/cc1101-morse-B_Cu-MIRRORED.dxf \
  --board cc1101-morse.kicad_pcb
```

**The axis comes from the board, not from the DXF.** The script reads the
Edge.Cuts outline out of the `.kicad_pcb` and mirrors about its X centre
(150.0 mm here). That matters: KiCad's DXF carries drawing-frame points far
outside the board, in units that differ between the GUI Plot dialog (inches by
default) and `kicad-cli --ou mm`. Every attempt to guess the board's extent
from the DXF alone got one of those cases wrong, and the failure is silent —
the artwork still looks like a board, just mirrored about the wrong line.

The script checks that the overall width is unchanged and refuses to write if
it is not, since a mirror that changes width has used the wrong axis.

KiCad's GUI can do it directly instead: File → Plot, tick **Mirrored plot** and
**Plot graphic items using their contours**, untick reference designators and
values. Watch the units there.

**The cheap check that catches a bad mirror:** the board is not symmetric. `J1`
sits at the top edge, `SW1` near the bottom, and the `vinilabs.cc` silkscreen
reads left to right. Put the artwork on screen and look at any asymmetric
feature — if the text reads backwards in the *mirrored* file, that is correct,
because you will be looking at the copper from the other side.

## Polarity: the laser removes what it marks

This is the one that ruins a board if it is got backwards.

`copper-B_Cu.dxf` contains the copper that must **remain** — traces, pads, and
the ground pour. A laser marks what you give it, so feeding this file directly
would burn away exactly the copper you wanted to keep.

In BSLApp, either:

- **invert the fill** so the marked region is the *background* rather than the
  shapes, or
- import the outline as an enclosing boundary and hatch the area *between* it
  and the copper shapes.

The ground pour makes this easier to sanity-check than it would otherwise be:
most of the board is copper, so the area the laser clears is a thin web around
the traces. **If the preview shows it clearing most of the board, the polarity
is inverted.**

## Hatching

Copper removal is an area operation, not an outline one — the fill settings
decide whether the copper actually comes off:

- **Hatch (fill) enabled**, line spacing around 0.02–0.05 mm.
- **Cross-hatch** (0° then 90°) gives a cleaner clear than one direction.
- Multiple passes at lower power beat one pass at high power: less heat into the
  substrate, less lifting at the trace edges.

Run a test coupon before committing the real board. The numbers above are a
starting point, not a recipe — they depend on the laser's wattage, the copper
thickness, and the laminate.

## Drill sizes

46 holes in six sizes:

| Size | Count | What |
|---|---|---|
| 0.80 mm | 4 | `C1`, `C2` |
| 0.84 mm | 16 | CC1101 socket (`J1`) |
| 0.95 mm | 4 | JST XH display header (`J2`) |
| 1.00 mm | 14 | test points, jumper pads |
| 1.10 mm | 4 | tact switch (`SW1`) |
| 2.10 mm | 4 | mounting holes (`H1`–`H4`) |

The drill file is Excellon, which BSLApp does not read — the holes are drilled
mechanically, not lasered.

**`cc1101-morse-DRILL-MARKS.dxf` is the one to use for hand drilling.** It is
one 1 mm filled dot per hole and nothing else: no traces, no pour, nothing to
read around. Print it or load it into the laser at low power to mark the
copper, then centre-punch and drill.

```sh
python3 scripts/drill-marks-dxf.py \
  exports/cc1101-morse.drl exports/cc1101-morse-DRILL-MARKS.dxf \
  --mirror 150.0
```

**`--mirror 150.0` is not optional here.** The copper is engraved mirrored
(see above), so marks generated from the raw drill file would land on the
wrong side of the board. 150.0 mm is the board's X centre, the same axis the
copper is mirrored about.

Every dot is the same size whatever the hole is. The file says *where* to
drill; the sizes are in the table below and in the `.drl`.

> [!NOTE]
> The mounting holes are **2.1 mm — M2 clearance**. This is deliberate, not a
> leftover: M2 screws keep the corner hardware small on a 40 × 60 mm board.

## Before exporting anything

Re-read this list each time; an export is only as good as the board behind it.

1. **Refill the zone** (`B`) and **re-run DRC until it is clean.** A zone that
   has not been refilled since the last routing pass reports a clearance
   violation against every net it touches, and the DXF will carry that same
   overlap into the copper.
2. **Check no trace crosses a mounting hole.** The autorouter could not read the
   holes' keepout areas — it logged `could not read keepout area of package
   MountingHole` — so it routed as if they were not there.
3. **Confirm 1:1 scale on import.** Measure the board in BSLApp: it must be
   40 × 60 mm. This is the check that catches a unit mismatch before it costs a
   piece of laminate.
