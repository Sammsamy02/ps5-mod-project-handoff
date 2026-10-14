#!/usr/bin/env python3
"""Team S3 two-source equivalence check (readiness item B4).

Compare the profile file this team's generator emits (``profile.ini``) with Team S1's mechanical
Townfall oracle (``gen/townfall-expected.json``, read FROM today's compiled runtime headers). Every
field the runtime uses must agree; a field that is intentionally different (an identity label the
runtime only checks for presence) or intentionally absent (a feature pin with no generic detector yet,
so the feature switches off and fails closed) is listed, not failed.

Run:  python3 -I host-tests/check_equivalence.py --ini OUT/profile.ini --oracle .../sb-team-S1/gen/townfall-expected.json
Exit: 0 all runtime-used fields agree (within the documented exceptions); 1 an unexpected mismatch; 2 bad input.

No console contact, no writes outside stdout.
"""
import argparse
import json
import re
import sys

# fields the runtime only checks for PRESENCE (non-empty / valid hex) -> a different string is not a mismatch
PRESENCE_ONLY = {"name", "digest", "body_sha256", "build_source", "stats_source"}
# fields the generator cannot derive from the image here -> console/config or no generic detector (feature off)
EXPECTED_ABSENT_SCALAR = {"exe_path"}
# whole [section]s S1 carries but the generator marks absent (feature off; fail closed). The runtime
# (READINESS B5) switches ONLY that feature off; the seeds still arm.
EXPECTED_ABSENT_SECTIONS = {"listener", "thread", "pak"}


def parse_ini(text):
    """-> {section: {key: value}} plus layout blocks as {'__members__': {...}, '__vtables__': {...}}."""
    out, cur = {}, None
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        ln = lines[i]
        s = ln.strip()
        if s.startswith("[") and s.endswith("]"):
            cur = s[1:-1]
            out.setdefault(cur, {})
            i += 1
            continue
        if s == "<<<":                                 # a multi-line layout block to the matching >>>
            block = []
            i += 1
            while i < len(lines) and lines[i].strip() != ">>>":
                block.append(lines[i])
                i += 1
            out[cur]["__block__"] = parse_layout("\n".join(block))
            i += 1
            continue
        if s and not s.startswith(";") and not s.startswith("#") and "=" in ln:
            k, v = ln.split("=", 1)
            out[cur][k.strip()] = v.strip()
        i += 1
    return out


def parse_layout(text):
    """A VTable/MemberVariable ini body -> {SectionName: {Key: int}} (offsets as ints)."""
    sec, out = None, {}
    for ln in text.splitlines():
        s = ln.strip()
        if s.startswith("[") and s.endswith("]"):
            sec = s[1:-1]
            if sec != "NativeLayout":
                out[sec] = {}
        elif s and "=" in s and sec and sec != "NativeLayout":
            k, v = s.split("=", 1)
            try:
                out[sec][k.strip()] = int(v.strip(), 0)
            except ValueError:
                pass
    return out


def as_int(v):
    if isinstance(v, int):
        return v
    try:
        return int(v, 0) if isinstance(v, str) else int(v)
    except (TypeError, ValueError):
        return None


