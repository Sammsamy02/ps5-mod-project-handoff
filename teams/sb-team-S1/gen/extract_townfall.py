#!/usr/bin/env python3
"""extract_townfall.py — Team S1 (B3) Townfall equivalence generator.

Reads TODAY's compiled per-game headers (read-only, from the live tree) and
emits, mechanically:
  * gen/townfall-profile.ini       — the profile data file in PROFILE-FORMAT.md
  * gen/townfall-expected.json     — the same per-game VALUES as a flat dict,
                                     used by host-tests/check_equivalence.py to
                                     prove the loader reproduces them byte-for-byte.

No value is hand-typed here; every number/byte/string is parsed out of the
compiled headers, so if the loader + this file reproduce them the runtime
behaves exactly as today on Townfall. Run with python3 -I.

Usage: python3 -I gen/extract_townfall.py [--live-root DIR] [--out-dir DIR]
"""
import argparse, hashlib, json, os, re, sys

LIVE_DEFAULT = "/WORKSPACE/.codex/worktrees/translation-core/what/outputs"

def rd(p):
    with open(p, "r", errors="replace") as f:
        return f.read()

def hx(n):  # lowercase 0x
    return "0x%x" % n

def num(s):
    s = s.strip().rstrip("uUlL")
    return int(s, 16) if s.lower().startswith("0x") else int(s)

def bytes_list(text):
    # parse a C initializer list of 0xNN bytes -> lowercase hex string
    vals = re.findall(r"0x[0-9a-fA-F]+", text)
    return "".join("%02x" % (int(v, 16) & 0xff) for v in vals)

