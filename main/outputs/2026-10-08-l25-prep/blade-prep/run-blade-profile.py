#!/usr/bin/env python3
"""Blade drop-in profile runner (blind-test generic generator, Mac-only prep).

Takes THE USER'S FUTURE DECRYPTED EBOOT PATH (acquisition per decrypt-off law
is documented in PREREQ-CHECKLIST.md and NEVER performed by this tool), runs
the existing classifier + anchor machinery, joins the per-version layout
table, and emits:

  - engine verdict (version / build config / stats axis, with margins)
  - generated profile JSON (schema-1 shape) when the evidence bar is met
  - confidence per pin (static confidence + runtime confidence, separate)
  - REFUSAL (verdict + reasons) when below bar - the Astro Bot path

GAME-AGNOSTIC LAW (Rule 10 extended to games): this script accepts a PATH,
never a game name; no game name, image sha or title id appears in any branch.
The only knowledge consulted is per-version-table.json, keyed by ENGINE
VERSION. No network, no console, no writes outside --out.

Machinery reused, not reimplemented:
  - ref_axes.py (axes classifier) as a SUBPROCESS, exactly as pinned in the
    Townfall profile receipts (C+Python agreement law).
  - offline_anchor_scan.py / FZ-GAME2 astro_fakeself_dryrun.py pattern
    syntax + anchor-first scan loop (patternsleuth resolver patterns,
    verbatim sets).
  - per-version-table.json (this dir) supplies the measured 5.6 row + seeds.

Usage:
  python3 run-blade-profile.py <decrypted-eboot.bin> [--table per-version-table.json]
                               [--out runs] [--ref-axes <ref_axes.py>]
                               [--engine-version-rs <engine_version.rs>]
Exit code 0 for both ACCEPTED and REFUSED (refusal is a valid verdict); 2 on
tool error.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]  # .../what
DEFAULT_REF_AXES = ROOT / "outputs/2026-10-04-zcode-team/worker-axes-detector/tools/ref_axes.py"
DEFAULT_RS = ROOT / ".superpowers/native-dependency-spike/ue4ss-native-patternsleuth-1/upstream/patternsleuth/src/resolvers/unreal/engine_version.rs"

# Anchor pattern sets, VERBATIM from worker-required-slots-feasibility
# offline_anchor_scan.py / patternsleuth resolvers (the sets that pinned
# Townfall and refused Astro Bot). '|' marks the address-of-interest offset,
# '[ ... ]' a capture span (wildcard bytes in every pattern here).
GUO_PATTERNS = [
    "8B 05 ?? ?? ?? ?? 3B 05 ?? ?? ?? ?? 75 ?? 48 8D 15 ?? ?? ?? ?? 48 8D 0D | ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8D 05",
    "74 ?? 48 8D 0D | ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01 E8 ?? ?? ?? ?? C6 05 ?? ?? ?? ?? 01",
    "75 ?? 48 ?? ?? 48 8D 0D | ?? ?? ?? ?? E8 ?? ?? ?? ?? 45 33 C9 4C 89 74 24",
    "45 84 c0 48 c7 41 10 00 00 00 00 b8 ff ff ff ff 4c 8d 1d | ?? ?? ?? ?? 89 41 08 4c 8b d1 4c 89 19 0f 45 05 ?? ?? ?? ?? ff c0 89 41 08 3b 05",
    "81 ce 00 00 00 02 83 e0 fb 89 47 08 48 8d 0d | ?? ?? ?? ?? 48 89 fa 45 31 c0 e8 ?? ?? ?? ??",
    "E8 ?? ?? ?? ?? 8B 05 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 03 0D | ?? ?? ?? ?? 29 C8 87 05",
    "41 39 ee 0f 8e ?? ?? ?? ?? bf | ?? ?? ?? ?? 48 8b 74 24 10 e8 ?? ?? ?? ?? e9",
    "8b 6f ?? 4c 89 f7 31 f6 e8 ?? ?? ?? ?? 41 39 ef 7e 0d bf | ?? ?? ?? ?? 48 89 de e8",
    "8B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ] 2B 05 [ ?? ?? ?? ?? ]",
]
GM_PATTERNS = [
    "48 ?? ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
    "eb 03 ?? 8b ?? 48 8b ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
    "e8 ?? ?? ?? ?? 48 8b ?? f0 ?? 0f b1 ?? | ?? ?? ?? ?? 74 ?? ?? 85 ?? 74 ?? ?? 8b",
    "48 85 C9 74 2E 53 48 83 EC 20 48 8B D9 48 8B ?? | ?? ?? ?? ?? 48 85 C9",
    "75 ?? E8 ?? ?? ?? ?? 48 8b 0d | ?? ?? ?? ?? 48 8b ?? 48 ?? ?? ff 50 ?? 48 83 c4 ?? ?? c3",
]
# UTF-16LE module-string census (FName ctor + FMalloc derivation route seeds).
FNAME_MODULE_STRINGS = ["Engine", "Renderer", "AnimGraphRuntime", "Landscape", "RenderCore", "FMallocBinned3"]

CONF_BAR = {  # acceptance bar (documented, engine-level; refusal below it)
    "marker_min": 3,             # engine markers present (Astro: 0; Townfall: 54)
    "version_resolved": True,    # version axis must resolve (ref_axes margin>=4)
    "guo_xref_refs": 50,         # lea refs needed to enter the candidate pool
    "fname_ctor_hits": 1,        # ctor-pattern hits (wildcard or exact row prefix)
}

# NOTE on the shipped pattern arm (pinned fact, worker-required-slots-
# feasibility REPORT.md): the 9 GUObjectArray + 5 GMalloc patternsleuth
# patterns have ZERO occurrences image-wide on the 5.6 reference image
# (compiler idiom drift) - 0/9 and 0/14 is the EXPECTED positive-control
# result, not a refusal. The reference DERIVATION arms that actually pinned
# Townfall are implemented below: (a) GUObjectArray label = rip-relative xref
# census over data/BSS targets (derive_seed_constants pass B: 1565 sites ->
# 0x93BA110, 0 mov-imm32); (b) FName ctor = exact measured-row prefix /
# wildcard pattern + UTF-16 module-string census (SeedTable kFNameCtorWchar
# route); (c) GMalloc = FMallocBinned3 UTF-16 literal census + deep-route
# flag (worker-gmalloc-derivation NEW ROUTE). The shipped patterns stay as
# probes: version-agnostic shapes that may hit other compiler versions.


def sha_slice(path, off, sz):
    h = hashlib.sha256()
    with path.open("rb") as f:
        f.seek(off)
        left = sz
        while left > 0:
            b = f.read(min(1 << 22, left))
            if not b:
                break
            h.update(b)
            left -= len(b)
    return h.hexdigest()


def parse_elf(path):
    head = path.open("rb").read(64)
    if len(head) < 64 or head[:4] != b"\x7fELF" or head[4] != 2 or head[5] != 1:
        return None, ("not a 64-bit little-endian plain ELF. Decrypt-off law: the PROFILING view is the "
                      "default-connection (decrypt-on) download of a PS5 eboot - magic 7f454c46. A Sony "
                      "magic (4f153d1d/5414f5ee) here means the file was fetched with SITE SELF and is "
                      "not statically profileable.")
    e_type, e_machine = struct.unpack_from("<HH", head, 16)
    e_entry, e_phoff = struct.unpack_from("<QQ", head, 24)
    e_phentsize, e_phnum = struct.unpack_from("<HH", head, 54)
    if e_machine != 62:
        return None, "not x86-64"
    loads, extent, sce_paths, build_id = [], 0, [], None
    with path.open("rb") as f:
        for i in range(e_phnum):
            f.seek(e_phoff + i * e_phentsize)
            ph = f.read(e_phentsize)
            if len(ph) < 56:
                return None, "short program header"
            p_type, p_flags = struct.unpack_from("<II", ph, 0)
            p_off, p_vaddr, _pa, p_filesz, p_memsz = struct.unpack_from("<QQQQQ", ph, 8)
            if p_type == 1 and p_filesz:
                loads.append(dict(i=i, flags=p_flags, off=p_off, vaddr=p_vaddr,
                                  filesz=p_filesz, memsz=p_memsz))
                extent = max(extent, p_vaddr + p_memsz)
            elif p_type == 0x6fffff00 and p_filesz:  # PT_SCE PATH carrier
                f2 = path.open("rb")
                f2.seek(p_off)
                payload = f2.read(p_filesz)
                f2.close()
                if payload[:4] == b"PATH":
                    _m, _r, rec_len, str_len = struct.unpack_from("<IIII", payload, 0)
                    if 12 + str_len <= p_filesz:
                        sce_paths.append(payload[12:12 + str_len].rstrip(b"\0").decode("ascii", "replace"))
            elif p_type == 4 and p_filesz:  # PT_NOTE -> GNU build-id
                f2 = path.open("rb")
                f2.seek(p_off)
                note = f2.read(p_filesz)
                f2.close()
                if len(note) < 12:
                    continue  # truncated/absent note (e.g. reconstruction gap)
                namesz, descsz, ntype = struct.unpack_from("<III", note, 0)
                off = 12
                if ntype == 3 and note[off:off + max(0, namesz - 1)] == b"GNU":
                    doff = off + ((namesz + 3) & ~3)
                    build_id = note[doff:doff + descsz].hex()
    return dict(e_type=e_type, entry=e_entry, loads=loads, extent=extent,
                sce_paths=sce_paths, build_id=build_id), None


def compile_pattern(pat):
    """offline_anchor_scan syntax -> (byte/None list, custom_at, capture spans)."""
    seq, custom_at, captures, cur_cap, in_capture = [], 0, [], None, False
    for t in pat.strip().split():
        if t == "|":
            custom_at = len(seq)
            continue
        if t == "[":
            in_capture = True
            cur_cap = len(seq)
            continue
        if t == "]":
            in_capture = False
            captures.append((cur_cap, len(seq)))
            continue
        if len(t) == 2 and all(c in "0123456789abcdefABCDEF" for c in t):
            seq.append(int(t, 16))
            continue
        if t == "??":
            seq.append(None)
            continue
        raise ValueError(f"token {t!r}")
    return seq, custom_at or 0, captures


def scan(buf, base_va, pat, limit=16):
    """Anchor-first regex scan (FZ astro_fakeself_dryrun.py loop).
    Returns list of (hit_vaddr, [(cap_start, cap_end) buffer-relative])."""
    seq, custom_at, captures = compile_pattern(pat)
    rx = bytearray()
    for b in seq:
        rx += b"." if b is None else re.escape(bytes([b]))
    rxre = re.compile(bytes(rx), re.S)
    first_fixed = next((k for k, b in enumerate(seq) if isinstance(b, int)), 0)
    anchor = bytes([seq[first_fixed]])
    out, pos = [], 0
    while True:
        idx = buf.find(anchor, pos)
        if idx < 0:
            break
        pos = idx + 1
        start = idx - first_fixed
        if start < 0:
            continue
        m = rxre.match(buf, start)
        if m and m.end() == start + len(seq):
            out.append((base_va + start + custom_at,
                        [(start + a, start + b) for a, b in captures]))
            if len(out) >= limit:
                break
    return out


def disp32_targets(buf, base_va, cap_ranges):
    """GUO pattern #8: rip-relative disp32 capture slots. Each carrier insn is
    '8B 05 d32' or '2B 05 d32' (6 bytes): opcode at cap_start-2, disp32 at
    cap_start..+4, next insn at cap_start+4. target = base_va + cap_start + 4
    + disp32 (RIP-relative law)."""
    out = []
    for a, b in cap_ranges:
        if b - a == 4 and a >= 2 and bytes(buf[a - 2:a]) in (b"\x8b\x05", b"\x2b\x05"):
            disp = struct.unpack_from("<i", buf, a)[0]
            out.append(base_va + a + 4 + disp)
    return out


def cluster(addresses, radius=0x400):
    out = []
    for a in sorted(set(addresses)):
        if out and a - out[-1][1][-1] <= radius:
            out[-1][1].append(a)
        else:
            out.append([a, [a]])
    return [(m[0], m) for m in out]


def run_ref_axes(ref_axes, rs, elf):
    try:
        r = subprocess.run([sys.executable, str(ref_axes), str(rs), str(elf)],
                           capture_output=True, text=True, timeout=1800)
    except subprocess.TimeoutExpired:
        return None, "ref_axes timeout"
    if r.returncode != 0:
        return None, f"ref_axes failed rc={r.returncode}: {r.stderr.strip()[:400]}"
    line = next((l for l in r.stdout.splitlines() if l.startswith("{")), None)
    if not line:
        return None, "ref_axes emitted no JSON line"
    return json.loads(line), None


def parse_band(s):
    parts = s.replace("UE", "").strip().split(".")
    while len(parts) < 2:
        parts.append("0")
    return (int(parts[0]), int(parts[1]))


def join_row(table, major, minor):
    for e in table["entries"]:
        if e["version"] == f"{major}.{minor}":
            return e
    for e in table["entries"]:
        if "-" in e["version"]:
            lo, hi = e["version"].split("-")
            if parse_band(lo) <= (major, minor) <= parse_band(hi):
                return e
    for e in table["entries"]:
        if "forward" in e["version_band"]:
            return e
    raise RuntimeError("table has no forward row")


def hex_pat(hexstr):
    return " ".join(hexstr.split())


RIP_LEA = re.compile(rb"[\x48\x4c\x49\x4d]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", re.S)
RIP_MOV = re.compile(rb"[\x48\x4c]\x8b[\x05\x0d\x15\x1d\x25\x2d\x35\x3d]", re.S)


def rip_census(exec_loads, data_spans, eboot):
    """derive_seed_constants pass-B arm: census rip-relative LEA (address-
    taken) and MOV (load) targets landing in non-exec data/BSS spans.
    Returns (lea_counter, mov_counter). Positive-control receipt: the
    reference image's GUObjectArray label shows EXACTLY 1565 lea + 0 mov
    (SeedTable kGUObjectArray uniqueness note)."""
    from collections import Counter
    lea_c, mov_c = Counter(), Counter()
    with eboot.open("rb") as f:
        for l in exec_loads:
            f.seek(l["off"])
            buf = f.read(l["filesz"])
            base = l["vaddr"]
            for m in RIP_LEA.finditer(buf):
                i = m.start()
                t = base + i + 7 + struct.unpack_from("<i", buf, i + 3)[0]
                for lo, hi in data_spans:
                    if lo <= t < hi:
                        lea_c[t] += 1
                        break
            for m in RIP_MOV.finditer(buf):
                i = m.start()
                t = base + i + 7 + struct.unpack_from("<i", buf, i + 3)[0]
                for lo, hi in data_spans:
                    if lo <= t < hi:
                        mov_c[t] += 1
                        break
    return lea_c, mov_c


def scan_anchors(args, report, exec_loads, all_loads, row):
    data_by_load = []
    with args.eboot.open("rb") as f:
        for l in exec_loads:
            f.seek(l["off"])
            buf = f.read(l["filesz"])
            # buffer index 0 == file offset l.off == vaddr l.vaddr (PT_LOAD law)
            data_by_load.append((buf, l["vaddr"]))

    def scan_all(pat):
        hits = []
        for buf, base in data_by_load:
            for va, caps in scan(buf, base, pat):
                hits.append((va, caps, buf, base))
        return hits

    # UTF-16 census over ALL file-backed loads (module strings live in the
    # read-only data segment, not exec; SeedTable receipts: Engine/Renderer/
    # AnimGraphRuntime/Landscape/RenderCore unique hits; FMallocBinned3 4x
    # substring occurrences)
    census = {}
    with args.eboot.open("rb") as f:
        for l in all_loads:
            f.seek(l["off"])
            buf = f.read(l["filesz"])
            for s in FNAME_MODULE_STRINGS:
                if s == "FMallocBinned3":
                    census[s] = census.get(s, 0) + buf.count(s.encode("utf-16-le"))
                else:
                    needle = ("\x00" + s + "\x00").encode("utf-16-le")
                    census[s] = census.get(s, 0) + buf.count(needle)
    report["fname_module_string_census"] = census

    # ---- GUObjectArray: shipped-pattern probes + xref-census derivation arm ----
    guo_per_pat = {i: scan_all(p) for i, p in enumerate(GUO_PATTERNS)}
    guo_hits = [h[0] for hits in guo_per_pat.values() for h in hits]
    label_targets = []
    for hits in guo_per_pat.values():
        for _va, caps, buf, base in hits:
            label_targets += disp32_targets(buf, base, caps)
    guo_clusters = cluster(guo_hits) if guo_hits else []
    pattern_consensus = 0
    best_center = None
    if guo_clusters:
        best_center, members = max(guo_clusters, key=lambda c: len(c[1]))
        pattern_consensus = sum(1 for i, hits in guo_per_pat.items()
                                if any(abs(h[0] - best_center) <= 0x400 for h in hits))
    # xref census arm: LEA(address-taken)-heavy data targets = candidate POOL.
    # HONESTY: 'top lea target' is NOT a valid unique discriminator on the
    # reference image (hot globals outrank the GUO label); the static census
    # emits a candidate pool and unique selection is deferred to the console's
    # live shape validators (SeedArming admission rounds) + the two-source law.
    data_spans = [(l["vaddr"], l["vaddr"] + l["memsz"])
                  for l in all_loads if not (l["flags"] & 1) and l["memsz"]]
    lea_c, mov_c = rip_census(exec_loads, data_spans, args.eboot)
    lea_pool = [{"vaddr": hex(t), "lea_refs": c, "mov_refs": mov_c.get(t, 0)}
                for t, c in lea_c.most_common(24) if c >= CONF_BAR["guo_xref_refs"]]
    address_taken_only = [e for e in lea_pool if e["mov_refs"] == 0]
    row_label = row.get("guobjectarray", {}).get("label", {}).get("image_vaddr")
    row_label_in_pool = any(e["vaddr"].lower() == str(row_label).lower() for e in lea_pool) if row_label else None
    pool_ok = len(address_taken_only) >= 1
    report["pins"]["slot0_guobjectarray"] = {
        "kind": "data-label candidate POOL (lea xref census arm + 9 shipped pattern probes)",
        "lea_top_targets": lea_pool,
        "address_taken_only_pool": address_taken_only[:24],
        "pool_nonempty": pool_ok,
        "row_label_in_pool": row_label_in_pool,
        "shipped_pattern_hits": len(guo_hits),
        "shipped_patterns_with_hits": sum(1 for h in guo_per_pat.values() if h),
        "shipped_pattern_consensus": pattern_consensus,
        "pattern8_disp32_label_candidates": [hex(t) for t in sorted(set(label_targets))][:12],
        "static_confidence": "high" if (pool_ok and row_label_in_pool) else ("medium" if pool_ok else "low"),
        "runtime_confidence": 0.0,
        "note": "reference derivation (derive_seed_constants pass B): the reference image's "
                "GUObjectArray label carries EXACTLY 1565 lea + 0 mov rip-refs (SeedTable receipt); "
                "the lea-only pool is the static candidate set; unique admission happens at runtime "
                "via live shape validators (chunked-65536 array shape) + two-source law. 0/9 shipped "
                "patterns on the reference image is the PINNED expected result (compiler idiom drift)",
    }

    # ---- GMalloc anchor family ----
    gm_per_pat = {i: scan_all(p) for i, p in enumerate(GM_PATTERNS)}
    gm_hits = [h[0] for hits in gm_per_pat.values() for h in hits]
    binned3 = census.get("FMallocBinned3", 0)
    report["pins"]["slot3_gmalloc"] = {
        "kind": "data-label anchor family (5 patterns)",
        "hits": len(gm_hits),
        "patterns_with_hits": sum(1 for h in gm_per_pat.values() if h),
        "clusters": [[hex(c), len(m)] for c, m in (cluster(gm_hits) if gm_hits else [])][:6],
        "fmallocbinned3_utf16_literals": binned3,
        "static_confidence": "medium" if (sum(1 for h in gm_per_pat.values() if h) >= 2 and binned3) else "low",
        "runtime_confidence": 0.0,
        "note": "full derivation route (UTF-16 literals -> validation roots -> RELA vtable -> ctor -> "
                "factory -> ONE bootstrap store) required before the GMalloc label is pinned; "
                "V1/V2 validators gate arming (worker-gmalloc-derivation route)",
    }

    # ---- FName ctor wchar (slot 2): wildcard seed + measured-row exact prefix ----
    ctor = row.get("fname_ctor_wchar_slot2", {})
    wc_hits = scan_all(ctor["wildcard_pattern"]) if ctor.get("wildcard_pattern") else []
    ex_hits = scan_all(hex_pat(ctor["prefix_bytes_32"])) if ctor.get("prefix_bytes_32") else []
    all_ctor = [h[0] for h in wc_hits] + [h[0] for h in ex_hits]
    ctor_clusters = cluster(all_ctor) if all_ctor else []
    expected = ctor.get("label", {}).get("image_vaddr")
    report["pins"]["slot2_fname_ctor_wchar"] = {
        "kind": "code slot (sret FName(const WIDECHAR*, EFindName))",
        "hits": len(all_ctor),
        "wildcard_hits": len(wc_hits),
        "exact_row_prefix_hits": len(ex_hits),
        "clusters": [[hex(c), len(m)] for c, m in ctor_clusters[:6]],
        "expected_label_row": expected,
        "expected_label_hit": any(h[0] == int(expected, 16) for h in ex_hits) if expected else None,
        "static_confidence": "high" if ex_hits else ("medium" if wc_hits else "none"),
        "runtime_confidence": 0.0,
        "note": "consensus machinery (module-string cluster -> xref census -> mov edx,1 + e8 rel32 "
                "consensus -> call-site re-verification) required before admission",
    }

    # ---- corroboration anchors from the measured row (version-hypothesis
    # probes: a hit strengthens; absence on ANOTHER version is not refusal) ----
    for key in ("fname_tostring_internal", "static_construct_object_internal"):
        e = row.get(key, {})
        px = e.get("prefix_bytes_32") or e.get("prefix_bytes_25")
        if not px:
            continue
        hits = scan_all(hex_pat(px))
        expected = e.get("label", {}).get("image_vaddr")
        report["pins"][key] = {
            "hits": len(hits),
            "sites": [hex(h[0]) for h in hits[:4]],
            "expected_label_row": expected,
            "expected_label_hit": any(h[0] == int(expected, 16) for h in hits) if expected else None,
            "static_confidence": "corroboration-only",
        }
    ops = row.get("engine_uobject_listener_ops", {})
    for name in ("add_delete_listener", "remove_delete_listener"):
        e = ops.get(name, {})
        px = e.get("prefix_bytes_16")
        if not px:
            continue
        hits = scan_all(hex_pat(px))
        report["pins"][f"engine_{name}"] = {
            "hits": len(hits),
            "sites": [hex(h[0]) for h in hits[:4]],
            "expected_label_row": e.get("image_vaddr"),
            "static_confidence": "corroboration-only",
        }
    return report


def build_profile(report, row, axes):
    cfg = {1: "shipping", 2: "development", 3: "test"}[axes["build_configuration"]]
    stats = {1: "off", 2: "on"}[axes["stats"]]
    members, vtables = [], []
    if row["status"] == "measured":
        ga = row["guobjectarray"]
        for k, v in ga["fuobjectarray_fields"].items():
            members.append({"name": f"FUObjectArray::{k}", "offset": v, "width": None,
                            "source": "engine-table-5.6 measured receipts (per-version-table.json); engine-version-level fact; live admission still required"})
        for k, v in ga["tuobjectarray_fields"].items():
            members.append({"name": f"TUObjectArray::{k}", "offset": v, "width": None,
                            "source": "engine-table-5.6 measured receipts"})
        members += [
            {"name": "FUObjectItem::Object", "offset": 0, "width": 8, "source": "engine-table-5.6 measured receipts"},
            {"name": "FUObjectItem::Flags", "offset": 8, "width": 4, "source": "engine-table-5.6 measured receipts"},
            {"name": "FUObjectItem::UEP_TotalSize", "offset": ga["fuobjectitem"]["total_size"], "width": None,
             "source": "engine-table-5.6 measured receipts"},
        ]
        for e in row["fmalloc"]["vtable_order_measured"]:
            vtables.append({"name": f"FMalloc::{e['method']}", "offset": e["byte_offset"], "width": 8,
                            "source": "engine-table-5.6 measured receipts (l21 allocator candidate-table)"})
    return {
        "schema": 1,
        "name": f"ps5-ue{row['version'].replace('.', '')}-{cfg}-stats-{stats}",
        "image": {"sha256": report["sha256"], "size": report["size"],
                  "extent": report["elf"]["extent"], "base": 0,
                  "base_status": "unresolved-live-observation-required"},
        "derived_label_candidates": {
            "guobjectarray_lea_pool": {
                "address_taken_only": report["pins"]["slot0_guobjectarray"].get("address_taken_only_pool"),
                "status": "static candidate POOL only - unique selection deferred to live shape "
                          "validators (chunked array shape) + two-source law; labels are IMAGE facts, "
                          "never transferable between games even of the same engine version"},
            "fname_ctor_wchar": {
                "sites": report["pins"]["slot2_fname_ctor_wchar"].get("clusters"),
                "status": "candidate (consensus machinery + live admission still required)"},
        },
        "layout": {"format": 1, "target": "prospero-x86_64-sysv", "pointer_size": 8,
                   "major": int(row["version"].split(".")[0]), "minor": int(row["version"].split(".")[1]),
                   "engine_revision": row.get("engine_revision_official", "unresolved"),
                   "build_id": (report["elf"].get("build_id") or "unresolved")},
        "axes": {"build_configuration": axes["build_configuration"], "stats": axes["stats"],
                 "build_source": "; ".join(report["elf"]["sce_paths"]) or "PT_SCE PATH absent",
                 "stats_source": f"244-marker classification:{axes['present_count']} present; margins {axes['costs']}"},
        "fname": {"case_preserving": row["fname_pool"]["case_preserving"],
                  "output_bytes": row["fname_pool"]["fname_value"]["bytes"],
                  "output_alignment": row["fname_pool"]["fname_value"]["alignment"],
                  "source": "engine-table-5.6 measured receipts"},
        "evidence": {"table": f"per-version-table.json entry {row['version']}",
                     "generator": "blade-prep/run-blade-profile.py",
                     "anchor_probes": report["pins"],
                     "census": report.get("fname_module_string_census")},
        "required_members": members,
        "required_vtables": vtables,
        "unknown_required_static_facts": ["image.base (live observation required)",
                                          "all runtime slot labels until live admission"],
        "blind_test_law": "profile generated from the engine-version table only; no game-specific facts anywhere",
    }


def emit(args, report, refused, row=None, axes=None):
    outdir = args.out / (report["sha256"][:12] if report["sha256"] else "noid")
    outdir.mkdir(parents=True, exist_ok=True)
    report["verdict"] = "REFUSED" if refused else "ACCEPTED"
    if not refused and row is not None and axes is not None:
        report["profile"] = build_profile(report, row, axes)
        (outdir / "profile.json").write_text(json.dumps(report["profile"], indent=2) + "\n")
    (outdir / "verdict.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"verdict": report["verdict"], "sha256": report["sha256"],
                      "engine": report.get("engine_verdict"), "refusals": report["refusals"],
                      "pins": {k: {kk: v.get(kk) for kk in ("hits", "static_confidence", "expected_label_hit") if kk in v}
                               for k, v in report.get("pins", {}).items()},
                      "out": str(outdir)}, indent=2))
    return 0


def main():
    ap = argparse.ArgumentParser(description="generic per-version-table profile generator (path in, verdict out; no game names)")
    ap.add_argument("eboot", type=Path)
    ap.add_argument("--table", type=Path, default=HERE / "per-version-table.json")
    ap.add_argument("--out", type=Path, default=HERE / "runs")
    ap.add_argument("--ref-axes", type=Path, default=DEFAULT_REF_AXES)
    ap.add_argument("--engine-version-rs", type=Path, default=DEFAULT_RS)
    args = ap.parse_args()
    table = json.loads(args.table.read_text())

    report = {"tool": "run-blade-profile.py", "eboot": str(args.eboot),
              "sha256": None, "size": args.eboot.stat().st_size,
              "refusals": [], "engine_verdict": None, "pins": {},
              "base_policy": "labels are image-relative; live base MUST be observed at runtime before any pin is used (PS5 eboots are ASLR'd; Townfall 0x400000 is one observation, not a law)"}

    if not args.eboot.is_file():
        report["refusals"].append("input file does not exist")
        return emit(args, report, refused=True)
    elf, err = parse_elf(args.eboot)
    if elf is None:
        report["refusals"].append(f"container: {err}")
        return emit(args, report, refused=True)

    report["sha256"] = sha_slice(args.eboot, 0, report["size"])
    report["elf"] = {"e_type": hex(elf["e_type"]), "entry": hex(elf["entry"]),
                     "extent": elf["extent"], "sce_paths": elf["sce_paths"],
                     "build_id": elf["build_id"]}
    exec_loads = [l for l in elf["loads"] if l["flags"] & 1]
    report["elf"]["exec_pt_loads"] = [{k: hex(l[k]) for k in ("i", "off", "vaddr", "filesz")}
                                      for l in exec_loads]

    axes, axerr = run_ref_axes(args.ref_axes, args.engine_version_rs, args.eboot)
    report["axes"] = axes
    if axerr:
        report["refusals"].append(f"axes: {axerr}")
        return emit(args, report, refused=True)
    if axes["present_count"] < CONF_BAR["marker_min"]:
        report["refusals"].append(
            f"axes: only {axes['present_count']} engine markers present (<{CONF_BAR['marker_min']}) - "
            "not an Unreal Engine binary (the Astro Bot refusal path)")
        return emit(args, report, refused=True)
    if axes["version"] is None:
        report["refusals"].append("axes: engine version axis UNRESOLVED (margin<4) - per-version table cannot be joined")
        return emit(args, report, refused=True)

    major, minor = map(int, axes["version"].split("."))
    report["engine_verdict"] = {"version": axes["version"],
                                "build_configuration": axes["build_configuration"],
                                "stats": axes["stats"], "margins": axes["costs"]}
    row = join_row(table, major, minor)
    report["table_row"] = {"version": row["version"], "band": row["version_band"],
                           "status": row["status"], "confidence": row["confidence"]}

    scan_anchors(args, report, exec_loads, elf["loads"], row)

    if row["status"] != "measured":
        report["refusals"].append(
            f"table: version {axes['version']} joined a SEED row (confidence 0.0) - seeds never emit "
            "pins; measure the binary per the row's confirm_when list and upgrade the table first")
        return emit(args, report, refused=True)

    guo = report["pins"].get("slot0_guobjectarray", {})
    ctor = report["pins"].get("slot2_fname_ctor_wchar", {})
    if not (guo.get("pool_nonempty") or guo.get("shipped_pattern_consensus", 0) >= 2):
        report["refusals"].append(
            f"anchors: GUObjectArray derivation arm failed - lea xref candidate pool empty "
            f"(bar: >=1 data/BSS target with lea_refs>={CONF_BAR['guo_xref_refs']} and 0 mov-refs); "
            f"shipped-pattern consensus {guo.get('shipped_pattern_consensus', 0)} (<2)")
    if ctor.get("hits", 0) < CONF_BAR["fname_ctor_hits"]:
        report["refusals"].append(f"anchors: FName ctor pattern hits {ctor.get('hits', 0)} "
                                  f"(<{CONF_BAR['fname_ctor_hits']})")
    if report["refusals"]:
        return emit(args, report, refused=True)
    return emit(args, report, refused=False, row=row, axes=axes)


if __name__ == "__main__":
    sys.exit(main())
