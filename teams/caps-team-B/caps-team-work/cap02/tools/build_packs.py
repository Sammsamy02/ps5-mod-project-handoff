#!/usr/bin/env python3
"""Build the engine-level layout packs for the cap02 automatic profile generator.

Mac-only. Reads ONLY existing local evidence (read-only):
  * the R11 shipped layout pair (extracted from the R11 lineage header; sha-pinned)
  * the R11 RuntimeBuildProfile pins (image/static evidence citations)
  * l23 generic-compiler measurements of Epic's own 5.6.1 declarations (TABLE.json)
  * round-10 FIELD-EVIDENCE.json (image measurements of the 15 added fields)
  * UE4SS upstream (Re-UE4SS/UEPseudo) per-version default layouts (Win64 builds):
    corroboration only, never counted as one of the two sources (same policy as
    round-10 FIELDS.md), because its ABI is MSVC, not the PS5 Itanium ABI.

Writes:
  packs/src/<pack>.json                      per-row values + every source seen
  PATCH/new-files/AutoLayoutPacks.generated.hpp   ONLY packs whose every row has
                                             >= 2 independent agreeing sources or
                                             is an already-shipped, console-proven
                                             R11 row (legacy admission, listed)
  evidence/PACK-SOURCES.json                 per-row source table + counts

No game names are written into the generated header (checked by tests).
Exit non-zero if any count is zero or a gate fails.
"""
import hashlib, json, re, sys
from pathlib import Path

CAP = Path(__file__).resolve().parents[1]
WHAT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
OUT = WHAT / "outputs"
# Stable source copy (same sha as the R11 lineage copy in l26-package-A, which
# is replaced at the next build). The sha pins below make any drift fail loudly.
R11_DECL = OUT / "2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp"
R11_PROFILE = OUT / "2026-10-08-l26-runtime/profile/units/RuntimeBuildProfile.hpp"
L23_TABLE = OUT / "2026-10-08-l23-layout-generic/TABLE.json"
L23_SIZES = OUT / "2026-10-08-l23-layout-generic/CLASS-SIZES.json"
FIELD_EV = OUT / "2026-10-08-l25-prep/round10-staging/fields/evidence/FIELD-EVIDENCE.json"
UEPSEUDO = WHAT / ".superpowers/native-dependency-spike/ue4ss-unreal-source-access-1/upstream/generated_include/FunctionBodies"

R11_DECL_SHA = "ab8750c5a7aabfeb6692140ecb5b4bae8e74659f40f0f6ab9be4ddd9f6abec9e"
R11_PROFILE_SHA = "09a14712c972ffe1a1e6967c5bc86eb88d918ebbc898727b12f4804c6d2af3aa"
GENERATOR_VERSION = "cap02-autoprofile-2"
# Names that must never appear in shipped generated data (game identities).
FORBIDDEN = re.compile(r"(?i)townfall|silent ?hill|ppsa\d+|stellar|blade|PPSA")


def sha256(b):
    return hashlib.sha256(b).hexdigest()


def r11_texts():
    raw = R11_DECL.read_bytes()
    if sha256(raw) != R11_DECL_SHA:
        raise SystemExit(f"R11 declaration header drifted: {sha256(raw)}")
    lits = re.findall(r'^"((?:[^"\\]|\\.)*)",?$', raw.decode(), re.M)
    if len(lits) != 4:
        raise SystemExit("unexpected R11 declaration shape")
    un = lambda s: s.encode().decode("unicode_escape")
    return un(lits[0]), un(lits[1])


def parse_ini(text):
    """-> header dict, ordered [(section, [(key, value_int)])]."""
    header, sections, cur = {}, [], None
    for ln in text.split("\n"):
        if not ln:
            continue
        m = re.fullmatch(r"\[(.+)\]", ln)
        if m:
            cur = (m[1], [])
            if m[1] != "NativeLayout":
                sections.append(cur)
            continue
        k, v = ln.split("=", 1)
        if cur[0] == "NativeLayout":
            header[k] = v
        else:
            cur[1].append((k, int(v, 16)))
    return header, sections


def r11_pins():
    raw = R11_PROFILE.read_bytes()
    if sha256(raw) != R11_PROFILE_SHA:
        raise SystemExit("R11 runtime profile drifted")
    pins = {}
    for name, off, src in re.findall(r'\{"([^"]+)",true,(\d+),\d+,"((?:[^"\\]|\\.)*)"\}', raw.decode()):
        pins[name] = (int(off), src)
    return pins


def l23_values():
    vals = {}
    for r in json.loads(L23_TABLE.read_text()):
        if r.get("offset") is not None:
            vals[r["name"].replace("UObject::ProcessEvent", "UObject::ProcessEvent")] = int(r["offset"])
    try:
        sizes = json.loads(L23_SIZES.read_text())
        items = sizes.items() if isinstance(sizes, dict) else []
        for k, v in items:
            if isinstance(v, dict) and isinstance(v.get("size"), int):
                vals.setdefault(f"{k}::UEP_TotalSize", v["size"])
            elif isinstance(v, int):
                vals.setdefault(f"{k}::UEP_TotalSize", v)
    except Exception:
        pass
    return vals


