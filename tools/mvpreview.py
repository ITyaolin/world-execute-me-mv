#!/usr/bin/env python3
"""Rasterise the MV's exported frames into PNGs (and optionally a GIF).

The C program can dump what it would draw at any moment as a tiny text format
(`./start --export DIR --export-from ... --export-to ... --export-step ...`), which
is exactly what runs on screen: one character plus one foreground colour per
terminal cell.  This tool replays that through the *same* baked 8x16 bitmap font
the program uses for its big typography (src/font_data.h), so the preview looks
like a real terminal, without needing a tty, a terminal emulator or a display.

It needs nothing but the Python standard library; GIF/sheet assembly is handed to
ImageMagick when available.

Examples
  tools/mvpreview.py --bin ./start --from 0 --to 30000 --step 100 --gif preview/opening.gif
  tools/mvpreview.py --frames preview/frames --out preview/png --scale 2
"""
import argparse
import glob
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
FONT_HEADER = os.path.join(ROOT, "src", "font_data.h")

GLYPH_RE = re.compile(r"\{\s*((?:0x[0-9a-fA-F]{2}\s*,\s*)+0x[0-9a-fA-F]{2})\s*\}")


def load_font(path):
    """-> list of 16 row-bytes for ASCII 32..126."""
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as exc:
        sys.exit("error: cannot read font header %s: %s" % (path, exc))
    glyphs = []
    for body in GLYPH_RE.findall(text):
        vals = [int(v.strip(), 16) for v in body.split(",") if v.strip()]
        if len(vals) == 16:
            glyphs.append(vals)
    if len(glyphs) < 95:
        sys.exit("error: %s does not look like the 8x16 font (%d glyphs)" % (path, len(glyphs)))
    return glyphs[:95]


def write_png(path, width, height, rows):
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data +
                struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    blob = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    with open(path, "wb") as fh:
        fh.write(blob)


def parse_mvf(path):
    with open(path, encoding="utf-8", errors="replace") as fh:
        lines = fh.read().split("\n")
    if not lines or not lines[0].startswith("MVF"):
        sys.exit("error: %s is not an MVF frame" % path)
    w = h = 0
    t = 0
    i = 1
    chars = []
    fg = []
    bg = []
    while i < len(lines):
        line = lines[i]
        if line.startswith("size "):
            _, ws, hs = line.split()
            w, h = int(ws), int(hs)
            i += 1
        elif line.startswith("time "):
            t = int(line.split()[1])
            i += 1
        elif line == "c":
            chars = lines[i + 1:i + 1 + h]
            i += 1 + h
        elif line == "g":
            fg = [ln.split() for ln in lines[i + 1:i + 1 + h]]
            i += 1 + h
        elif line == "b":
            bg = [ln.split() for ln in lines[i + 1:i + 1 + h]]
            i += 1 + h
        else:
            i += 1
    return w, h, t, chars, fg, bg


def rasterizer(font, scale, bg_rgb):
    cache = {}

    def pix(byte, color):
        key = (byte, color)
        v = cache.get(key)
        if v is None:
            on = bytes(color)
            off = bytes(bg_rgb)
            v = b"".join((on if (byte >> (7 - i)) & 1 else off) * scale for i in range(8))
            if len(cache) > 300000:
                cache.clear()
            cache[key] = v
        return v

    def render(w, h, chars, fg):
        rows = []
        for cy in range(h):
            crow = chars[cy] if cy < len(chars) else ""
            frow = fg[cy] if cy < len(fg) else []
            for sy in range(16):
                line = bytearray()
                for cx in range(w):
                    ch = crow[cx] if cx < len(crow) else " "
                    code = ord(ch)
                    if code < 32 or code > 126:
                        code = 32
                    color = (0, 0, 0)
                    if cx < len(frow):
                        tok = frow[cx]
                        if len(tok) == 6:
                            color = (int(tok[0:2], 16), int(tok[2:4], 16), int(tok[4:6], 16))
                    row_byte = font[code - 32][sy]
                    line += pix(row_byte, color)
                rows.append(bytes(line))
                for _ in range(scale - 1):
                    rows.append(bytes(line))
        return rows

    return render


def run(cmd):
    return subprocess.call(cmd) == 0


