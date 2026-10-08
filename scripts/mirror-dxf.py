#!/usr/bin/env python3
"""Mirror a KiCad DXF export about a vertical axis.

The copper on this board is on B.Cu, and KiCad draws every layer from the top
— so a B.Cu export is a view *through* the laminate. Engraving it as exported
puts a reversed image on the copper and nothing fits. This flips it.

Usage:
    mirror-dxf.py input.dxf output.dxf --board board.kicad_pcb
    mirror-dxf.py input.dxf output.dxf --axis 150.0

The mirror axis must be the board's X centre. Pass the `.kicad_pcb` and it is
read from the Edge.Cuts outline; pass `--axis` to give it directly.

Do not try to infer the axis from the DXF itself. KiCad's DXF carries
drawing-frame points far outside the board, in units that differ between the
GUI Plot dialog (inches by default) and `kicad-cli --ou mm`. Every heuristic
for telling frame from board gets one of those cases wrong, and the failure is
silent: the artwork still looks like a board, just mirrored about the wrong
line, which no visual check reliably catches.

Everything is mirrored, frame included. That is correct — the frame is not
part of the engraving, and leaving it in place while flipping the board would
only make the two disagree.
"""
import argparse
import re
import sys

# DXF group codes carrying an X coordinate.
X_CODES = ("10", "11")


def axis_from_board(path):
    """X centre of the Edge.Cuts outline, in millimetres."""
    text = open(path).read()
    rect = re.search(
        r"\(gr_rect\s*\(start ([-\d.]+) ([-\d.]+)\)\s*\(end ([-\d.]+) ([-\d.]+)\)", text
    )
    if rect:
        x1, _, x2, _ = (float(g) for g in rect.groups())
        return (x1 + x2) / 2

    # No rectangle: fall back to the extent of every Edge.Cuts line.
    xs = []
    for m in re.finditer(
        r"\(gr_line\s*\(start ([-\d.]+) [-\d.]+\)\s*\(end ([-\d.]+) [-\d.]+\)(.*?)\)",
        text,
        re.S,
    ):
        if "Edge.Cuts" in m.group(3):
            xs += [float(m.group(1)), float(m.group(2))]
    if not xs:
        sys.exit(f"{path}: found no Edge.Cuts outline to take a centre from")
    return (min(xs) + max(xs)) / 2


def mirror(src, dst, axis):
    """Flip X on entity geometry only.

    Group codes 10 and 11 mean "X coordinate" inside an entity, but the same
    numbers appear in the HEADER and TABLES sections as ordinary values.
    Rewriting those corrupts the file: the first version of this script did,
    and the result loaded in KiCad but hung every other DXF viewer.

    So only the ENTITIES section is touched.
    """
    lines = open(src).read().split("\n")
    out = []
    flipped = 0
    in_entities = False

    i = 0
    while i < len(lines):
        # Track which section we are in. A section starts with
        #   0 / SECTION / 2 / <name>  and ends with  0 / ENDSEC.
        if lines[i].strip() == "2" and i + 1 < len(lines):
            if lines[i + 1].strip() == "ENTITIES":
                in_entities = True
        if lines[i].strip() == "ENDSEC":
            in_entities = False

        out.append(lines[i])

        if in_entities and lines[i].strip() in X_CODES and i + 1 < len(lines):
            try:
                v = float(lines[i + 1])
            except ValueError:
                i += 1
                continue
            out.append(f"{2 * axis - v:.6f}")
            flipped += 1
            i += 2
            continue
        i += 1

    open(dst, "w").write("\n".join(out))
    return flipped


def x_extent(path):
    """X extent of entity geometry, ignoring header and table values."""
    lines = open(path).read().split("\n")
    xs = []
    in_entities = False
    for i, line in enumerate(lines[:-1]):
        if line.strip() == "2" and lines[i + 1].strip() == "ENTITIES":
            in_entities = True
        if line.strip() == "ENDSEC":
            in_entities = False
        if in_entities and line.strip() in X_CODES:
            try:
                xs.append(float(lines[i + 1]))
            except ValueError:
                pass
    return (min(xs), max(xs)) if xs else None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--board", help=".kicad_pcb to read the board centre from")
    g.add_argument("--axis", type=float, help="mirror axis, in the DXF's own units")
    args = ap.parse_args()

    axis = args.axis if args.axis is not None else axis_from_board(args.board)

    before = x_extent(args.src)
    flipped = mirror(args.src, args.dst, axis)
    after = x_extent(args.dst)

    if before is None or after is None:
        sys.exit("no X coordinates found")

    # A mirror preserves width. If it does not, the axis was in the wrong
    # units or the file was not what we thought.
    w_in, w_out = before[1] - before[0], after[1] - after[0]
    if abs(w_in - w_out) > 1e-6:
        sys.exit(f"width changed: {w_in:.6f} -> {w_out:.6f}; axis {axis} is probably wrong")

    print(f"{args.src} -> {args.dst}")
    print(f"  mirrored {flipped} X coordinates about x={axis}")
    print(f"  extent {before[0]:.3f}..{before[1]:.3f} -> {after[0]:.3f}..{after[1]:.3f}")


if __name__ == "__main__":
    main()
