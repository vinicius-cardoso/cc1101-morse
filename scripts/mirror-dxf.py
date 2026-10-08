#!/usr/bin/env python3
"""Mirror a KiCad DXF export about its own X centre.

The copper on this board is on B.Cu, and KiCad draws every layer from the top
— so a B.Cu export is a view *through* the laminate. Engraving it as exported
puts a reversed image on the copper. This flips it.

Usage:  mirror-dxf.py input.dxf output.dxf

Only board geometry is mirrored. KiCad's DXF carries a few drawing-frame
points far outside the board (at 0 and at the sheet edge); flipping those
drags the extents out and corrupts the result, so they are left alone. The
script verifies the board width is unchanged and exits non-zero if it is not.
"""
import re
import sys

# Board coordinates live in this range; anything outside is frame furniture.
# Works for both inch and millimetre exports, since the board is ~1.5 in /
# ~39 mm wide and the frame sits at 0 and several hundred units out.
BOARD_MIN, BOARD_MAX = 1.0, 100.0

# DXF group codes carrying an X coordinate.
X_CODES = ("10", "11")


def board_x_values(lines):
    out = []
    for i, line in enumerate(lines[:-1]):
        if line.strip() in X_CODES:
            try:
                v = float(lines[i + 1])
            except ValueError:
                continue
            if BOARD_MIN < v < BOARD_MAX:
                out.append(v)
    return out


def main(src, dst):
    lines = open(src).read().split("\n")

    xs = board_x_values(lines)
    if not xs:
        sys.exit(f"{src}: found no board geometry to mirror")
    lo, hi = min(xs), max(xs)
    axis = lo + hi          # x' = (lo + hi) - x

    out = []
    i = 0
    flipped = kept = 0
    while i < len(lines):
        out.append(lines[i])
        if lines[i].strip() in X_CODES and i + 1 < len(lines):
            try:
                v = float(lines[i + 1])
            except ValueError:
                i += 1
                continue
            if BOARD_MIN < v < BOARD_MAX:
                out.append(repr(axis - v))
                flipped += 1
            else:
                out.append(lines[i + 1])
                kept += 1
            i += 2
            continue
        i += 1

    open(dst, "w").write("\n".join(out))

    check = board_x_values(open(dst).read().split("\n"))
    width_in, width_out = hi - lo, max(check) - min(check)
    if abs(width_in - width_out) > 1e-6:
        sys.exit(f"width changed: {width_in:.6f} -> {width_out:.6f}; not written correctly")

    print(f"{src} -> {dst}")
    print(f"  mirrored {flipped} coordinates about x={axis / 2:.4f}, left {kept} frame points")
    print(f"  width {width_out:.4f} preserved")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