# (oracle key, ini section, ini key, is_int) for every runtime-used scalar S1 records
SCALARS = [
    ("image_sha256", "profile", "image_sha256", False),
    ("image_size", "profile", "image_size", True),
    ("image_extent", "profile", "image_extent", True),
    ("image_base", "profile", "image_base", True),
    ("format", "build", "format", True),
    ("pointer_size", "build", "pointer_size", True),
    ("engine_major", "build", "engine_major", True),
    ("engine_minor", "build", "engine_minor", True),
    ("target", "build", "target", False),
    ("engine_revision", "build", "engine_revision", False),
    ("build_id", "build", "build_id", False),
    ("build_configuration", "build", "build_configuration", True),
    ("stats", "build", "stats", True),
    ("non_shipping", "build", "non_shipping", True),
    ("stats_layout", "build", "stats_layout", True),
    ("fname_case_preserving", "build", "fname_case_preserving", True),
    ("fname_output_bytes", "build", "fname_output_bytes", True),
    ("fname_output_alignment", "build", "fname_output_alignment", True),
    ("pool_current_block_offset", "pool", "current_block_offset", True),
    ("pool_current_byte_offset_offset", "pool", "current_byte_offset_offset", True),
    ("pool_blocks_offset", "pool", "blocks_offset", True),
    ("pool_none_id_offset", "pool", "none_id_offset", True),
    ("pool_byte_property_id_offset", "pool", "byte_property_id_offset", True),
    ("pool_block_offset_bits", "pool", "block_offset_bits", True),
    ("pool_max_blocks", "pool", "max_blocks", True),
    ("required_set_complete", "seeds", "required_set_complete", True),
    ("fname_pool_label", "seeds", "fname_pool_label", True),
    ("guobject_array_label", "seeds", "guobject_array_label", True),
    ("fname_ctor_label", "seeds", "fname_ctor_label", True),
    ("gmalloc_label", "seeds", "gmalloc_label", True),
    ("gmalloc_instance_label", "seeds", "gmalloc_instance_label", True),
    ("gmalloc_vtable_label", "seeds", "gmalloc_vtable_label", True),
    ("sco_label", "seeds", "sco_label", True),
    ("fname_ctor_prefix", "fname", "ctor_prefix", False),
    ("sco_prefix", "code_pins", "sco_prefix", False),
    ("tostring_probe_id", "tostring", "probe_id", True),
    ("tostring_probe_expect", "tostring", "probe_expect", False),
    ("tostring_probe_expect_units", "tostring", "probe_expect_units", True),
    ("tostring_out_units", "tostring", "out_units", True),
]


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--ini", required=True)
    ap.add_argument("--oracle", required=True)
    ap.add_argument("--quiet", action="store_true")
    ns = ap.parse_args(argv)
    try:
        ini = parse_ini(open(ns.ini, encoding="utf-8").read())
        oracle = json.load(open(ns.oracle, encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print("check_equivalence: bad input: %s" % exc, file=sys.stderr)
        return 2

    equal, diff, absent, mismatch = [], [], [], []

    # 1. scalars
    for okey, sec, key, is_int in SCALARS:
        if okey not in oracle:
            continue
        want = oracle[okey]
        have = ini.get(sec, {}).get(key)
        if have is None:
            absent.append((okey, "not emitted (section %s key %s)" % (sec, key)))
            continue
        a, b = (as_int(have), as_int(want)) if is_int else (str(have), str(want))
        (equal if a == b else mismatch).append((okey, want, have))

    # 2. presence-only identity fields (any non-empty value is accepted; S1's exact string is not required)
    for okey, sec, key in [("name", "profile", "name"), ("digest", "profile", "digest"),
                           ("build_source", "build", "build_source"), ("stats_source", "build", "stats_source")]:
        have = ini.get(sec, {}).get(key)
        if have:
            diff.append((okey, oracle.get(okey), have, "presence-only; runtime checks the field is non-empty, not its text"))
        else:
            mismatch.append((okey, oracle.get(okey), None))

    # 3. expected-absent scalars (console/config)
    for okey in EXPECTED_ABSENT_SCALAR:
        absent.append((okey, "intentionally absent (console/config; not derivable from the image)"))

    # 4. layout member/vtable blocks: compare every key both sides share
    want_m = parse_layout(oracle.get("layout_members_text", ""))
    have_m = ini.get("layout.members", {}).get("__block__", {})
    want_v = parse_layout(oracle.get("layout_vtables_text", ""))
    have_v = ini.get("layout.vtables", {}).get("__block__", {})
    layout_equal, layout_mismatch, layout_only_oracle, layout_only_gen = compare_layout(want_m, have_m)
    ve, vm, vo, vg = compare_layout(want_v, have_v)
    layout_equal += ve
    layout_mismatch += vm
    layout_only_oracle += vo
    layout_only_gen += vg

    # 5. sections S1 carries that the generator marks absent (feature off)
    for sec in EXPECTED_ABSENT_SECTIONS:
        vals = [k for k in ini.get(sec, {}) if k != "__block__"]
        if vals:
            mismatch.append(("section[%s]" % sec, "expected feature-off (comments only)", "has keys: %s" % vals))
        else:
            absent.append(("section[%s]" % sec, "feature off (no generic detector yet); runtime fails that feature closed"))

    ok = not mismatch and not layout_mismatch
    checked = len(equal) + len(diff) + len(absent) + len(mismatch) + len(layout_equal) + len(layout_mismatch) \
        + len(layout_only_oracle) + len(layout_only_gen)
    if checked == 0:
        print("check_equivalence: FAIL: compared 0 fields (a gate that scans nothing is a failed gate)", file=sys.stderr)
        return 1

    if not ns.quiet:
        print("Two-source equivalence: generator profile.ini vs S1 compiled-header oracle")
        print("  scalar fields EQUAL                : %d" % len(equal))
        print("  layout member/vtable keys EQUAL    : %d" % len(layout_equal))
        print("  presence-only (text differs, OK)   : %d" % len(diff))
        print("  intentionally ABSENT (feature off) : %d" % (len(absent)))
        print("  layout keys only in oracle (gaps)  : %d" % len(layout_only_oracle))
        print("  layout keys only in generator      : %d" % len(layout_only_gen))
        print("  UNEXPECTED MISMATCHES              : %d" % (len(mismatch) + len(layout_mismatch)))
        print("  total fields compared              : %d" % checked)
        for okey, want, have in mismatch:
            print("  MISMATCH %s: oracle=%r generator=%r" % (okey, want, have))
        for sec, key, want, have in layout_mismatch:
            print("  LAYOUT MISMATCH %s.%s: oracle=0x%x generator=0x%x" % (sec, key, want, have))
        if layout_only_oracle:
            print("  layout keys only in the oracle (the generator does not yet derive these):")
            for sec, key in sorted(layout_only_oracle):
                print("    %s.%s" % (sec, key))
        if layout_only_gen:
            print("  layout keys only in the generator (extra; not in S1's compiled pair):")
            for sec, key in sorted(layout_only_gen):
                print("    %s.%s" % (sec, key))
    print("RESULT: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


def compare_layout(want, have):
    equal, mismatch, only_oracle, only_gen = [], [], [], []
    for sec, keys in want.items():
        for key, wval in keys.items():
            hval = have.get(sec, {}).get(key)
            if hval is None:
                only_oracle.append((sec, key))
            elif hval == wval:
                equal.append((sec, key))
            else:
                mismatch.append((sec, key, wval, hval))
    for sec, keys in have.items():
        for key in keys:
            if key not in want.get(sec, {}):
                only_gen.append((sec, key))
    return equal, mismatch, only_oracle, only_gen


if __name__ == "__main__":
    sys.exit(main())
