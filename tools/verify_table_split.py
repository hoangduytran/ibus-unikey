#!/usr/bin/env python3
"""
Compare split single-byte and double-byte table sources in this repo
against a reference `ukengine/tables/data.cpp` from a git revision or a file.

The default reference (git 2a0bf13) is a monolithic data.cpp with Latin-1 / \\x..
character literals. Use --git 7efc506 only if you accept UTF-8-mojibake sources
(verification will fail on those char literals). Double-byte uses 0x.... UKWORDs.

  ./tools/verify_table_split.py
  ./tools/verify_table_split.py --git 2a0bf13
  ./tools/verify_table_split.py --ref-file /path/to/data.cpp
  ./tools/verify_table_split.py --git ede78c3 --git-path ukengine/data.cpp
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
TOTAL = 213

# Must match the exact // markers in the reference data.cpp
SINGLE = [
    ("// TCVN3", "tcvn3.cpp", "kSingleByteTcvn3"),
    ("//VPS", "vps.cpp", "kSingleByteVps"),
    ("//VISCII", "viscii.cpp", "kSingleByteViscii"),
    ("// BKHCM1", "bkhcm1.cpp", "kSingleByteBkhcm1"),
    ("//Vietware-F", "vietware_f.cpp", "kSingleByteVietwareF"),
    ("// ISC", "isc.cpp", "kSingleByteIsc"),
]

DOUBLE = [
    ("//VNI-WIN", "vni_win.cpp", "kDoubleByteVniWin"),
    ("//BKHCM2", "bkhcm2.cpp", "kDoubleByteBkhcm2"),
    ("//VIETWARE-X", "vietware_x.cpp", "kDoubleByteVietwareX"),
    ("// VNI-MAC", "vni_mac.cpp", "kDoubleByteVniMac"),
]

def read_char_literal(b: bytes, i: int) -> tuple[int, int]:
    if i >= len(b) or b[i] != ord("'"):
        raise ValueError("expected ' at %d" % i)
    i += 1
    if i >= len(b):
        raise ValueError("eof after '")
    if b[i] != ord("\\"):
        v = b[i] & 0xFF
        i += 1
    else:
        i += 1
        if i >= len(b):
            raise ValueError("eof after \\")
        c = b[i]
        if c in (ord("x"), ord("X")):
            i += 1
            n = 0
            for _ in range(2):
                if i >= len(b) or b[i] not in b"0123456789abcdefABCDEF":
                    raise ValueError("short \\x")
                n = n * 16 + int(chr(b[i]), 16)
                i += 1
            v = n & 0xFF
        elif c in b"01234567":
            n = 0
            for _ in range(3):
                if i < len(b) and b[i] in b"01234567":
                    n = n * 8 + (b[i] - ord("0"))
                    i += 1
                else:
                    break
            v = n & 0xFF
        else:
            esc = {
                ord("a"): 7,
                ord("b"): 8,
                ord("f"): 12,
                ord("n"): 10,
                ord("r"): 13,
                ord("t"): 9,
                ord("v"): 11,
                ord("\\"): 92,
                ord("'"): 39,
            }
            v = esc.get(c, c) & 0xFF
            i += 1
    if i < len(b) and b[i] == ord("'"):
        i += 1
    return v, i


def cut_matching_brace(b: bytes, start: int) -> bytes:
    if start >= len(b) or b[start] != ord("{"):
        raise ValueError("expected { at %d" % start)
    depth = 0
    j = start
    n = len(b)
    while j < n:
        if b[j : j + 2] == b"//":  # line comment
            j += 2
            while j < n and b[j] != 10:
                j += 1
            continue
        if b[j] == ord("'"):
            j += 1
            while j < n:
                if b[j] == ord("\\"):
                    j = min(j + 2, n)
                    continue
                if b[j] == ord("'"):
                    j += 1
                    break
                j += 1
            continue
        if b[j] == ord("{"):
            depth += 1
        elif b[j] == ord("}"):
            depth -= 1
            if depth == 0:
                return b[start : j + 1]
        j += 1
    raise ValueError("unclosed brace at %d" % start)


def parse_array_body_u8(block: bytes) -> list[int]:
    assert block and block[0] == ord("{")
    out: list[int] = []
    i = 1
    n = len(block)
    while i < n:
        while i < n and block[i] in b" \t\n\r,":
            i += 1
        if i < n and block[i] == ord("}"):
            break
        if i + 1 < n and block[i : i + 2] == b"//":
            i += 2
            while i < n and block[i] != 10:
                i += 1
            continue
        if i < n and block[i] == ord("0") and i + 1 < n and block[i + 1] in b"xX":
            m = re.match(rb"0x([0-9A-Fa-f]+)\b", block[i:])
            if not m:
                raise ValueError("bad hex at %d" % i)
            out.append(int(m.group(1), 16) & 0xFF)
            i += m.end()
        elif block[i] == ord("'"):
            v, i = read_char_literal(block, i)
            out.append(v)
        else:
            raise ValueError("unexpected at %d: %r" % (i, block[i : i + 32]))
    if i < n and block[i] == ord("}"):
        pass
    return out


def parse_array_body_u16(block: bytes) -> list[int]:
    assert block and block[0] == ord("{")
    out: list[int] = []
    i = 1
    n = len(block)
    while i < n:
        while i < n and block[i] in b" \t\n\r,":
            i += 1
        if i < n and block[i] == ord("}"):
            break
        if i + 1 < n and block[i : i + 2] == b"//":
            i += 2
            while i < n and block[i] != 10:
                i += 1
            continue
        if i < n and block[i] == ord("0") and i + 1 < n and block[i + 1] in b"xX":
            m = re.match(rb"0x([0-9A-Fa-f]+)\b", block[i:])
            if not m:
                raise ValueError("bad hex at %d" % i)
            out.append(int(m.group(1), 16) & 0xFFFF)
            i += m.end()
        else:
            raise ValueError("u16: unexpected at %d: %r" % (i, block[i : i + 20]))
    return out


def _find_row_opening_brace(blob: bytes, marker: str) -> int:
    """Index of the `{` that starts one charset row (TCVN3 uses `{{` — use the inner)."""
    p = blob.find(marker.encode("ascii"))
    if p < 0:
        raise ValueError("marker not found: %r" % marker)
    j = p + len(marker)
    while j < len(blob) and blob[j] in b" \t\n\r":
        j += 1
    if j + 1 < len(blob) and blob[j : j + 2] == b"{{":
        return j + 1
    if j < len(blob) and blob[j] == ord("{"):
        return j
    raise ValueError("no { after %r (at %d): %r" % (marker, j, blob[j : j + 25]))


def extract_table_after_marker(blob: bytes, marker: str) -> bytes:
    i = _find_row_opening_brace(blob, marker)
    return cut_matching_brace(blob, i)


def load_ref_blob(args: argparse.Namespace) -> bytes:
    if args.ref_file:
        return Path(args.ref_file).read_bytes()
    return subprocess.check_output(
        ["git", "show", f"{args.git}:{args.git_path}"],
        cwd=REPO,
    )


def parse_ref_tables(blob: bytes) -> tuple[list[list[int]], list[list[int]]]:
    single: list[list[int]] = []
    for marker, _fn, _s in SINGLE:
        block = extract_table_after_marker(blob, marker)
        vals = parse_array_body_u8(block)
        if len(vals) != TOTAL:
            print(
                "reference %r: %d u8 (expected %d)" % (marker, len(vals), TOTAL),
                file=sys.stderr,
            )
        single.append(vals)
    double: list[list[int]] = []
    for marker, _fn, _s in DOUBLE:
        block = extract_table_after_marker(blob, marker)
        vals = parse_array_body_u16(block)
        if len(vals) != TOTAL:
            print(
                "reference %r: %d u16 (expected %d)" % (marker, len(vals), TOTAL),
                file=sys.stderr,
            )
        double.append(vals)
    return single, double


def parse_split_single() -> list[list[int]]:
    d = REPO / "ukengine" / "tables" / "singlebyte"
    out: list[list[int]] = []
    for _m, name, sym in SINGLE:
        b = (d / name).read_bytes()
        p = b.find(f"{sym}[TOTAL_VNCHARS]".encode("ascii"))
        if p < 0:
            raise SystemExit(f"symbol {sym} not in {name}")
        i = b.find(ord("{"), p)
        if i < 0:
            raise SystemExit("no { in " + name)
        block = cut_matching_brace(b, i)
        vals = parse_array_body_u8(block)
        if len(vals) != TOTAL:
            raise SystemExit(
                f"{name}: {len(vals)} u8, expected {TOTAL}"
            )
        out.append(vals)
    return out


def parse_split_double() -> list[list[int]]:
    d = REPO / "ukengine" / "tables" / "doublebytes"
    out: list[list[int]] = []
    for _m, name, sym in DOUBLE:
        b = (d / name).read_bytes()
        p = b.find(f"{sym}[TOTAL_VNCHARS]".encode("ascii"))
        if p < 0:
            raise SystemExit(f"symbol {sym} not in {name}")
        i = b.find(ord("{"), p)
        if i < 0:
            raise SystemExit("no { in " + name)
        block = cut_matching_brace(b, i)
        vals = parse_array_body_u16(block)
        if len(vals) != TOTAL:
            raise SystemExit(
                f"{name}: {len(vals)} u16, expected {TOTAL}"
            )
        out.append(vals)
    return out


def diff_rows(
    label: str,
    a: list[int],
    b: list[int],
    bits: int,
) -> int:
    mask = 0xFF if bits == 8 else 0xFFFF
    n = 0
    for i, (x, y) in enumerate(zip(a, b)):
        if (x & mask) != (y & mask):
            print("  MISMATCH %s idx %d: ref 0x%x  split 0x%x" % (label, i, x & mask, y & mask))
            n += 1
    return n


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--git",
        default="2a0bf13",
        help="git revision to load ukengine/tables/data.cpp (default: 2a0bf13)",
    )
    p.add_argument(
        "--ref-file",
        type=Path,
        help="path to a monolithic data.cpp (instead of --git)",
    )
    p.add_argument(
        "--git-path",
        default="ukengine/tables/data.cpp",
        metavar="PATH",
        help="path inside the repo at --git (default: ukengine/tables/data.cpp). "
        "Older trees used e.g. ukengine/data.cpp",
    )
    args = p.parse_args()

    try:
        blob = load_ref_blob(args)
    except subprocess.CalledProcessError as e:
        print("Could not read reference:", e, file=sys.stderr)
        return 2

    try:
        ref_s, ref_d = parse_ref_tables(blob)
    except Exception as e:
        print("Failed to parse reference data.cpp:", e, file=sys.stderr)
        return 2

    try:
        sp_s = parse_split_single()
        sp_d = parse_split_double()
    except SystemExit as e:
        print(e, file=sys.stderr)
        return 2

    err = 0
    for i, ((marker, name, _sym), ra, sa) in enumerate(
        zip(SINGLE, ref_s, sp_s)
    ):
        d = diff_rows(f"{name} ({marker.strip()})", ra, sa, 8)
        if d:
            err += d
        else:
            print("OK  single  %s  (%d bytes)" % (name, TOTAL))

    for i, ((marker, name, _sym), ra, sa) in enumerate(
        zip(DOUBLE, ref_d, sp_d)
    ):
        d = diff_rows(f"{name} ({marker.strip()})", ra, sa, 16)
        if d:
            err += d
        else:
            print("OK  double  %s  (%d UKWORD)" % (name, TOTAL))

    if err:
        print("FAILED: %d field(s) differ." % err, file=sys.stderr)
        return 1
    print("All %d single-byte and %d double-byte tables match the reference."
          % (len(SINGLE), len(DOUBLE)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
