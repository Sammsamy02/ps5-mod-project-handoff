#!/usr/bin/env python3
"""decode_session_log.py -- NUL-interleaved UE4SS session-log decoder (agent GZ).

WHY THIS EXISTS (Claude's 2026-10-10 finding): the deployed PS5 UE4SS log
writer is a DUAL-PATH writer whose two paths land in ONE file with TWO
byte grammars, and plain `grep` over the raw file silently misses every
marker that spans a wide run. That is how a refusal line
('[EU-VMT] arming refused: non-text-ent ...' -- reason wide-doubled) could
be overlooked and an evidence row overclaimed (agent-GO row 3 audit
trail). Every harvest/verdict tool must therefore DECODE-THEN-ANALYZE,
never plain-grep the raw bytes.

THE GRAMMAR (grounded in the writer code, not guessed):

  Narrow path  -- literal fmt strings and native StringType args go
    Output::send(STR("...")) -> Output::NewFileDevice (single fd,
    O_WRONLY|O_APPEND) -> FileDevice::receive -> default_format_string ->
    PosixFile::write_string_to_file -> encode_utf8 -> write_all
    (outputs/2026-10-08-l26-runtime/typescan-r4/original/src/File/FileType/
    PosixFile.cpp:293-299 + :138-156). Clean UTF-8; encode_utf8 can never
    emit a 0x00 byte for text, so the narrow stream contains NO NULs.

  Wide path    -- any argument converted through to_generic_string ->
    ensure_str -> to_charT_string resolves the vendored codecvt header on
    some deployed TUs, and the PS5 SDK libc++ locale pairs 2-byte-wchar
    do_in/do_out wrappers with 4-byte-unit helpers, so the converted
    StringType carries the original bytes INTERLEAVED WITH U+0000 UNITS
    ("unit-doubling"; staged-fix comment in port/first/include/Helpers/
    String.hpp:15-22 + outputs/2026-10-08-l26-runtime/logwriter-proposal/
    FIX-PROPOSAL.md section 2 item 3, host-reproduced byte-for-byte by
    race_repro.cpp). encode_utf8 then writes each unit as ONE byte, so a
    wide-written 'non-text-ent' lands on disk as
    'n\\x00o\\x00n\\x00-\\x00t\\x00e\\x00x\\x00t\\x00-\\x00e\\x00n\\x00t\\x00'.

  Race overlay -- the two writer threads share the unlocked fd
    (FIX-PROPOSAL section 2 items 1-2: write_all is completion-atomic,
    not message-atomic), so a narrow message can land INSIDE a wide
    message: mid-line switches, displaced tails and orphan leading-NUL
    fragments ('\\x00s\\x00t\\x00a' preceding a later [Lua] line,
    zzdiag-v2 session proof).

Observed invariants over the ENTIRE retained corpus (22 NUL-carrying
logs, verified 2026-10-08 by this agent): no double NULs (a wide unit
with a zero low byte never occurs), no file-leading NUL, no
file-trailing NUL, and every byte preceding a NUL is ASCII (< 0x80).
Hence wide code units are U+0001..U+007F with high byte 0x00, and the
decode is unambiguous:

    DECODE RULE: a NUL byte exists ONLY as a wide-run high half (or an
    orphaned half split off by the race). Removing every NUL byte
    reconstructs the concatenated text the writers intended; everything
    else is preserved byte-for-byte (narrow UTF-8 passes through
    untouched, so clean host logs are a strict no-op).

The decoder below walks (byte, NUL) pairs explicitly so --stats can
report pair runs vs orphan NULs, but its output is exactly
"NUL-stripped" for this grammar -- with the crucial difference that the
TOOLS now run against decoded text, which is what plain grep failed to
do. Line structure (physical \\n lines, interleaved fragments, partial
trailing lines) is preserved FAITHFULLY: the race's mid-line damage is
evidence, not noise; only the encoding is repaired.

CLI:
    python3 decode_session_log.py IN OUT [--stats] [--json]
    python3 decode_session_log.py IN OUT --raw   # identity copy (diffing)

--stats / --json report: bytes in/out, NUL count, wide-pair count, wide
runs, orphan NULs (race-split halves), line counts (total / narrow /
wide / mixed), BOM handling, partial-final-line flag.

Property tests: run with --self-test (synthetic interleavings built from
the writer grammar + the real menu-up log requirement: the decoded
claude2 menu session must contain BOTH '[EU-VMT] arming refused' AND
'[LUA-FN] in=' -- the exact pair plain grep missed).

Exit codes: 0 ok; 2 usage/file error; 1 self-test failure.
"""

