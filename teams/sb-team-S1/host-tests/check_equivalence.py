#!/usr/bin/env python3
"""check_equivalence.py — Team S1 (B3) Townfall byte-for-byte equivalence proof.

Builds dump_profile, loads gen/townfall-profile.ini through the REAL loader,
and compares every behavioural per-game value to townfall-expected.json (the
values the generator extracted from TODAY's compiled headers). 100% equal =>
Townfall behaves exactly as today once the values come from the data file.

Provenance-only fields (slot derivations) are not behaviour and are excluded.
Prints the number of fields compared; exits nonzero on any mismatch or if
zero fields were compared. Run with python3 -I.
"""
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
GEN = os.path.join(HERE, "..", "gen")

def build_and_run():
    exe = "/tmp/s1_dump_equiv"
    cc = subprocess.run(["c++", "-std=c++23", "-I", os.path.join(HERE, "..", "new"),
                         os.path.join(HERE, "dump_profile.cpp"), "-o", exe],
                        capture_output=True, text=True)
    if cc.returncode != 0:
        print("COMPILE FAILED\n" + cc.stderr); sys.exit(2)
    run = subprocess.run([exe, os.path.join(GEN, "townfall-profile.ini")],
                         capture_output=True, text=True)
    if run.returncode != 0:
        print("LOAD FAILED\n" + run.stderr); sys.exit(1)
    out = {}
    for line in run.stdout.splitlines():
        if "=" in line:
            k, v = line.split("=", 1); out[k] = v
    return out, run.stderr

def as_int(v):
    v = v.strip()
    return int(v, 16) if v.lower().startswith("0x") else int(v)

def layout_lookup(text, klass, member):
    cur = None
    for raw in text.split("\n"):
        line = raw.strip()
        if not line or line[0] in ";#":
            continue
        if line.startswith("["):
            cur = line[1:line.index("]")] if "]" in line else None
            continue
        if cur == klass and "=" in line:
            k, val = line.split("=", 1)
            if k.strip() == member:
                return as_int(val)
    return None

def main():
    exp = json.load(open(os.path.join(GEN, "townfall-expected.json")))
    dump, stderr = build_and_run()
    print(stderr.strip())
    fails = []
    n = 0

    def cmp_int(key, dkey=None):
        nonlocal n
        dkey = dkey or key
        if key not in exp or dkey not in dump:
            fails.append("%s: missing (exp=%s dump=%s)" % (key, key in exp, dkey in dump)); return
        n += 1
        if as_int(str(exp[key])) != as_int(dump[dkey]):
            fails.append("%s: exp=%s dump=%s" % (key, exp[key], dump[dkey]))

    def cmp_str(key, dkey=None):
        nonlocal n
        dkey = dkey or key
        if key not in exp or dkey not in dump:
            fails.append("%s: missing" % key); return
        n += 1
        if str(exp[key]) != dump[dkey]:
            fails.append("%s: exp=%r dump=%r" % (key, exp[key], dump[dkey]))

    # strings
    for k in ["name", "digest", "body_sha256", "image_sha256", "exe_path", "target",
              "engine_revision", "build_id", "build_source", "stats_source",
              "fname_ctor_prefix", "sco_prefix", "listener_add_prefix",
              "listener_remove_prefix", "thread_name_prefix", "tostring_prefix",
              "tostring_probe_expect"]:
        cmp_str(k)
    # ints / hex
    for k in ["image_size", "image_extent", "image_base", "format", "pointer_size",
              "engine_major", "engine_minor", "build_configuration", "stats",
              "non_shipping", "stats_layout", "fname_case_preserving",
              "fname_output_bytes", "fname_output_alignment",
              "pool_current_block_offset", "pool_current_byte_offset_offset",
              "pool_blocks_offset", "pool_none_id_offset", "pool_byte_property_id_offset",
              "pool_block_offset_bits", "pool_max_blocks", "required_set_complete",
              "fname_pool_label", "guobject_array_label", "fname_ctor_label",
              "gmalloc_label", "gmalloc_instance_label", "gmalloc_vtable_label", "sco_label",
              "listener_add_label", "listener_remove_label", "tostring_label",
              "tostring_probe_id", "tostring_probe_expect_units", "tostring_out_units",
              "thread_accessor", "thread_game_name", "thread_render_name", "thread_empty_name",
              "pak_vtable", "pak_platform_mgr_get", "pak_platform_mgr_getpf",
              "pak_mountpak_delegate", "pak_handle_mount_pak", "pak_delegate_vtable",
              "pak_execute", "pak_lower_impl", "pak_snapshot_delegate",
              "pak_snapshot_vtable", "pak_snapshot_execute"]:
        cmp_int(k)
    # pak proofs
    for i, pr in enumerate(exp["pak_proofs"]):
        n += 1
        dl = dump.get("pak_proof%d_linked" % i)
        db = dump.get("pak_proof%d_bytes" % i)
        if dl is None or as_int(dl) != pr["linked"]:
            fails.append("pak_proof%d_linked: exp=%s dump=%s" % (i, hex(pr["linked"]), dl))
        if db != pr["bytes"]:
            fails.append("pak_proof%d_bytes: exp=%s dump=%s" % (i, pr["bytes"], db))

    # layout offsets: the loader's layout_offset() over the parsed text must
    # equal the compiled INI's own value for every member/vtable queried.
    for dk, dv in dump.items():
        m = re.match(r"layout\.(members|vtables)\.([^.]+)\.([^.]+)_val$", dk)
        if not m:
            continue
        which, klass, member = m.groups()
        text = exp["layout_members_text"] if which == "members" else exp["layout_vtables_text"]
        want = layout_lookup(text, klass, member)
        n += 1
        if dv == "MISS" or want is None or as_int(dv) != want:
            fails.append("layout.%s.%s.%s: exp=%s dump=%s" % (which, klass, member, hex(want) if want is not None else None, dv))

    print("fields compared: %d" % n)
    if n == 0:
        print("FAIL: zero fields compared"); sys.exit(2)
    if fails:
        print("MISMATCHES (%d):" % len(fails))
        for f in fails:
            print("  " + f)
        sys.exit(1)
    print("PASS: all %d behavioural fields identical to the compiled headers" % n)

if __name__ == "__main__":
    main()
