#!/usr/bin/env python3
"""Run the MV inside a pseudo terminal and check it behaves on a real tty.

This is the only way to exercise the code paths that a pipe never reaches: raw
mode, the alternate screen, cursor hiding, SIGWINCH resizing and the guarantee
that Ctrl-C leaves the terminal exactly as it was.

  tools/ptycheck.py --bin ./mv --seconds 3
"""
import argparse
import fcntl
import os
import pty
import re
import select
import signal
import struct
import sys
import termios
import time

ALT_ON = b"\x1b[?1049h"
ALT_OFF = b"\x1b[?1049l"
CUR_HIDE = b"\x1b[?25l"
CUR_SHOW = b"\x1b[?25h"
CURSOR_RE = re.compile(rb"\x1b\[[0-9;]*H")
CANVAS_BG = b"\x1b[48;2;3;4;9m"   # the explicit background frame_clear paints


def set_size(fd, cols, rows):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def drain(fd, buf, seconds):
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if not r:
            continue
        try:
            data = os.read(fd, 1 << 16)
        except OSError:
            return False
        if not data:
            return False
        buf += data
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "mv"))
    ap.add_argument("--seconds", type=float, default=3.0)
    ap.add_argument("--size", default="80x24")
    ap.add_argument("--resize-to", default="100x30")
    ap.add_argument("--no-audio", action="store_true", default=True)
    ap.add_argument("args", nargs="*", help="extra arguments for the MV")
    a = ap.parse_args()

    cols, rows = (int(v) for v in a.size.split("x"))
    argv = [a.bin]
    if a.no_audio:
        argv.append("--no-audio")
    argv += a.args

    # Pin the terminal capabilities so the colour assertions below are
    # deterministic no matter what the caller's environment looks like.
    os.environ["TERM"] = "xterm-256color"
    os.environ["COLORTERM"] = "truecolor"

    pid, fd = pty.fork()
    if pid == 0:
        os.execv(a.bin, argv)
        os._exit(127)

    set_size(fd, cols, rows)
    buf = bytearray()
    drain(fd, buf, a.seconds * 0.5)

    # Resize while it runs: the program must notice SIGWINCH and redraw.
    rcols, rrows = (int(v) for v in a.resize_to.split("x"))
    set_size(fd, rcols, rrows)
    drain(fd, buf, a.seconds * 0.5)

    # Ctrl-C: the process must exit cleanly and restore the terminal.
    os.write(fd, b"\x03")
    status = None
    end = time.time() + 4.0
    while time.time() < end:
        drain(fd, buf, 0.1)
        p, st = os.waitpid(pid, os.WNOHANG)
        if p == pid:
            status = st
            break
    if status is None:
        os.kill(pid, signal.SIGKILL)
        os.waitpid(pid, 0)
        print("FAIL: did not exit after Ctrl-C")
        return 1

    text = bytes(buf)
    frames = len(CURSOR_RE.findall(text))
    checks = [
        ("entered alternate screen", ALT_ON in text),
        ("left alternate screen", ALT_OFF in text),
        ("hid the cursor", CUR_HIDE in text),
        ("restored the cursor", CUR_SHOW in text),
        ("painted the canvas", CANVAS_BG in text),
        ("drew frames", frames > 20),
        ("handled the resize", text.count(b"\x1b[2J") >= 1),
        ("exited cleanly", os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0),
        ("no traceback", b"Segmentation" not in text and b"assert" not in text),
    ]
    print("captured %d bytes, %d cursor moves, exit status %s"
          % (len(text), frames, status))
    bad = 0
    for name, ok in checks:
        print("  %-24s %s" % (name, "ok" if ok else "FAIL"))
        bad += 0 if ok else 1
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())