def have(cmd):
    return shutil.which(cmd) is not None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", default=os.path.join(ROOT, "start"),
                    help="the MV binary, used to export frames when needed")
    ap.add_argument("--frames", default=os.path.join(ROOT, "preview", "frames"),
                    help="directory of .mvf frames")
    ap.add_argument("--out", default=os.path.join(ROOT, "preview", "png"),
                    help="directory for the PNGs")
    ap.add_argument("--from", dest="t_from", type=int, default=0, help="first frame time (ms)")
    ap.add_argument("--to", dest="t_to", type=int, default=20000, help="last frame time (ms)")
    ap.add_argument("--step", type=int, default=100, help="frame spacing (ms)")
    ap.add_argument("--size", default="80x24", help="canvas asked from the binary")
    ap.add_argument("--scale", type=int, default=2, help="pixel zoom (1 = 8x16 px cell)")
    ap.add_argument("--bg", default="05060c", help="background colour (default terminal dark)")
    ap.add_argument("--gif", default=None, help="also assemble a GIF here")
    ap.add_argument("--sheet", default=None, help="also build a contact sheet here")
    ap.add_argument("--tile", default="3x3", help="contact sheet tiles")
    ap.add_argument("--font", default=FONT_HEADER)
    ap.add_argument("--at", default=None,
                    help="comma separated times in ms; exports exactly those frames "
                         "(a storyboard) instead of a range")
    ap.add_argument("--no-export", action="store_true", help="use existing .mvf frames only")
    ap.add_argument("--keep-frames", action="store_true")
    args = ap.parse_args()

    font = load_font(args.font)
    bg = tuple(int(args.bg[i:i + 2], 16) for i in (0, 2, 4))
    os.makedirs(args.frames, exist_ok=True)
    os.makedirs(args.out, exist_ok=True)

    if args.at and not args.no_export:
        if not os.path.exists(args.bin):
            sys.exit("error: %s not built yet (run make first)" % args.bin)
        for t in (int(v) for v in args.at.split(",") if v.strip()):
            ok = run([args.bin, "--export", args.frames, "--export-from", str(t),
                      "--export-to", str(t), "--export-step", "1000", "--size", args.size,
                      "-q"])
            if not ok:
                sys.exit("error: frame export failed at %d ms" % t)
        args.no_export = True

    existing = sorted(glob.glob(os.path.join(args.frames, "f*.mvf")))
    need_export = not args.no_export and (
        not existing or args.t_from != 0 or args.t_to != 20000)
    if need_export:
        if not os.path.exists(args.bin):
            sys.exit("error: %s not built yet (run make first)" % args.bin)
        for f in existing:
            os.unlink(f)
        print("exporting frames %d..%d step %d ..." % (args.t_from, args.t_to, args.step))
        ok = run([args.bin, "--export", args.frames, "--export-from", str(args.t_from),
                  "--export-to", str(args.t_to), "--export-step", str(args.step),
                  "--size", args.size, "-q"])
        if not ok:
            sys.exit("error: frame export failed")

    frames = sorted(glob.glob(os.path.join(args.frames, "f*.mvf")))
    if not frames:
        sys.exit("error: no frames in %s" % args.frames)

    render = rasterizer(font, args.scale, bg)
    pngs = []
    for path in frames:
        w, h, t, chars, fg, _bg = parse_mvf(path)
        rows = render(w, h, chars, fg)
        out = os.path.join(args.out, os.path.basename(path).replace(".mvf", ".png"))
        write_png(out, w * 8 * args.scale, h * 16 * args.scale, rows)
        pngs.append(out)
    print("wrote %d PNG frames to %s" % (len(pngs), args.out))

    if args.gif:
        if not have("magick") and not have("convert"):
            print("note: ImageMagick not found, skipping GIF", file=sys.stderr)
        else:
            tool = "magick" if have("magick") else "convert"
            delay = max(2, args.step // 10)
            os.makedirs(os.path.dirname(os.path.abspath(args.gif)) or ".", exist_ok=True)
            cmd = [tool, "-delay", str(delay), "-loop", "0"]
            cmd += pngs
            cmd += [args.gif]
            if run(cmd):
                print("wrote %s" % args.gif)

    if args.sheet:
        if not have("magick") and not have("convert"):
            print("note: ImageMagick not found, skipping contact sheet", file=sys.stderr)
        else:
            tool = "magick" if have("magick") else "convert"
            selected = pngs[::max(1, len(pngs) // 9)][:9]
            if tool == "magick":
                cmd = ["magick", "montage"] + selected + ["-tile", args.tile,
                                                          "-geometry", "+4+4",
                                                          "-background", "#111111",
                                                          args.sheet]
            else:
                cmd = ["montage"] + selected + ["-tile", args.tile, "-geometry", "+4+4",
                                                "-background", "#111111", args.sheet]
            if run(cmd):
                print("wrote %s" % args.sheet)


if __name__ == "__main__":
    main()