import argparse
import json
import os
import sys

UTF8_BOM = b"\xef\xbb\xbf"
UTF16LE_BOM = b"\xff\xfe"
UTF16BE_BOM = b"\xfe\xff"


def decode_bytes(data: bytes):
    """Decode one raw session-log byte string per the grammar above.

    Returns (decoded_bytes, stats_dict). The walk pairs each byte
    followed by 0x00 as one wide unit (low byte kept); a NUL not
    preceded by a consumable low byte (position 0, after another NUL,
    or -- with pair semantics -- after a byte already consumed as a wide
    low) is an orphan race-split half and is dropped with a count.
    """
    n = len(data)
    out = bytearray()
    wide_pairs = 0
    wide_runs = 0
    orphan_nuls = 0
    i = 0
    in_wide_run = False
    while i < n:
        b = data[i]
        if b == 0x00:
            # NUL never leads a pair (grammar: high half only). If we are
            # here the previous byte was consumed as a narrow byte or as
            # a wide low -- either way this is an orphaned high half from
            # a race-split wide message; drop it, count it.
            orphan_nuls += 1
            in_wide_run = False
            i += 1
            continue
        if i + 1 < n and data[i + 1] == 0x00 and b not in (0x0A, 0x0D):
            # (low, 0x00) wide unit. A newline/CR low byte is EXCLUDED: line
            # breaks are written by the narrow path (format-string tails),
            # so a NUL right after \n is an orphaned race-split high half,
            # never a wide pair. (Text output is unaffected either way --
            # a pair emits its low byte -- this rule only keeps the STATS
            # honest; grounded in the corpus: every NUL-after-newline is a
            # displaced tail fragment per FIX-PROPOSAL section 2 item 2.)
            if not in_wide_run:
                wide_runs += 1
                in_wide_run = True
            out.append(b)
            wide_pairs += 1
            i += 2
        else:
            out.append(b)
            in_wide_run = False
            i += 1
    stats = {
        "bytes_in": n,
        "bytes_out": len(out),
        "nul_count": data.count(b"\x00"),
        "wide_pairs": wide_pairs,
        "wide_runs": wide_runs,
        "orphan_nuls": orphan_nuls,
    }
    return bytes(out), stats


def classify_lines(text: str):
    """Deprecated placeholder kept for API stability; line classification
    lives in line_stats() over the raw bytes (see below)."""
    return None