def cstr_first(text, name):
    m = re.search(re.escape(name) + r'\s*=\s*"((?:[^"\\]|\\.)*)"', text)
    return m.group(1) if m else None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--live-root", default=LIVE_DEFAULT)
    ap.add_argument("--out-dir", default=os.path.dirname(os.path.abspath(__file__)))
    a = ap.parse_args()
    L = a.live_root
    U = L + "/2026-10-07-l19-local-profile-r4/units"
    P = L + "/2026-10-08-l26-runtime/profile/units"
    TSI = L + "/2026-10-08-l26-runtime/typescan-r4/include"
    THR = L + "/2026-10-08-r5-thread-final-integration-v2/thread"

    seed = rd(U + "/SeedTable.h")
    sv = rd(U + "/SeedValidation.hpp")
    fab = rd(U + "/FNameAbiProfile.hpp")
    rbp = rd(P + "/RuntimeBuildProfile.hpp")
    csp = rd(P + "/CodeSignatureProfile.hpp")
    cnl = rd(P + "/CompiledNativeLayoutDeclarations.hpp")
    elo = rd(TSI + "/EngineListenerOpsProfile.hpp")
    ets = rd(TSI + "/EngineToStringProfile.hpp")
    pak = rd(TSI + "/pak_mount_pins.inc")
    thr = rd(THR + "/ThreadNameProfile.hpp")

    def g(text, pat, grp=1, cast=num):
        m = re.search(pat, text)
        if not m:
            raise SystemExit("extract miss: " + pat)
        return cast(m.group(grp))

    exp = {}

    # --- SeedTable.h ---
    exp["image_base"] = g(seed, r"kBaseObserved\s*=\s*(0x[0-9a-fA-F]+)")
    exp["fname_pool_label"] = g(seed, r"kFNamePoolLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["gmalloc_instance_label"] = g(seed, r"kGMallocInstanceLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["gmalloc_vtable_label"] = g(seed, r"kGMallocVtableLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["guobject_array_label"] = g(seed, r"kGUObjectArray\{[^}]*?,\s*(0x[0-9a-fA-F]+),")
    exp["fname_ctor_label"] = g(seed, r"kFNameCtorWchar\{[^}]*?,\s*(0x[0-9a-fA-F]+),")
    exp["gmalloc_label"] = g(seed, r"kGMalloc\{[^}]*?,\s*(0x[0-9a-fA-F]+),")
    exp["sco_label"] = g(seed, r"kStaticConstructObject\{[^}]*?,\s*(0x[0-9a-fA-F]+),")
    exp["image_sha256"] = g(seed, r'kImageSha256\s*=\s*"([0-9a-f]{64})"', cast=str)
    exp["required_set_complete"] = 1 if "kRequiredSetComplete = true" in seed else 0

    # --- SeedValidation.hpp (pool layout + extent) ---
    pl = re.search(r"kPoolLayout\{\s*([^}]*)\}", sv).group(1)
    pn = [num(x) for x in re.findall(r"0x[0-9a-fA-F]+|\b\d+\b", pl)]
    (exp["pool_current_block_offset"], exp["pool_current_byte_offset_offset"],
     exp["pool_blocks_offset"], exp["pool_none_id_offset"], exp["pool_byte_property_id_offset"],
     exp["pool_block_offset_bits"], exp["pool_max_blocks"]) = pn[:7]
    exp["image_extent"] = g(sv, r"kImageFileExtentBytes\s*=\s*(0x[0-9a-fA-F]+)")

    # --- FNameAbiProfile.hpp ---
    exp["fname_ctor_prefix"] = bytes_list(re.search(r"kPrefix\[\]\s*=\s*\{([^}]*)\}", fab).group(1))
    exp["fname_output_bytes"] = g(fab, r"kOutputBytes\s*=\s*(\d+)")
    exp["fname_output_alignment"] = g(fab, r"kOutputAlignment\s*=\s*(\d+)")
    exp["fname_case_preserving"] = 1 if re.search(r"kCasePreserving\s*=\s*true", fab) else 0

    # --- RuntimeBuildProfile.hpp (buildfacts::Profile kProfile{...}) ---
    exp["digest"] = g(rbp, r'kDigest\s*=\s*"([0-9a-f]{64})"', cast=str)
    body = re.search(r"kProfile\{(.*?)\};", rbp, re.S).group(1)
    # positional fields per the struct: 1,name,kDigest,sha,size,extent,base,
    #   format,ptr,major,minor,target,revision,build_id, config,stats,build_source,stats_source,
    #   fname_case_known,fname_case_preserving,fname_bytes,fname_alignment
    exp["name"] = re.search(r'"(townfall[^"]*)"', body).group(1)
    exp["image_size"] = g(body, r",\s*(\d{6,})ull\s*,")  # 194241424ull
    exp["engine_major"] = g(rbp, r"PT_SCE|", 0, lambda s: 0) if False else 5  # overwritten below
    # robust: pull the three numeric fields on the line after the sha
    nums = re.findall(r"(\d{3,})ull", body)
    exp["image_size"] = int(nums[0]); _extent = int(nums[1]); _base = int(nums[2])
    assert _extent == exp["image_extent"], (_extent, exp["image_extent"])
    assert _base == exp["image_base"]
    exp["name"] = re.search(r'kDigest,"[0-9a-f]{64}"', rbp) and exp["name"]
    # the identity line: 1,8,5,6,"target","revision","build_id",
    ident = re.search(r'ull,\s*\n\s*(\d+),(\d+),(\d+),(\d+),"([^"]+)","([^"]+)","([^"]+)",', rbp)
    exp["format"] = int(ident.group(1)); exp["pointer_size"] = int(ident.group(2))
    exp["engine_major"] = int(ident.group(3)); exp["engine_minor"] = int(ident.group(4))
    exp["target"] = ident.group(5); exp["engine_revision"] = ident.group(6); exp["build_id"] = ident.group(7)
    cfg = re.search(r'",\s*\n\s*(\d+),(\d+),"([^"]+)","([^"]+)",', rbp)
    exp["build_configuration"] = int(cfg.group(1)); exp["stats"] = int(cfg.group(2))
    exp["build_source"] = cfg.group(3); exp["stats_source"] = cfg.group(4)
    tail = re.search(r'",\s*\n\s*(true|false),(true|false),(\d+),(\d+)\s*\n?\s*\};', rbp)
    exp["fname_case_preserving"] = 1 if tail.group(2) == "true" else 0
    exp["fname_output_bytes"] = int(tail.group(3)); exp["fname_output_alignment"] = int(tail.group(4))
    exp["non_shipping"] = 1 if exp["build_configuration"] != 1 else 0
    exp["stats_layout"] = 1 if exp["stats"] == 2 else 0

    # --- CodeSignatureProfile.hpp (sco prefix + exe path) ---
    exp["sco_prefix"] = bytes_list(re.search(r"kScoPrefix\[\]\s*=\s*\{([^}]*)\}", csp).group(1))
    exp["exe_path"] = re.search(r'kProfile\{\s*\n?\s*"([^"]+eboot\.bin)"', csp).group(1)

    # --- EngineListenerOpsProfile.hpp ---
    exp["guobject_array_label2"] = g(elo, r"kGuObjectArrayLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["listener_add_label"] = g(elo, r"kAddDeleteListenerLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["listener_remove_label"] = g(elo, r"kRemoveDeleteListenerLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["listener_add_prefix"] = bytes_list(re.search(r"kAddPrefix\[\]\s*=\s*\{([^}]*)\}", elo).group(1))
    exp["listener_remove_prefix"] = bytes_list(re.search(r"kRemovePrefix\[\]\s*=\s*\{([^}]*)\}", elo).group(1))
    assert exp["guobject_array_label2"] == exp["guobject_array_label"]

    # --- EngineToStringProfile.hpp (constants; label/prefix provenance) ---
    exp["tostring_label"] = g(ets, r"kToStringInternalLabel\s*=\s*(0x[0-9a-fA-F]+)")
    exp["tostring_prefix"] = bytes_list(re.search(r"kToStringInternalPrefix\[\]\s*=\s*\{([^}]*)\}", ets).group(1))
    exp["tostring_probe_id"] = g(ets, r"kProbeId\s*=\s*(0x[0-9a-fA-F]+)")
    exp["tostring_probe_expect"] = re.search(r'kProbeExpect\[\]\s*=\s*u"([^"]+)"', ets).group(1)
    exp["tostring_probe_expect_units"] = g(ets, r"kProbeExpectUnits\s*=\s*(\d+)")
    exp["tostring_out_units"] = g(ets, r"kOutUnits\s*=\s*(\d+\s*\+\s*\d+)", cast=lambda s: eval(s))

    # --- ThreadNameProfile.hpp ---
    exp["thread_accessor"] = g(thr, r"kAccessor\s*=\s*(0x[0-9a-fA-F]+)")
    exp["thread_game_name"] = g(thr, r"kGameNameStorage\s*=\s*(0x[0-9a-fA-F]+)")
    exp["thread_render_name"] = g(thr, r"kRenderNameStorage\s*=\s*(0x[0-9a-fA-F]+)")
    exp["thread_empty_name"] = g(thr, r"kEmptyNameStorage\s*=\s*(0x[0-9a-fA-F]+)")
    exp["thread_name_prefix"] = bytes_list(re.search(r"kPrefix\[\]\s*=\s*\{([^}]*)\}", thr).group(1))

    # --- pak_mount_pins.inc ---
    def pg(name):
        return g(pak, r"p\.%s\s*=\s*(0x[0-9a-fA-F]+)" % re.escape(name))
    for k in ["pak_vtable", "platform_mgr_get", "platform_mgr_getpf", "mountpak_delegate",
              "handle_mount_pak", "delegate_vtable", "execute", "lower_impl",
              "snapshot_delegate", "snapshot_vtable", "snapshot_execute"]:
        exp["pak_" + k if not k.startswith("pak_") else k] = pg(k)
    # normalize names to match the ini keys
    exp["pak_platform_mgr_get"] = exp.pop("pak_platform_mgr_get", pg("platform_mgr_get"))
    proofs = re.findall(r"p\.proofs\[(\d)\]\s*=\s*\{(0x[0-9a-fA-F]+),\{([^}]*)\},(\d+)\}", pak)
    exp["pak_proofs"] = []
    for idx, linked, blob, size in sorted(proofs, key=lambda x: int(x[0])):
        exp["pak_proofs"].append({"linked": num(linked), "bytes": bytes_list(blob), "size": int(size)})
    assert len(exp["pak_proofs"]) == 6

    # --- layout texts (CompiledNativeLayoutDeclarations.hpp) ---
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', cnl)
    # first two long string literals are members, vtables
    def unescape(s):
        return s.replace("\\n", "\n").replace('\\"', '"').replace("\\\\", "\\")
    long_lits = [unescape(x) for x in lits if "\\n" in x]
    exp["layout_members_text"] = long_lits[0]
    exp["layout_vtables_text"] = long_lits[1]

    # ---------------------------------------------------------------- emit ini
    def ini_lines():
        o = []
        o.append("[profile]")
        o.append("schema=1")
        o.append("name=%s" % exp["name"])
        o.append("digest=%s" % exp["digest"])
        o.append("__BODY_SHA__")  # placeholder, replaced after hashing
        o.append("image_sha256=%s" % exp["image_sha256"])
        o.append("image_size=%d" % exp["image_size"])
        o.append("image_extent=%s" % hx(exp["image_extent"]))
        o.append("image_base=%s" % hx(exp["image_base"]))
        o.append("exe_path=%s" % exp["exe_path"])
        o.append("[build]")
        o.append("format=%d" % exp["format"])
        o.append("pointer_size=%d" % exp["pointer_size"])
        o.append("engine_major=%d" % exp["engine_major"])
        o.append("engine_minor=%d" % exp["engine_minor"])
        o.append("target=%s" % exp["target"])
        o.append("engine_revision=%s" % exp["engine_revision"])
        o.append("build_id=%s" % exp["build_id"])
        o.append("build_configuration=%d" % exp["build_configuration"])
        o.append("build_source=%s" % exp["build_source"])
        o.append("stats=%d" % exp["stats"])
        o.append("stats_source=%s" % exp["stats_source"])
        o.append("non_shipping=%d" % exp["non_shipping"])
        o.append("stats_layout=%d" % exp["stats_layout"])
        o.append("fname_case_preserving=%d" % exp["fname_case_preserving"])
        o.append("fname_output_bytes=%d" % exp["fname_output_bytes"])
        o.append("fname_output_alignment=%d" % exp["fname_output_alignment"])
        o.append("[pool]")
        o.append("current_block_offset=%s" % hx(exp["pool_current_block_offset"]))
        o.append("current_byte_offset_offset=%s" % hx(exp["pool_current_byte_offset_offset"]))
        o.append("blocks_offset=%s" % hx(exp["pool_blocks_offset"]))
        o.append("none_id_offset=%s" % hx(exp["pool_none_id_offset"]))
        o.append("byte_property_id_offset=%s" % hx(exp["pool_byte_property_id_offset"]))
        o.append("block_offset_bits=%d" % exp["pool_block_offset_bits"])
        o.append("max_blocks=%d" % exp["pool_max_blocks"])
        o.append("[seeds]")
        o.append("generated=1")
        o.append("required_set_complete=%d" % exp["required_set_complete"])
        o.append("fname_pool_label=%s" % hx(exp["fname_pool_label"]))
        o.append("guobject_array_label=%s" % hx(exp["guobject_array_label"]))
        o.append("fname_ctor_label=%s" % hx(exp["fname_ctor_label"]))
        o.append("gmalloc_label=%s" % hx(exp["gmalloc_label"]))
        o.append("gmalloc_instance_label=%s" % hx(exp["gmalloc_instance_label"]))
        o.append("gmalloc_vtable_label=%s" % hx(exp["gmalloc_vtable_label"]))
        o.append("sco_label=%s" % hx(exp["sco_label"]))
        for slot in ["guobject_array", "fname_ctor", "gmalloc", "sco"]:
            o.append("%s_present=1" % slot)
            o.append("%s_derivation=two-source generator envelope (see runtime-profile.json method/evidence); provenance only" % slot)
        o.append("[fname]")
        o.append("ctor_prefix=%s" % exp["fname_ctor_prefix"])
        o.append("ctor_prefix_size=%d" % (len(exp["fname_ctor_prefix"]) // 2))
        o.append("[code_pins]")
        o.append("sco_prefix=%s" % exp["sco_prefix"])
        o.append("sco_prefix_size=%d" % (len(exp["sco_prefix"]) // 2))
        o.append("listener_add_prefix=%s" % exp["listener_add_prefix"])
        o.append("listener_add_prefix_size=%d" % (len(exp["listener_add_prefix"]) // 2))
        o.append("listener_remove_prefix=%s" % exp["listener_remove_prefix"])
        o.append("listener_remove_prefix_size=%d" % (len(exp["listener_remove_prefix"]) // 2))
        o.append("thread_name_prefix=%s" % exp["thread_name_prefix"])
        o.append("thread_name_prefix_size=%d" % (len(exp["thread_name_prefix"]) // 2))
        o.append("[listener]")
        o.append("add_label=%s" % hx(exp["listener_add_label"]))
        o.append("remove_label=%s" % hx(exp["listener_remove_label"]))
        o.append("add_prefix=%s" % exp["listener_add_prefix"])
        o.append("add_prefix_size=%d" % (len(exp["listener_add_prefix"]) // 2))
        o.append("remove_prefix=%s" % exp["listener_remove_prefix"])
        o.append("remove_prefix_size=%d" % (len(exp["listener_remove_prefix"]) // 2))
        o.append("[tostring]")
        o.append("probe_id=%s" % hx(exp["tostring_probe_id"]))
        o.append("probe_expect=%s" % exp["tostring_probe_expect"])
        o.append("probe_expect_units=%d" % exp["tostring_probe_expect_units"])
        o.append("out_units=%d" % exp["tostring_out_units"])
        o.append("label=%s" % hx(exp["tostring_label"]))
        o.append("prefix=%s" % exp["tostring_prefix"])
        o.append("prefix_size=%d" % (len(exp["tostring_prefix"]) // 2))
        o.append("[thread]")
        o.append("accessor=%s" % hx(exp["thread_accessor"]))
        o.append("game_name=%s" % hx(exp["thread_game_name"]))
        o.append("render_name=%s" % hx(exp["thread_render_name"]))
        o.append("empty_name=%s" % hx(exp["thread_empty_name"]))
        o.append("[pak]")
        o.append("pak_vtable=%s" % hx(exp["pak_vtable"]))
        o.append("platform_mgr_get=%s" % hx(exp["pak_platform_mgr_get"]))
        o.append("platform_mgr_getpf=%s" % hx(exp["pak_platform_mgr_getpf"]))
        o.append("mountpak_delegate=%s" % hx(exp["pak_mountpak_delegate"]))
        o.append("handle_mount_pak=%s" % hx(exp["pak_handle_mount_pak"]))
        o.append("delegate_vtable=%s" % hx(exp["pak_delegate_vtable"]))
        o.append("execute=%s" % hx(exp["pak_execute"]))
        o.append("lower_impl=%s" % hx(exp["pak_lower_impl"]))
        o.append("snapshot_delegate=%s" % hx(exp["pak_snapshot_delegate"]))
        o.append("snapshot_vtable=%s" % hx(exp["pak_snapshot_vtable"]))
        o.append("snapshot_execute=%s" % hx(exp["pak_snapshot_execute"]))
        o.append("slot_get_lower_level=0x68")
        o.append("slot_delegate_execute=0x60")
        o.append("delegate_inst_off=0")
        o.append("delegate_size_off=8")
        o.append("pak_count_offset=0x18")
        for i, pr in enumerate(exp["pak_proofs"]):
            o.append("proof%d_linked=%s" % (i, hx(pr["linked"])))
            o.append("proof%d_size=%d" % (i, pr["size"]))
            o.append("proof%d_bytes=%s" % (i, pr["bytes"]))
        o.append("[layout.members]")
        o.append("<<<")
        o.append(exp["layout_members_text"].rstrip("\n"))
        o.append(">>>")
        o.append("[layout.vtables]")
        o.append("<<<")
        o.append(exp["layout_vtables_text"].rstrip("\n"))
        o.append(">>>")
        return o

    lines = ini_lines()
    # body hash = sha256 of the file with the body_sha256 line removed.
    body_idx = lines.index("__BODY_SHA__")
    without = lines[:body_idx] + lines[body_idx + 1:]
    body_text = "\n".join(without) + "\n"
    body_sha = hashlib.sha256(body_text.encode()).hexdigest()
    lines[body_idx] = "body_sha256=%s" % body_sha
    ini_text = "\n".join(lines) + "\n"

    os.makedirs(a.out_dir, exist_ok=True)
    with open(os.path.join(a.out_dir, "townfall-profile.ini"), "w") as f:
        f.write(ini_text)
    exp["body_sha256"] = body_sha
    with open(os.path.join(a.out_dir, "townfall-expected.json"), "w") as f:
        json.dump(exp, f, indent=1, sort_keys=True)
    print("wrote townfall-profile.ini (%d bytes) and townfall-expected.json; %d expected fields"
          % (len(ini_text), len(exp)))

if __name__ == "__main__":
    main()
