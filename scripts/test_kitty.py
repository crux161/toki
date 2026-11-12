#!/usr/bin/env python3

"""End-to-end check of `toki --kitty` under a pseudo-terminal.

The script plays the terminal: it answers toki's protocol support query the
way kitty would, captures everything toki writes, then stops it with SIGINT
and checks the byte stream against the graphics protocol:

  - the a=t transmission is chunked at 4096 base64 chars, non-final chunks
    carry m=1 and only the m/q keys, and the payload reassembles to exactly
    width * height * 3 RGB bytes
  - the placement command re-places image 1 without moving the cursor
  - teardown deletes the image and restores the cursor
  - an unchanged face causes no second transmission

Toki runs in a scratch directory with a scratch HOME, so the default theme
(green #00FF00 on #101010, effects off) fixes the expected pixel values.

Usage: python3 scripts/test_kitty.py [path-to-toki]
"""

import base64
import os
import pty
import re
import select
import signal
import subprocess
import sys
import tempfile
import time

TOKI = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "cmake-build/toki")

# The spec's detection pair, and the replies kitty itself would give.
QUERY = b"\x1b_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\x1b\\\x1b[c"
QUERY_REPLY = b"\x1b_Gi=31;OK\x1b\\\x1b[?62c"

PLACE_CMD = b"\x1b_Ga=p,i=1,p=1,C=1,q=1\x1b\\"
DELETE_CMD = b"\x1b_Ga=d,d=I,i=1\x1b\\"

# APC G <control> [; <base64 payload>] ST, control split from payload at ';'.
APC = re.compile(rb"\x1b_G([^;\x1b]*)(?:;([A-Za-z0-9+/=]*))?\x1b\\")


def parse_keys(control):
    return dict(kv.split("=", 1) for kv in control.split(",") if "=" in kv)


def find_transmission(stream):
    """Returns (keys, chunks) for the first a=t command in the stream.

    chunks is the list of (keys, payload) of every chunk belonging to it,
    first (full control data) to last.
    """
    seqs = [(parse_keys(m.group(1).decode()), m.group(2) or b"")
            for m in APC.finditer(stream)]

    for i, (keys, payload) in enumerate(seqs):
        if keys.get("a") != "t":
            continue
        chunks = [(keys, payload)]
        if keys.get("m", "0") == "1":
            for keys2, payload2 in seqs[i + 1:]:
                chunks.append((keys2, payload2))
                if keys2.get("m", "0") == "0":
                    break
        return chunks
    return []


def run_kitty(args, respond, capture_seconds):
    """Runs toki --kitty on a pty; returns (output, exit code, stderr)."""
    output = b""

    with tempfile.TemporaryDirectory() as sandbox:
        master, slave = pty.openpty()
        proc = subprocess.Popen(
            [TOKI, "--kitty", *args],
            stdin=slave, stdout=slave, stderr=subprocess.PIPE,
            cwd=sandbox, env=dict(os.environ, HOME=sandbox),
            start_new_session=True)
        os.close(slave)

        replied = False
        deadline = time.monotonic() + capture_seconds
        try:
            while time.monotonic() < deadline:
                ready, _, _ = select.select([master], [], [], 0.05)
                if not ready:
                    if proc.poll() is not None:
                        break
                    continue
                try:
                    chunk = os.read(master, 1 << 16)
                except OSError:
                    break
                if not chunk:
                    break
                output += chunk
                if respond and not replied and QUERY in output:
                    os.write(master, QUERY_REPLY)
                    replied = True

            if proc.poll() is None:
                proc.send_signal(signal.SIGINT)
        finally:
            # Drain whatever teardown writes after the signal, then collect.
            while True:
                ready, _, _ = select.select([master], [], [], 0.2)
                if not ready:
                    break
                try:
                    chunk = os.read(master, 1 << 16)
                except OSError:
                    break
                if not chunk:
                    break
                output += chunk
            os.close(master)

        stderr = proc.communicate(timeout=10)[1]
        return output, proc.returncode, stderr


def check_frame(output, width, height):
    chunks = find_transmission(output)
    assert chunks, "no a=t transmission found in output"

    first = chunks[0][0]
    assert first["f"] == "24", first
    assert first["s"] == str(width), first
    assert first["v"] == str(height), first
    assert first["i"] == "1", first
    assert first.get("q") == "1", first

    for index, (keys, payload) in enumerate(chunks):
        assert len(payload) <= 4096, len(payload)
        if index < len(chunks) - 1:
            assert keys.get("m") == "1", keys
            assert len(payload) % 4 == 0, len(payload)
        if index > 0:
            assert set(keys) <= {"q", "m"}, keys
    assert chunks[-1][0].get("m", "0") == "0", chunks[-1][0]

    data = base64.b64decode(b"".join(payload for _, payload in chunks))
    assert len(data) == width * height * 3, len(data)

    # Default theme: #101010 background everywhere the glyphs are not.
    assert data[0:3] == b"\x10\x10\x10", data[0:3]
    assert data[-3:] == b"\x10\x10\x10", data[-3:]
    assert b"\x00\xff\x00" in data, "no foreground pixels found"


def test_positive():
    for args, (width, height) in [([], (360, 72)), (["-s", "4"], (90, 18))]:
        output, code, stderr = run_kitty(args, respond=True, capture_seconds=2.0)
        assert code == 0, f"exit {code}, stderr: {stderr.decode()}"
        assert b"\x1b[?25l" in output, "cursor was not hidden"

        check_frame(output, width, height)

        # Static face within the capture window: exactly one transmission.
        transmissions = sum(1 for m in APC.finditer(output)
                            if m.group(1).startswith(b"a=t,"))
        assert transmissions == 1, f"{transmissions} transmissions for a static face"
        assert output.count(PLACE_CMD) == 1, "expected exactly one placement"

        # Teardown after SIGINT.
        assert DELETE_CMD in output, "image was not deleted on exit"
        assert b"\x1b[?25h" in output, "cursor was not restored on exit"


def test_unsupported_terminal():
    output, code, stderr = run_kitty([], respond=False, capture_seconds=0.5)
    assert code == 1, f"exit {code}, stderr: {stderr.decode()}"
    assert b"does not support" in stderr, stderr.decode()
    assert b"a=t" not in output, "transmitted without confirmed support"


def main():
    for name, test in [("supported terminal", test_positive),
                       ("unsupported terminal", test_unsupported_terminal)]:
        test()
        print(f"ok: {name}")
    print("all kitty render tests passed")


if __name__ == "__main__":
    main()
