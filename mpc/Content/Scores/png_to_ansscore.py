#!/usr/bin/env python3
"""Convert a grayscale image (sonogram/spectrogram) to an ANS .ansscore file.

Usage:
    python3 png_to_ansscore.py input.png Content/Scores/user.ansscore
    python3 png_to_ansscore.py input.png out.ansscore --width 1024 --px-per-mm 10
    python3 png_to_ansscore.py input.png out.ansscore --flip

Row 0 of the output is the highest pitch (top of the image); row 719 is the
lowest. White = open mastic (sound), black = closed. The output is resized to
720 rows; the width is scaled proportionally unless --width is given.

Duration at full scan speed (24 mm/s) = width / (px_per_mm * 24) seconds.

Requires Pillow:  python3 -m pip install pillow
"""

import argparse
import struct
import sys


def main() -> int:
    ap = argparse.ArgumentParser(description="Convert an image to an ANS .ansscore file")
    ap.add_argument("input", help="input image (PNG/JPEG/TIFF...)")
    ap.add_argument("output", help="output .ansscore path")
    ap.add_argument("--width", type=int, default=0,
                    help="output width in pixels (default: keep the aspect ratio)")
    ap.add_argument("--px-per-mm", type=int, default=10,
                    help="horizontal resolution stored in the header (default: 10)")
    ap.add_argument("--flip", action="store_true",
                    help="flip vertically (use if low frequencies are at the top)")
    args = ap.parse_args()

    try:
        from PIL import Image
    except ImportError:
        print("Pillow is required: python3 -m pip install pillow", file=sys.stderr)
        return 1

    img = Image.open(args.input).convert("L")
    src_w, src_h = img.size
    if src_h == 0 or src_w == 0:
        print("empty image", file=sys.stderr)
        return 1

    if args.flip:
        img = img.transpose(Image.FLIP_TOP_BOTTOM)

    out_w = args.width if args.width > 0 else max(1, round(src_w * 720.0 / src_h))
    resample = getattr(Image, "Resampling", Image).LANCZOS
    img = img.resize((out_w, 720), resample)
    data = img.tobytes()

    header = b"ANS1" + struct.pack("<HHHHI", 1, out_w, 720, args.px_per_mm, len(data))
    with open(args.output, "wb") as f:
        f.write(header)
        f.write(data)

    duration = out_w / (max(1, args.px_per_mm) * 24.0)
    print(f"wrote {args.output}: {out_w} x 720, {args.px_per_mm} px/mm, "
          f"{duration:.2f} s at 24 mm/s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
