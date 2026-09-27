#!/usr/bin/env python3
"""Bake the song's lyric timeline (LRC + per-word YRC) into src/lyrics_data.h.

Input: the .lrc file (clean, human-readable line text + line timing) and the
.yrc file (per-word timing for karaoke highlighting).  Both are expected to
describe the same 129 lines; the script fails loudly if they do not line up,
because the whole MV scene table is anchored to lyric line indices.

Usage:
  tools/gen_lyrics.py                       # uses ../world.execute (me) ;.{lrc,yrc}
  tools/gen_lyrics.py --lrc A.lrc --yrc A.yrc --out src/lyrics_data.h
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
SONG_DIR = os.path.abspath(os.path.join(ROOT, ".."))
DEFAULT_LRC = os.path.join(SONG_DIR, "world.execute (me) ;.lrc")
DEFAULT_YRC = os.path.join(SONG_DIR, "world.execute (me) ;.yrc")
DEFAULT_OUT = os.path.join(ROOT, "src", "lyrics_data.h")

TOTAL_MS = 212000      # playback length: music ends 207.5s, tail silence to 211.9s
BPM = 130

LRC_RE = re.compile(r"^\[(\d+):(\d+)[.:](\d+)\](.*)$")
YRC_HEAD_RE = re.compile(r"^\[(\d+),(\d+)\]")
YRC_MARK_RE = re.compile(r"\((\d+),(\d+),(\d+)\)")
# Full-width punctuation shows up in the .yrc word pieces; fold it back to ASCII.
FOLD = {"（": "(", "）": ")", "；": ";", "，": ",", "：": ":", "！": "!", "？": "?"}
KW_RE = re.compile(r"^[A-Z][A-Z'\-]*$")


def read_lrc(path):
    lines = []
    with open(path, encoding="utf-8") as fh:
        for raw in fh:
            raw = raw.rstrip("\n")
            m = LRC_RE.match(raw)
            if not m:
                continue
            mm, ss, frac, text = m.groups()
            if len(frac) == 2:
                ms = int(frac) * 10
            elif len(frac) == 3:
                ms = int(frac)
            else:
                ms = int(frac[:3].ljust(3, "0"))
            start = (int(mm) * 60 + int(ss)) * 1000 + ms
            lines.append((start, text.strip()))
    lines.sort(key=lambda x: x[0])
    return lines


def read_yrc(path):
    out = []
    with open(path, encoding="utf-8") as fh:
        for raw in fh:
            raw = raw.rstrip("\n")
            head = YRC_HEAD_RE.match(raw)
            if not head:
                continue
            start, dur = int(head.group(1)), int(head.group(2))
            body = raw[head.end():]
            marks = list(YRC_MARK_RE.finditer(body))
            words = []
            for i, mk in enumerate(marks):
                end = marks[i + 1].start() if i + 1 < len(marks) else len(body)
                text = body[mk.end():end]
                text = "".join(FOLD.get(ch, ch) for ch in text)
                words.append((int(mk.group(1)), int(mk.group(2)), text))
            # The first mark's own offset is authoritative for the line start.
            if marks:
                start = int(marks[0].group(1))
            out.append((start, dur, words))
    out.sort(key=lambda x: x[0])
    return out


def cstr(s):
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "?":
            out.append("\\?")   # avoid trigraph surprises
        elif 32 <= ord(ch) < 127:
            out.append(ch)
        else:
            out.append("?")
    return '"' + "".join(out) + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lrc", default=DEFAULT_LRC)
    ap.add_argument("--yrc", default=DEFAULT_YRC)
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()

    for p in (args.lrc, args.yrc):
        if not os.path.exists(p):
            sys.exit("error: missing %s" % p)

    lrc = read_lrc(args.lrc)
    yrc = read_yrc(args.yrc)
    if len(lrc) != len(yrc):
        sys.exit("error: %d lrc lines vs %d yrc lines" % (len(lrc), len(yrc)))

    lines = []
    for i, ((lstart, ltext), (ystart, ydur, words)) in enumerate(zip(lrc, yrc)):
        # The .yrc (per-word) timing is authoritative; the .lrc one is only a
        # sanity check that the two files describe the same performance.  A few
        # lines legitimately differ (the drawn-out title line), so only warn.
        if abs(lstart - ystart) > 300:
            print("warn: line %d lrc=%dms yrc=%dms (using yrc)" % (i, lstart, ystart),
                  file=sys.stderr)
        # Keep only pieces that actually carry characters.
        clean = []
        for ws, wd, wt in words:
            if wt.strip("\u3000 \t") == "" and wt == "":
                continue
            clean.append((ws, wd, wt))
        joined = "".join(w for _, _, w in clean).strip()
        if joined.replace(" ", "") != ltext.replace(" ", ""):
            print("warn: line %d text differs: lrc=%r yrc=%r" % (i, ltext, joined), file=sys.stderr)
        lines.append((ystart, ydur, ltext, clean))

    # end_ms: a line runs until the next line starts (or to the end of the song).
    total_end = TOTAL_MS
    flat_words = []
    for i, (start, dur, text, words) in enumerate(lines):
        end = lines[i + 1][0] if i + 1 < len(lines) else min(total_end, start + dur)
        lines[i] = (start, end, text, words)
        for ws, wd, wt in words:
            flat_words.append((ws, wd, wt, i))

    out = os.path.abspath(args.out)
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("/* Generated by tools/gen_lyrics.py -- do not edit by hand.\n"
                 " * Source: %s + .yrc (per-word timing).\n"
                 " * Line indices below are the anchors used by src/scenes.c. */\n"
                 % os.path.basename(args.lrc))
        fh.write("#ifndef MV_LYRICS_DATA_H\n#define MV_LYRICS_DATA_H\n\n")
        fh.write("#define LYRIC_TOTAL_MS %d\n" % TOTAL_MS)
        fh.write("#define LYRIC_END_MS   %d\n" % lines[-1][1])
        fh.write("#define LYRIC_BPM      %d\n\n" % BPM)
        fh.write("#if defined(__GNUC__)\n#  define MV_UNUSED __attribute__((unused))\n"
                 "#else\n#  define MV_UNUSED\n#endif\n\n")
        fh.write("typedef struct { int start_ms, dur_ms; const char *text; } MvWord;\n")
        fh.write("typedef struct {\n"
                 "    int start_ms, end_ms;   /* audible span of the phrase   */\n"
                 "    const char *text;       /* clean text from the .lrc     */\n"
                 "    int first_word, n_words;\n"
                 "} MvLyric;\n\n")
        fh.write("static const MvWord LYRIC_WORDS[] MV_UNUSED = {\n")
        for ws, wd, wt, _ in flat_words:
            fh.write("    {%7d,%6d,%s},\n" % (ws, wd, cstr(wt)))
        fh.write("};\n\n")
        fh.write("static const MvLyric LYRIC_LINES[] MV_UNUSED = {\n")
        widx = 0
        for start, end, text, words in lines:
            fh.write("    {%7d,%7d,%-42s,%4d,%3d}, /* %s */\n"
                     % (start, end, cstr(text), widx, len(words), text[:46]))
            widx += len(words)
        fh.write("};\n\n")
        fh.write("#define LYRIC_WORD_COUNT %d\n" % len(flat_words))
        fh.write("#define LYRIC_LINE_COUNT %d\n\n" % len(lines))
        fh.write("#endif /* MV_LYRICS_DATA_H */\n")

    keywords = sum(1 for _, _, w, _ in flat_words if KW_RE.match(w.strip()) and len(w.strip()) >= 4)
    print("wrote %s: %d lines, %d words, %d keyword words" % (out, len(lines), len(flat_words), keywords))
    print("song anchor: first=%dms last_end=%dms" % (lines[0][0], lines[-1][1]))


if __name__ == "__main__":
    main()