#!/usr/bin/env python3
"""coverage_report_validator.py — rc-seed-coverage/1 report checker.

Unknown-byte policy enforcer for preflight fixtures of the SeedValidation
pool/FName validator chain (see results/COVERAGE-LEDGER.md, section 2).

Studio rule being enforced: missing captured heap bytes are explicitly
UNKNOWN. A report may only claim:
  - "address-covered"  : bytes/ranges whose ADDRESSES were exercised by
                         pure address/size arithmetic (membership, bounds,
                         alignment, decomposition) — no byte knowledge.
  - "bytes-known"      : ranges whose CONTENT was actually read back by the
                         fixture through a read-backed exercise record.

The validator FAILS (exit 1) any report that:
  1. uses a schema other than "rc-seed-coverage/1";
  2. contains a "bytes-known" claim not FULLY covered by read-backed
     exercise ranges (i.e. coverage claimed over unexercised bytes);
  3. contains an "address-covered" claim not covered by (address-
     arithmetic OR read-backed) exercise ranges;
  4. records a read-backed exercise on a span that is unpublished or whose
     protection lacks the READ bit ('r') (address-arithmetic exercise is
     lawful on any listed span, including unpublished exec-only text —
     that is what classify_seed_address does);
  5. has an unknown_range overlapping a READ-BACKED range (bytes actually
     read back cannot be unknown);
  6. fails to declare as unknown every address-arithmetic-exercised byte
     that has NO read-backed record (an honest report treats the contents
     of address-arithmetic-only bytes as explicitly UNKNOWN);
  7. claims coverage of bytes at or above capture.envelope_end while
     capture.truncated is true (those bytes are UNKNOWN: omitted, not
     proven unmapped);
  8. records exercised ranges at/above the truncated capture envelope;
  9. has read-backed exercise records with an empty/missing "ops"
     provenance list.

Usage:
    python3 coverage_report_validator.py REPORT.json [REPORT2.json ...]

Exit codes: 0 = all reports conform; 1 = at least one violation.
This tool inspects JSON reports only; it never reads game memory and makes
no claims about any live process. Fixture evidence proves code behavior,
never live pool contents.
"""

import json
import sys

SCHEMA = "rc-seed-coverage/1"


class Violations:
    def __init__(self, name):
        self.name = name
        self.items = []

    def add(self, msg):
        self.items.append(msg)

    def ok(self):
        return not self.items


def parse_addr(value, where, v):
    """Accept int or '0x...' / decimal string; return non-negative int."""
    if isinstance(value, bool):
        v.add(f"{where}: boolean is not an address")
        return None
    if isinstance(value, int):
        if value < 0:
            v.add(f"{where}: negative address {value}")
            return None
        return value
    if isinstance(value, str):
        text = value.strip()
        try:
            return int(text, 0)
        except ValueError:
            v.add(f"{where}: unparseable address {value!r}")
            return None
    v.add(f"{where}: address must be int or string, got {type(value).__name__}")
    return None


def parse_range(obj, where, v):
    lo = parse_addr(obj.get("lo"), f"{where}.lo", v)
    hi = parse_addr(obj.get("hi"), f"{where}.hi", v)
    if lo is None or hi is None:
        return None
    if hi <= lo:
        v.add(f"{where}: empty/inverted range [{lo:#x},{hi:#x})")
        return None
    return (lo, hi)


def union(intervals):
    """Merge overlapping/adjacent [lo,hi) intervals."""
    merged = []
    for lo, hi in sorted(intervals):
        if merged and lo <= merged[-1][1]:
            if hi > merged[-1][1]:
                merged[-1] = (merged[-1][0], hi)
        else:
            merged.append((lo, hi))
    return merged


def subtract(rng, merged_universe):
    """Return sub-intervals of rng NOT covered by the merged universe."""
    lo, hi = rng
    out = []
    for u_lo, u_hi in merged_universe:
        if u_hi <= lo:
            continue
        if u_lo >= hi:
            break
        if u_lo > lo:
            out.append((lo, min(u_lo, hi)))
        lo = max(lo, u_hi)
        if lo >= hi:
            break
    if lo < hi:
        out.append((lo, hi))
    return [(a, b) for a, b in out if b > a]


def fully_covered(claim, universe):
    """True iff [claim.lo, claim.hi) is a subset of the merged universe."""
    need_lo, need_hi = claim
    for lo, hi in universe:
        if hi <= need_lo:
            continue
        if lo > need_lo:
            return False
        if hi >= need_hi:
            return True
        need_lo = hi
    return False


