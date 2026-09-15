# Fabrication — getting the board onto the fiber laser

The laser software (BSLApp, the EZCAD family that ships with JPT/Raycus
markers) imports **DXF**. KiCad exports DXF directly, so there is no
intermediate tool.

Everything below is generated, not source — `exports/` is gitignored apart from
this description of how to recreate it.

## Generating the files

From `hardware/kicad/cc1101-morse/`:

```sh
# copper — what the laser removes
kicad-cli pcb export dxf --mode-single -l F.Cu \
  --uc --erd --ev --ou mm --drill-shape-opt 0 \
  -o exports/copper-F_Cu.dxf cc1101-morse.kicad_pcb

# board outline — cut or scribe
kicad-cli pcb export dxf --mode-single -l Edge.Cuts \
  --uc --erd --ev --ou mm --drill-shape-opt 0 \
  -o exports/outline-Edge_Cuts.dxf cc1101-morse.kicad_pcb

# drill positions, plus a DXF map of them
kicad-cli pcb export drill --format excellon --excellon-units mm \
  --generate-map --map-format dxf -o exports/ cc1101-morse.kicad_pcb
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
| `copper-F_Cu.dxf` | **engrave** — the copper to remove |
| `outline-Edge_Cuts.dxf` | **cut** the 40 × 60 mm board out, or scribe it for snapping |
| `cc1101-morse.drl` + `-drl_map.dxf` | **drill** — 40 holes |

## Polarity: the laser removes what it marks

This is the one that ruins a board if it is got backwards.

`copper-F_Cu.dxf` contains the copper that must **remain** — traces, pads, and
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

40 holes in five sizes:

| Size | Count | What |
|---|---|---|
| 0.80 mm | 4 | C1, C2 |
| 0.84 mm | 16 | CC1101 socket (`J1`) |
| 1.00 mm | 12 | test points, headers |
| 1.10 mm | 4 | tact switch (`SW1`) |
| 2.10 mm | 4 | mounting holes (`H1`–`H4`) |

The drill file is Excellon, which BSLApp does not read. Either use the
`-drl_map.dxf` as a visual guide and drill by hand, or import the map DXF and
mark centre points for a drill press.

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
