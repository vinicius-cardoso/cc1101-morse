#!/usr/bin/env python3
"""Turn a KiCad Excellon drill file into a DXF of filled circles.

For drilling by hand rather than lasering the holes: one solid dot at every
hole position, so the artwork can be printed or marked and centre-punched.

Usage:
    drill-marks-dxf.py input.drl output.dxf [--diameter 1.0] [--mirror 150.0]

Each hole becomes a filled disc of the given diameter — 1 mm by default, big
enough to see and small enough to centre by eye — regardless of the hole's
real size. The point is *where* to drill, not how wide; the real sizes stay in
the .drl and in docs/fabrication.md.

Pass --mirror with the board's X centre to match mirrored copper artwork. The
copper on this board is on B.Cu and gets engraved mirrored, so marks made from
an unmirrored file would land on the wrong side of the board.

Filled discs are drawn as SOLID entities in a fan around each centre, because
that is the one fill primitive every DXF reader handles the same way. HATCH
would be tidier and is widely misread.
"""
import argparse
import math
import re
import sys

# Facets per circle. 32 is smooth at any size a drill mark is printed.
SEGMENTS = 32


def read_holes(path):
    """Positions from an Excellon file, in millimetres."""
    text = open(path).read()
    if "METRIC" not in text:
        sys.exit(f"{path}: not a metric drill file; re-export with --excellon-units mm")

    holes = []
    for line in text.splitlines():
        m = re.fullmatch(r"X(-?[\d.]+)Y(-?[\d.]+)", line.strip())
        if m:
            holes.append((float(m.group(1)), float(m.group(2))))
    return holes


def solid(x, y, r, a0, a1):
    """One triangular wedge of a disc, as a DXF SOLID."""
    x1, y1 = x + r * math.cos(a0), y + r * math.sin(a0)
    x2, y2 = x + r * math.cos(a1), y + r * math.sin(a1)
    # A SOLID's four corners are 10/11/12/13; repeating the last point makes
    # it a triangle. Note 12 and 13 are deliberately the same vertex.
    return (
        f"  0\nSOLID\n  8\nDrill\n 62\n7\n"
        f" 10\n{x:.6f}\n 20\n{y:.6f}\n 30\n0.0\n"
        f" 11\n{x1:.6f}\n 21\n{y1:.6f}\n 31\n0.0\n"
        f" 12\n{x2:.6f}\n 22\n{y2:.6f}\n 32\n0.0\n"
        f" 13\n{x2:.6f}\n 23\n{y2:.6f}\n 33\n0.0\n"
    )


def write_dxf(path, holes, diameter):
    r = diameter / 2
    body = []
    for x, y in holes:
        for i in range(SEGMENTS):
            a0 = 2 * math.pi * i / SEGMENTS
            a1 = 2 * math.pi * (i + 1) / SEGMENTS
            body.append(solid(x, y, r, a0, a1))

    with open(path, "w") as f:
        # Minimal but complete DXF: header declaring millimetres, a layer
        # table, then the entities.
        f.write(
            "  0\nSECTION\n  2\nHEADER\n"
            "  9\n$ACADVER\n  1\nAC1018\n"
            "  9\n$INSUNITS\n 70\n4\n"          # 4 = millimetres
            "  9\n$MEASUREMENT\n 70\n1\n"       # 1 = metric
            "  0\nENDSEC\n"
            "  0\nSECTION\n  2\nTABLES\n"
            "  0\nTABLE\n  2\nLAYER\n 70\n1\n"
            "  0\nLAYER\n  2\nDrill\n 70\n0\n 62\n7\n  6\nCONTINUOUS\n"
            "  0\nENDTAB\n  0\nENDSEC\n"
            "  0\nSECTION\n  2\nENTITIES\n"
        )
        f.writelines(body)
        f.write("  0\nENDSEC\n  0\nEOF\n")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--diameter", type=float, default=1.0,
                    help="mark diameter in mm (default 1.0)")
    ap.add_argument("--mirror", type=float, metavar="X",
                    help="mirror about this X, to match mirrored copper artwork")
    args = ap.parse_args()

    holes = read_holes(args.src)
    if not holes:
        sys.exit(f"{args.src}: no hole positions found")

    if args.mirror is not None:
        holes = [(2 * args.mirror - x, y) for x, y in holes]

    write_dxf(args.dst, holes, args.diameter)

    xs = [h[0] for h in holes]
    ys = [h[1] for h in holes]
    print(f"{args.src} -> {args.dst}")
    print(f"  {len(holes)} holes as {args.diameter} mm filled circles")
    print(f"  extent X {min(xs):.2f}..{max(xs):.2f}  Y {min(ys):.2f}..{max(ys):.2f}")
    if args.mirror is not None:
        print(f"  mirrored about x={args.mirror}")


if __name__ == "__main__":
    main()