def check_report(path):
    v = Violations(path)
    with open(path, "r", encoding="utf-8") as fh:
        try:
            report = json.load(fh)
        except json.JSONDecodeError as exc:
            v.add(f"invalid JSON: {exc}")
            return v

    if not isinstance(report, dict):
        v.add("top level must be an object")
        return v
    if report.get("schema") != SCHEMA:
        v.add(f"schema must be {SCHEMA!r}, got {report.get('schema')!r}")
        return v

    capture = report.get("capture") or {}
    truncated = bool(capture.get("truncated", False))
    envelope_end = parse_addr(
        capture.get("envelope_end"), "capture.envelope_end", v
    ) if capture.get("envelope_end") is not None else None
    if truncated and envelope_end is None:
        v.add("capture.truncated=true requires capture.envelope_end")

    arith = []      # address-arithmetic-only exercise
    readback = []   # read-backed exercise
    claims_addr = []
    claims_bytes = []
    unknown = []

    spans = report.get("spans") or []
    if not isinstance(spans, list) or not spans:
        v.add("spans must be a non-empty array")

    for si, span in enumerate(spans):
        where = f"spans[{si}]"
        if not isinstance(span, dict):
            v.add(f"{where}: must be an object")
            continue
        srange = parse_range(span, where, v)
        prot = str(span.get("protection", ""))
        readable = "r" in prot
        published = bool(span.get("published", False))

        for ei, ex in enumerate(span.get("exercised") or []):
            ewhere = f"{where}.exercised[{ei}]"
            if not isinstance(ex, dict):
                v.add(f"{ewhere}: must be an object")
                continue
            rng = parse_range(ex, ewhere, v)
            if rng is None:
                continue
            if srange is not None and not fully_covered(rng, [srange]):
                v.add(
                    f"{ewhere}: exercised [{rng[0]:#x},{rng[1]:#x}) is not inside "
                    f"its span [{srange[0]:#x},{srange[1]:#x})"
                )
            kind = ex.get("kind")
            if kind == "address-arithmetic":
                arith.append(rng)
            elif kind == "read-backed":
                if not published or not readable:
                    v.add(
                        f"{ewhere}: read-backed exercise requires a PUBLISHED "
                        f"span with READ protection (got published={published}, "
                        f"protection={prot!r})"
                    )
                ops = ex.get("ops")
                if not isinstance(ops, list) or not ops:
                    v.add(f"{ewhere}: read-backed exercise requires non-empty ops provenance")
                readback.append(rng)
            else:
                v.add(f"{ewhere}: kind must be 'address-arithmetic' or 'read-backed', got {kind!r}")
            if truncated and envelope_end is not None and rng[1] > envelope_end:
                v.add(
                    f"{ewhere}: exercised [{rng[0]:#x},{rng[1]:#x}) reaches past the "
                    f"truncated capture envelope {envelope_end:#x} (bytes there are UNKNOWN)"
                )

    for ui, unk in enumerate(report.get("unknown_ranges") or []):
        uwhere = f"unknown_ranges[{ui}]"
        if not isinstance(unk, dict):
            v.add(f"{uwhere}: must be an object")
            continue
        rng = parse_range(unk, uwhere, v)
        if rng is None:
            continue
        if not unk.get("reason"):
            v.add(f"{uwhere}: unknown range requires a reason")
        unknown.append(rng)

    read_u = union(readback)
    unknown_u = union(unknown)

    # U1: bytes actually read back can never be declared unknown.
    for ui, rng in enumerate(unknown):
        for rrng in read_u:
            lo = max(rng[0], rrng[0])
            hi = min(rng[1], rrng[1])
            if lo < hi:
                v.add(
                    f"unknown_ranges[{ui}]: unknown range [{rng[0]:#x},{rng[1]:#x}) "
                    f"overlaps a read-backed range [{rrng[0]:#x},{rrng[1]:#x}) — "
                    f"bytes that were read back cannot be declared unknown"
                )

    # U2 (the honesty rule): every address-arithmetic-exercised byte with no
    # read-backed record MUST be declared unknown. A report that leaves such
    # bytes undeclared is silently treating address knowledge as byte knowledge.
    for ai, rng in enumerate(arith):
        for lo, hi in subtract(rng, read_u):
            if not fully_covered((lo, hi), unknown_u):
                v.add(
                    f"address-arithmetic exercise #{ai} [{rng[0]:#x},{rng[1]:#x}): "
                    f"bytes [{lo:#x},{hi:#x}) have no read-backed record and are NOT "
                    f"declared in unknown_ranges — undeclared unknown bytes are forbidden"
                )

    for ci, claim in enumerate(report.get("claims") or []):
        cwhere = f"claims[{ci}]"
        if not isinstance(claim, dict):
            v.add(f"{cwhere}: must be an object")
            continue
        rng = parse_range(claim, cwhere, v)
        if rng is None:
            continue
        kind = claim.get("kind")
        if kind == "bytes-known":
            claims_bytes.append((rng, cwhere))
        elif kind == "address-covered":
            claims_addr.append((rng, cwhere))
        else:
            v.add(f"{cwhere}: kind must be 'bytes-known' or 'address-covered', got {kind!r}")
        if truncated and envelope_end is not None and rng[1] > envelope_end:
            v.add(
                f"{cwhere}: {kind} claim [{rng[0]:#x},{rng[1]:#x}) reaches past the "
                f"truncated capture envelope {envelope_end:#x}; those bytes must be "
                f"listed in unknown_ranges instead"
            )

    arith_u = union(arith)
    read_u = union(readback)
    any_u = union(arith + readback)

    for rng, where in claims_bytes:
        if not fully_covered(rng, read_u):
            v.add(
                f"{where}: bytes-known claim [{rng[0]:#x},{rng[1]:#x}) covers bytes "
                f"with NO read-backed exercise record (claimed coverage of "
                f"unexercised bytes is forbidden)"
            )
    for rng, where in claims_addr:
        if not fully_covered(rng, any_u):
            v.add(
                f"{where}: address-covered claim [{rng[0]:#x},{rng[1]:#x}) covers "
                f"addresses with NO exercise record of any kind"
            )
        # A bytes-known claim inside an address-covered claim must be read-backed;
        # address-covered claims carried ONLY by read-back are upgraded honestly,
        # so no extra check is needed here beyond the subset test above.

    return v


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    failed = False
    for path in argv[1:]:
        v = check_report(path)
        if v.ok():
            print(f"PASS {path}")
        else:
            failed = True
            print(f"FAIL {path}")
            for item in v.items:
                print(f"  - {item}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