def field_evidence():
    d = json.loads(FIELD_EV.read_text())
    return {k: int(v["offset"], 16) for k, v in d["facts"].items() if v.get("offset")}


def uepseudo(version):
    vals = {}
    for p in sorted(UEPSEUDO.glob(f"{version}_MemberVariableLayout_DefaultSetter_*.cpp")):
        if "CasePreserving" in p.name:
            continue
        cls = p.name.split("DefaultSetter_", 1)[1][:-4].replace("__", "::")
        for key, val in re.findall(r'MemberOffsets\.emplace\(STR\("(\w+)"\), (0x[0-9A-Fa-f]+)\)', p.read_text()):
            vals[f"{cls}::{key}"] = int(val, 16)
    return vals


def build_56(members_text, vtables_text):
    mh, msec = parse_ini(members_text)
    vh, vsec = parse_ini(vtables_text)
    if mh != vh:
        raise SystemExit("R11 pair identity headers differ")
    pins, l23, fev, uep = r11_pins(), l23_values(), field_evidence(), uepseudo("5_06")
    table, legacy = [], []

    def row(kind, sec, key, val):
        name = f"{sec}::{key}"
        srcs = {}
        if name in pins:
            srcs["image_static_pin"] = {"value": pins[name][0], "cite": pins[name][1][:240]}
        if name in l23:
            srcs["epic_5_6_1_generic_compiler"] = {"value": l23[name]}
        if name in fev:
            srcs["image_field_evidence"] = {"value": fev[name]}
        corro = {"ue4ss_upstream_5_06_win64": uep[name]} if name in uep else {}
        agree = [k for k, s in srcs.items() if s["value"] == val]
        disagree = [k for k, s in srcs.items() if s["value"] != val]
        r = {"kind": kind, "name": name, "value": val, "sources": srcs, "agreeing": agree,
             "disagreeing": disagree, "corroboration_only": corro,
             "status": "TWO-SOURCE" if len(agree) >= 2 else "R11-LEGACY-ADMITTED"}
        if r["status"] != "TWO-SOURCE":
            legacy.append(name)
        table.append(r)
        return r

    for sec, rows in msec:
        for k, v in rows:
            row("member", sec, k, v)
    for sec, rows in vsec:
        for k, v in rows:
            row("vtable", sec, k, v)
    pack = {
        "id": "ue5.6-prospero-x86_64-sysv-shipping-statsoff",
        "engine_major": int(mh["EngineMajor"]), "engine_minor": int(mh["EngineMinor"]),
        "target": mh["Target"], "pointer_size": int(mh["PointerSize"]), "format": int(mh["Format"]),
        "non_shipping": mh["NonShipping"] == "1", "stats": mh["Stats"] == "1",
        "fname": {"case_preserving": False, "bytes": 8, "alignment": 8,
                  "source": "R11 FNameAbiProfile (authenticated) + shipping build config (no editor-only data)"},
        "members": [[s, [[k, v] for k, v in rows]] for s, rows in msec],
        "vtables": [[s, [[k, v] for k, v in rows]] for s, rows in vsec],
        "admission": "engine-level pack calibrated on one PS5 game image; every value is an R11 shipped, "
                     "launch-14 console-proven value. Rows listed in legacy_rows lack a second mechanical "
                     "source in this tree and were admitted by the R11 lineage review.",
        "legacy_rows": legacy,
        "platform_dependent_rows": [n for n in (r["name"] for r in table)
                                    if n.startswith("FUObjectArray::") and n.split("::")[1] in
                                    ("UObjectCreateListeners", "UObjectDeleteListeners", "PrimarySerialNumber",
                                     "bShouldRecycleObjectIndices", "UEP_TotalSize")],
        "config_dependent_rows": ["UObject::ProcessEvent (UE_WITH_IRIS=1 -> 0x268; =0 -> 0x260)"],
    }
    # Arming clearance (review F5): a pack may arm on an image only when every
    # row is two-source AND the cell models every config axis a row depends on.
    # The cell has no IRIS axis yet, so any config-dependent row also blocks.
    pack["single_source_rows"] = len(legacy)
    pack["arming_cleared"] = (not legacy) and (not pack["config_dependent_rows"])
    return pack, table


def build_426_candidate():
    uep = uepseudo("4_26")
    rows = [{"name": k, "ue4ss_upstream_4_26_win64": v, "itanium_sources": 0,
             "status": "NOT-SHIPPED: single MSVC-ABI source; needs Epic 4.26 Itanium measurement"}
            for k, v in sorted(uep.items())]
    return {"id": "ue4.26-candidate", "engine_major": 4, "engine_minor": 26, "shipped": False,
            "why_not_shipped": "Only one source (UE4SS upstream, Win64/MSVC ABI). PS5 uses the Itanium ABI; "
                               "the second, independent source (Epic's own 4.26 declarations measured "
                               "for x86-64 Itanium) is not in the local tree. Vtable rows have zero "
                               "Itanium sources (MSVC slot order differs).",
            "rows": rows}