def line_stats(raw: bytes, decoded: bytes):
    """Per-line stats computed from raw/decoded in lockstep (decode never
    removes newlines: no wide unit is U+000A -- every low byte preceding
    a NUL in the corpus is ASCII text, and a wide-written newline would
    require a Lua payload to embed one, which print() never does)."""
    total = decoded.count(b"\n") + (1 if decoded and not decoded.endswith(b"\n") else 0)
    # wide lines: raw lines (by decoded newline positions) containing pairs
    wide_lines = 0
    mixed_lines = 0
    narrow_lines = 0
    pos = 0
    for raw_line in _iter_lines_lockstep(raw, decoded):
        # count pairs with the SAME rule as decode_bytes (newline lows are
        # narrow; a NUL after \n is an orphan)
        pairs = 0
        k = 0
        while k < len(raw_line):
            b = raw_line[k]
            if b == 0x00:
                k += 1
            elif (k + 1 < len(raw_line) and raw_line[k + 1] == 0x00
                  and b not in (0x0A, 0x0D)):
                pairs += 1
                k += 2
            else:
                k += 1
        narrow_bytes = len(raw_line.replace(b"\x00", b"")) - pairs
        if pairs:
            wide_lines += 1
            if narrow_bytes > 0:
                mixed_lines += 1
        else:
            narrow_lines += 1
    partial_final = bool(decoded) and not decoded.endswith(b"\n")
    return {
        "lines_total": total,
        "lines_narrow": narrow_lines,
        "lines_wide": wide_lines,
        "lines_mixed": mixed_lines,
        "partial_final_line": partial_final,
    }


def _iter_lines_lockstep(raw: bytes, decoded: bytes):
    """Yield raw byte-lines corresponding to decoded lines (split on the
    SAME newline positions; decode preserved every newline byte)."""
    # newline positions in decoded == newline positions in raw (NULs are
    # never newlines), so walk raw and emit segments at each b'\n'.
    start = 0
    for i, b in enumerate(raw):
        if b == 0x0A:
            yield raw[start:i + 1]
            start = i + 1
    if start < len(raw):
        yield raw[start:]


def strip_boms(data: bytes):
    """Handle BOMs: UTF-8 BOM stripped (noted); a whole-file UTF-16 BOM
    re-routes to full UTF-16 decoding (a different, single-path writer --
    not in the retained corpus, handled for robustness). Returns
    (bytes, notes:list)."""
    notes = []
    if data.startswith(UTF8_BOM):
        data = data[len(UTF8_BOM):]
        notes.append("utf8-bom-stripped")
    elif data.startswith(UTF16LE_BOM) or data.startswith(UTF16BE_BOM):
        enc = "utf-16-le" if data.startswith(UTF16LE_BOM) else "utf-16-be"
        try:
            text = data.decode(enc)
            notes.append("whole-file-%s-decoded" % enc)
            return text.encode("utf-8"), notes
        except UnicodeDecodeError:
            notes.append("%s-bom-present-but-not-whole-file-utf16 (treated as dual-path)" % enc)
    else:
        notes.append("bom-less")
    return data, notes


def decode_file(in_path: str, raw: bool = False):
    with open(in_path, "rb") as f:
        data = f.read()
    notes = []
    if raw:
        return data, {"mode": "raw"}, ["raw-copy (no decode)"]
    data, bom_notes = strip_boms(data)
    notes.extend(bom_notes)
    decoded, stats = decode_bytes(data)
    stats.update(line_stats(data, decoded))
    stats["mode"] = "decode"
    return decoded, stats, notes


# ---------------------------------------------------------------------------
# Property tests (--self-test)
# ---------------------------------------------------------------------------

def _wide(s: str) -> bytes:
    """Encode like the corrupted wide path: each ASCII char + NUL."""
    return b"".join(bytes([ord(c), 0x00]) for c in s)


def _self_test(argv):
    import random
    import tempfile

    failures = []

    def check(name, cond, detail=""):
        if cond:
            print("  PASS %s" % name)
        else:
            failures.append(name)
            print("  FAIL %s %s" % (name, detail))

    print("== property tests: synthetic interleavings (writer grammar) ==")

    # P1: clean narrow log is a strict no-op (idempotence baseline).
    clean = b"[2026-10-10 00:53:05] UE4SS - v3.0.1 - Git SHA #e3ba101\n[2026-10-10 00:53:05] [l22] Lua mod started\n"
    d, s = decode_bytes(clean)
    check("clean-narrow-noop", d == clean and s["nul_count"] == 0, repr(d[:60]))

    # P2: pure wide Lua print line decodes to readable text.
    line = b"[Lua] " + _wide("[zz] K1 f7") + b"\n"
    d, s = decode_bytes(line)
    check("wide-lua-print", d == b"[Lua] [zz] K1 f7\n" and s["wide_pairs"] == 10,
          repr(d))

    # P3: mid-line switch narrow->wide->narrow (fmt literal + converted arg
    # + literal tail -- the '[EU-VMT] arming refused' shape).
    line = (b"[EU-VMT] arming refused: " + _wide("non-text-ent")
            + b" (R3 carriers remain the pump)\n")
    d, _ = decode_bytes(line)
    check("mid-line-switch", d == b"[EU-VMT] arming refused: non-text-ent (R3 carriers remain the pump)\n", repr(d))

    # P4: race interleave -- narrow message lands inside a wide one; the
    # decode preserves the fragment order faithfully (evidence, not noise).
    # Model: wide message 'TownLoaded' is split mid-run by the narrow line;
    # the low byte 'L' stays with the first fragment, its high NUL leads
    # the displaced tail (the FIX-PROPOSAL orphan-fragment shape
    # '\x00s\x00t\x00a' preceding a later [Lua] line).
    raw = (b"[Lua] " + _wide("Town") + b"L"
           + b"[l22] ExecuteInGameThread callback returned ref=12\n"
           + b"\x00" + _wide("oaded") + b"\n")
    d, s = decode_bytes(raw)
    expected = (b"[Lua] TownL[l22] ExecuteInGameThread callback returned ref=12\n"
                b"oaded\n")
    check("race-interleave-faithful", d == expected, repr(d))
    check("race-interleave-orphan-counted", s["orphan_nuls"] == 1, str(s))

    # P5: NUL-padding tail (wide run then padding NULs then newline) and
    # partial trailing line without newline.
    raw = b"[Seeds] " + _wide("slot 1") + b"\x00\x00\x00" + b"tail-no-newline"
    d, s = decode_bytes(raw)
    check("nul-padding", d == b"[Seeds] slot 1tail-no-newline" and s["orphan_nuls"] == 3, repr(d))

    # P6: random interleavings -- decode is exactly NUL-removal and never
    # invents/loses non-NUL bytes (grounding invariant).
    rng = random.Random(20261008)
    for trial in range(200):
        frags = []
        for _ in range(rng.randint(1, 8)):
            if rng.random() < 0.5:
                frags.append(bytes(rng.randint(1, 255) or 1 for _ in range(rng.randint(0, 30))).replace(b"\x00", b""))
            else:
                frags.append(_wide("".join(chr(rng.randint(33, 126)) for _ in range(rng.randint(0, 20)))))
            if rng.random() < 0.2:
                frags.append(b"\n")
        blob = b"".join(frags)
        d, s = decode_bytes(blob)
        if d != blob.replace(b"\x00", b""):
            check("random-trial-%d" % trial, False, repr(blob[:50]))
            break
    else:
        check("random-200-trials-equals-nul-drop", True)

    # P7: decode idempotence: decoding decoded output is a no-op.
    d1, _ = decode_bytes(raw)
    d2, s2 = decode_bytes(d1)
    check("idempotent", d1 == d2 and s2["nul_count"] == 0)

    print("== property tests: the real menu-up log (the plain-grep miss) ==")
    here = os.path.dirname(os.path.abspath(__file__))
    real = os.path.join(here, "..", "evidence", "claude2-menu-session-20261010.log")
    if not os.path.isfile(real):
        # self-test must stay honest if run from another checkout
        print("  SKIP real-log test (evidence file not found: %s)" % real)
    else:
        with open(real, "rb") as f:
            data = f.read()
        decoded, stats = decode_bytes(data)
        text = decoded.decode("utf-8", errors="replace")
        check("real-log-contains-EU-VMT-refusal",
              "[EU-VMT] arming refused" in text)
        check("real-log-contains-wide-reason",
              "arming refused: non-text-ent" in text,
              "the wide-doubled reason must become greppable")
        check("real-log-contains-LUA-FN-in",
              "[LUA-FN] in=" in text)
        # the exact plain-grep miss: raw contiguous grep for the refusal
        # reason fails, decoded succeeds
        check("raw-grep-misses-reason",
              b"non-text-ent" not in data)
        check("decoded-grep-finds-reason",
              "non-text-ent" in decoded.decode("utf-8", "replace"))
        # sha of raw preserved untouched on disk (we never rewrite inputs)
        check("stats-sane",
              stats["nul_count"] == data.count(b"\x00")
              and stats["bytes_out"] == stats["bytes_in"] - stats["nul_count"],
              str(stats))

    print("== self-test verdict: %s (%d failure(s)) ==" %
          ("PASS" if not failures else "FAIL", len(failures)))
    return 0 if not failures else 1


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main(argv):
    ap = argparse.ArgumentParser(
        prog="decode_session_log.py",
        description="Decode NUL-interleaved PS5 UE4SS session logs (dual-path "
                    "writer: narrow UTF-8 + wide-doubled runs) into clean UTF-8. "
                    "Grounded in the writer grammar; see module docstring.")
    ap.add_argument("infile", nargs="?", default=None,
                    help="raw session log (any size)")
    ap.add_argument("outfile", nargs="?", default=None,
                    help="decoded output path (stdout with '-')")
    ap.add_argument("--stats", action="store_true",
                    help="print one-line decode stats to stderr")
    ap.add_argument("--json", action="store_true",
                    help="print full stats JSON to stderr")
    ap.add_argument("--raw", action="store_true",
                    help="identity copy WITHOUT decoding (for --raw diffing of "
                         "tool behavior; keeps old plain-grep mode possible)")
    ap.add_argument("--self-test", action="store_true",
                    help="run property tests (synthetic interleavings + the "
                         "real menu-up log) and exit")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test(argv)

    if not args.infile or not args.outfile:
        ap.error("infile and outfile are required (unless --self-test)")

    if not os.path.isfile(args.infile):
        sys.stderr.write("ERROR: input not found: %s\n" % args.infile)
        return 2
    try:
        with open(args.infile, "rb") as f:
            data = f.read()
    except OSError as e:
        sys.stderr.write("ERROR: cannot read %s: %s\n" % (args.infile, e))
        return 2

    notes = []
    if args.raw:
        decoded, stats = data, {"mode": "raw"}
        notes.append("raw-copy (no decode)")
    else:
        data2, bom_notes = strip_boms(data)
        notes.extend(bom_notes)
        decoded, stats = decode_bytes(data2)
        stats.update(line_stats(data2, decoded))
        stats["mode"] = "decode"
    stats["notes"] = notes

    if args.outfile == "-":
        sys.stdout.buffer.write(decoded)
    else:
        try:
            with open(args.outfile, "wb") as f:
                f.write(decoded)
        except OSError as e:
            sys.stderr.write("ERROR: cannot write %s: %s\n" % (args.outfile, e))
            return 2

    if args.json:
        sys.stderr.write(json.dumps(stats, indent=2, sort_keys=True) + "\n")
    elif args.stats:
        sys.stderr.write(
            "[decode] %s: %d->%d bytes, nuls=%d wide-pairs=%d wide-runs=%d "
            "orphan-nuls=%d lines=%d (narrow=%d wide=%d mixed=%d)%s\n" % (
                os.path.basename(args.infile), stats.get("bytes_in", len(data)),
                stats.get("bytes_out", len(decoded)), stats.get("nul_count", 0),
                stats.get("wide_pairs", 0), stats.get("wide_runs", 0),
                stats.get("orphan_nuls", 0), stats.get("lines_total", 0),
                stats.get("lines_narrow", 0), stats.get("lines_wide", 0),
                stats.get("lines_mixed", 0),
                " partial-final-line" if stats.get("partial_final_line") else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