def emit_header(packs):
    out = ["#pragma once",
           "// GENERATED by caps-team-work/cap02/tools/build_packs.py; do not edit.",
           "// Engine-level layout packs (no game identities). Values are data; the",
           "// generator selects a pack only on an exact engine cell match.",
           "#include <cstddef>", "#include <cstdint>",
           "namespace rc::autoprofile::packs {",
           "struct Row { const char* section; const char* key; uint32_t value; };",
           "struct Pack { const char* id; const char* digest; uint32_t format, pointer_size, major, minor;",
           "  bool non_shipping, stats; const char* target; bool fname_case_preserving; uint32_t fname_bytes, fname_alignment;",
           "  const Row* members; size_t member_count; const Row* vtables; size_t vtable_count;",
           "  bool arming_cleared; uint32_t single_source_rows; };"]
    names = []
    for i, p in enumerate(packs):
        for kind in ("members", "vtables"):
            rows = [f'{{"{s}","{k}",0x{v:x}u}}' for s, rr in p[kind] for k, v in rr]
            out.append(f"inline constexpr Row kPack{i}_{kind}[]={{" + ",".join(rows) + "};")
        out.append(f'inline constexpr Pack kPack{i}{{"{p["id"]}","{p["digest"]}",{p["format"]}u,{p["pointer_size"]}u,'
                   f'{p["engine_major"]}u,{p["engine_minor"]}u,{str(p["non_shipping"]).lower()},{str(p["stats"]).lower()},'
                   f'"{p["target"]}",{str(p["fname"]["case_preserving"]).lower()},{p["fname"]["bytes"]}u,{p["fname"]["alignment"]}u,'
                   f'kPack{i}_members,sizeof(kPack{i}_members)/sizeof(Row),kPack{i}_vtables,sizeof(kPack{i}_vtables)/sizeof(Row),'
                   f'{str(p["arming_cleared"]).lower()},{p["single_source_rows"]}u}};')
        names.append(f"&kPack{i}")
    out.append("inline constexpr const Pack* kPacks[]={" + ",".join(names) + "};")
    out.append(f"inline constexpr size_t kPackCount={len(packs)};")
    out.append(f'inline constexpr const char* kGeneratorVersion="{GENERATOR_VERSION}";')
    out.append("}")
    return "\n".join(out) + "\n"


def pack_digest(p):
    canon = json.dumps({k: p[k] for k in ("id", "format", "pointer_size", "engine_major", "engine_minor",
                                          "non_shipping", "stats", "target", "fname", "members", "vtables")},
                       sort_keys=True, separators=(",", ":"))
    return sha256(canon.encode())


def main():
    mem, vt = r11_texts()
    (CAP / "evidence").mkdir(parents=True, exist_ok=True)
    (CAP / "evidence/r11-members.ini").write_text(mem)
    (CAP / "evidence/r11-vtables.ini").write_text(vt)
    p56, table = build_56(mem, vt)
    p56["digest"] = pack_digest(p56)
    c426 = build_426_candidate()
    (CAP / "packs/src").mkdir(parents=True, exist_ok=True)
    (CAP / "packs/src" / f"{p56['id']}.json").write_text(json.dumps(p56, indent=1) + "\n")
    (CAP / "packs/src/ue4.26-candidate.json").write_text(json.dumps(c426, indent=1) + "\n")
    two = sum(1 for r in table if r["status"] == "TWO-SOURCE")
    corro_agree = sum(1 for r in table if r["corroboration_only"] and
                      list(r["corroboration_only"].values())[0] == r["value"])
    corro_total = sum(1 for r in table if r["corroboration_only"])
    summary = {"pack": p56["id"], "digest": p56["digest"], "rows": len(table),
               "member_rows": sum(1 for r in table if r["kind"] == "member"),
               "vtable_rows": sum(1 for r in table if r["kind"] == "vtable"),
               "two_source_rows": two, "legacy_rows": len(table) - two,
               "uepseudo_5_06_corroboration": f"{corro_agree}/{corro_total} agree",
               "ue4_26_candidate_rows": len(c426["rows"]), "ue4_26_shipped": False, "table": table}
    (CAP / "evidence/PACK-SOURCES.json").write_text(json.dumps(summary, indent=1) + "\n")
    hdr = emit_header([p56])
    if FORBIDDEN.search(hdr):
        raise SystemExit("FAIL: game identity text leaked into the generated header")
    (CAP / "PATCH/new-files").mkdir(parents=True, exist_ok=True)
    (CAP / "PATCH/new-files/AutoLayoutPacks.generated.hpp").write_text(hdr)
    print(f"checked rows={len(table)} (members={summary['member_rows']} vtables={summary['vtable_rows']}) "
          f"two-source={two} legacy={len(table)-two} uepseudo-corroboration={summary['uepseudo_5_06_corroboration']} "
          f"ue4.26-candidate-rows={len(c426['rows'])} shipped-packs=1 arming_cleared={str(p56['arming_cleared']).lower()} "
          f"digest={p56['digest']}")
    if not table or not c426["rows"]:
        print("FAIL: zero rows checked"); return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
