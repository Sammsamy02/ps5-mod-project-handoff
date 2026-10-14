#!/usr/bin/env python3
# verify_intake.py — intake gate for PC Townfall material (worker-pc-acquisition-pack, 2026-10-05)
#
# Implements the acquisition/intake half of COMPARISON-PROTOCOL.md
# (worker-re-pc-ps5-comparison, 2026-10-04): presence, sha256, PE/ELF class via
# lief, the version-divergent vs platform-divergent classification law (§2.2),
# and the ready-to-run comparison op list with the PS5 anchors already filled (§5).
#
# READ-ONLY on the staging dir. It never writes there (protocol §2.3: the capture
# dir holds the files + capture.json and nothing else). Optional --json PATH writes
# a machine receipt OUTSIDE the staging dir.
#
# Usage:
#   python3 verify_intake.py <staging_dir> [--json OUT_PATH]
# Exit codes: 0 = READY, 1 = NOT-READY (dir exists, evidence missing or wrong),
#             2 = staging dir missing/not a directory.

import argparse
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone

try:
    import lief  # 1.0.0 installed
    HAVE_LIEF = True
except ImportError:
    HAVE_LIEF = False

# ---------------------------------------------------------------------------
# Evidence patterns (all mechanical, evidence-based scans — no assumptions)
# ---------------------------------------------------------------------------

TITLE_ASCII = b"Townfall"
TITLE_UTF16 = "Townfall".encode("utf-16-le")

# UE build strings embedded in shipping executables, e.g. "++UE5+Release-5.6-CL-0".
RX_UE_RELEASE = [
    re.compile(rb"\+\+UE\d*\+Release-(\d+\.\d+)"),
    re.compile(rb"Release-(\d+\.\d+)-CL-\d+"),
]

# Protocol §5 op 5: allocator flavor literals live as UTF-16 (TCHAR) strings.
ALLOCATOR_UTF16 = {
    "FMallocBinned3": "FMallocBinned3".encode("utf-16-le"),
    "FMallocBinned2": "FMallocBinned2".encode("utf-16-le"),
    "FMallocAnsi": "FMallocAnsi".encode("utf-16-le"),
}

# PS5 pinned reference (INVENTORY.md / COMPARISON-PROTOCOL.md §2 — retained, cited,
# not re-verified by this worker): sha 3b6cfad4…3622fc, UE 5.6 validated-static.
PS5_REF = {
    "sha256": "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc",
    "engine": "5.6",
    "engine_provenance": "validated-static serialized version; revision/patch unknown",
}

REQUIRED_RECEIPT_FIELDS = ["store", "store_version", "remote_path", "expected_bytes"]
OPTIONAL_RECEIPT_FIELDS = ["utc", "store_version_source", "acquired_from_machine"]

CHUNK = 1 << 20          # 1 MiB scan chunks
OVERLAP = 8192           # > any pattern length; guards chunk-boundary straddles


# ---------------------------------------------------------------------------
# Mechanical helpers
# ---------------------------------------------------------------------------

def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for blk in iter(lambda: f.read(CHUNK), b""):
            h.update(blk)
    return h.hexdigest()


def scan_binary(path):
    """Chunked, overlap-guarded literal/regex scan. Returns evidence dict."""
    ev = {
        "title_ascii_hits": 0, "title_ascii_first": None,
        "title_utf16_hits": 0, "title_utf16_first": None,
        "allocator_utf16": {}, "ue_versions": [], "ue_hits": 0,
    }
    fixed = {
        "title_ascii": TITLE_ASCII.lower(),
        "title_utf16": TITLE_UTF16.lower(),
    }
    for k in ALLOCATOR_UTF16:
        fixed["alloc_" + k] = ALLOCATOR_UTF16[k].lower()
        ev["allocator_utf16"][k] = 0
    first = {}
    with open(path, "rb") as f:
        prev_tail = b""
        chunk_off = 0  # file offset of the current chunk's first byte
        while True:
            chunk = f.read(CHUNK)
            if not chunk:
                break
            buf = prev_tail + chunk
            low = buf.lower()
            min_end = len(prev_tail) + 1  # skip matches fully inside the overlap
            buf_start = chunk_off - len(prev_tail)
            for name, pat in fixed.items():
                start = 0
                while True:
                    i = low.find(pat, start)
                    if i < 0:
                        break
                    start = i + 1
                    if i + len(pat) < min_end:
                        continue  # already counted in the previous round
                    if name.startswith("alloc_"):
                        ev["allocator_utf16"][name[6:]] += 1
                    elif name == "title_ascii":
                        ev["title_ascii_hits"] += 1
                    elif name == "title_utf16":
                        ev["title_utf16_hits"] += 1
                    if name not in first:
                        first[name] = buf_start + i
            for rx in RX_UE_RELEASE:
                for m in rx.finditer(buf):
                    if m.end() < min_end:
                        continue
                    ev["ue_hits"] += 1
                    v = m.group(1).decode("ascii", "replace")
                    if v not in ev["ue_versions"]:
                        ev["ue_versions"].append(v)
            prev_tail = chunk[-OVERLAP:]
            chunk_off += len(chunk)
    ev["title_first_offsets"] = {
        k: first[k] for k in ("title_ascii", "title_utf16") if k in first
    }
    return ev


def sniff_format(path):
    with open(path, "rb") as f:
        head = f.read(4)
    if head[:2] == b"MZ":
        return "PE"
    if head[:4] == b"\x7fELF":
        return "ELF"
    return "UNKNOWN"


def pe_info(path):
    """lief PE parse → structural facts (protocol §4 pass-1 intake subset)."""
    out = {"format": "PE", "machine": None, "pe32plus": None, "error": None}
    if not HAVE_LIEF:
        out["error"] = "lief not importable"
        return out
    try:
        b = lief.PE.parse(str(path))
        if b is None:
            out["error"] = "lief.PE.parse returned None"
            return out
        try:
            out["machine"] = str(b.header.machine).split(".")[-1]
        except Exception:
            pass
        try:
            out["pe32plus"] = "PE32_PLUS" in str(b.optional_header.magic)
        except Exception:
            pass
        try:
            out["nsections"] = len(b.sections)
            out["sections"] = [str(s.name) for s in b.sections][:20]
        except Exception:
            pass
        try:
            out["entrypoint"] = int(b.entrypoint)
        except Exception:
            pass
        try:
            out["characteristics"] = [str(c).split(".")[-1] for c in b.header.characteristics.list()][:16]
        except Exception:
            pass
    except Exception as e:
        out["error"] = "lief PE parse failed: %s" % e
    return out


def elf_info(path):
    out = {"format": "ELF", "class": None, "machine": None, "e_type": None, "error": None}
    if not HAVE_LIEF:
        out["error"] = "lief not importable"
        return out
    try:
        with open(path, "rb") as f:
            ident = f.read(16)
        out["class"] = "ELF64" if len(ident) > 4 and ident[4] == 2 else "ELF32"
        b = lief.ELF.parse(str(path))
        if b is None:
            out["error"] = "lief.ELF.parse returned None"
            return out
        try:
            out["machine"] = str(b.header.machine_type).split(".")[-1]
        except Exception:
            pass
        try:
            out["e_type"] = int(b.header.file_type)
        except Exception:
            pass
    except Exception as e:
        out["error"] = "lief ELF parse failed: %s" % e
    return out


# ---------------------------------------------------------------------------
# Classification (protocol §2.2 law — never merge the failure classes)
# ---------------------------------------------------------------------------

def classify(executables, receipt, byte_mismatch=None):
    """Returns (verdict_id, verdict_lines, ready_bool, blockers, warnings).

    READY law (stated in output, protocol §2.2/§2.4): a Townfall-identified Win64 PE
    with computed sha + provenance receipt (store, remote_path). The version class
    (SAME-UPDATE-PLAUSIBLE / VERSION-DIVERGENT / VERSION-UNPROVEN) gates how future
    anchor mismatches are reported — it is output, not a hard gate, because §2.2
    says "if unobtainable, proceed anyway". A byte-count mismatch against the
    user-provided expected_bytes IS a hard blocker (incomplete copy).
    """
    lines = []
    warnings = []
    ps5 = "PS5 pinned reference: sha %s…%s, UE %s (%s)" % (
        PS5_REF["sha256"][:8], PS5_REF["sha256"][-6:], PS5_REF["engine"], PS5_REF["engine_provenance"])

    exes = [e for e in executables if e["sniff"] in ("PE", "ELF")]
    if not exes:
        return ("NO_EXECUTABLE",
                ["  No PE/ELF executable staged — nothing to classify.",
                 "  " + ps5], False, ["no executable staged"], [])

    exe = exes[0]  # intake considers the first parseable executable
    lines.append("  Executable under classification: %s" % exe["name"])
    lines.append("  " + ps5)

    if exe["sniff"] == "ELF":
        lines.append("  VERDICT: PS5-SIDE-FORMAT / NOT-PC-INTAKE")
        lines.append("    ELF input (e_type %s, %s %s). PC Townfall intake is PE/PE32+ (Win64)."
                     % (exe["info"].get("e_type"), exe["info"].get("class") or "ELF", exe["info"].get("machine") or ""))
        lines.append("    An ELF here is the PS5 pinned side or a mis-staged file — do NOT count it as PC material.")
        return ("PS5_SIDE_FORMAT", lines, False,
                ["staged executable is ELF, not PE — not PC intake"], warnings)

    info = exe["info"]
    ev = exe["strings"]
    lines.append("  PE class: %s, machine=%s, sections=%s%s" % (
        "PE32+ (Win64)" if info.get("pe32plus") else "PE32+ unknown",
        info.get("machine") or "?", info.get("nsections", "?"),
        "" if not info.get("error") else " (lief note: %s)" % info["error"]))

    title_found = (ev["title_ascii_hits"] + ev["title_utf16_hits"]) > 0
    lines.append('  Title marker "Townfall": %s (ascii %d hit(s), utf16 %d hit(s)%s)' % (
        "FOUND" if title_found else "NOT FOUND",
        ev["title_ascii_hits"], ev["title_utf16_hits"],
        "".join(", first @ 0x%x" % o for o in ev["title_first_offsets"].values())))

    if not title_found:
        lines.append("  VERDICT: FOREIGN-PE — NOT TOWNFALL")
        lines.append("    The staged executable parses but carries no Townfall marker.")
        lines.append("    It is NOT the title: do not compare, do not count as PC Townfall intake.")
        lines.append("    (No version-divergent/platform-divergent claim applies to a non-title binary.")
        lines.append("     Note: absence of the literal is strong-but-not-proof evidence; the store")
        lines.append("     manifest cross-check, once present, is the confirming authority.)")
        return ("FOREIGN_PE", lines, False, ["no Townfall title marker in executable"], warnings)

    allocs = [k for k, n in ev["allocator_utf16"].items() if n]
    lines.append("  Allocator flavor literals (UTF-16, protocol §5 op 5): %s" % (
        ", ".join("%s x%d" % (k, ev["allocator_utf16"][k]) for k in allocs) if allocs
        else "none found — TO-COLLECT, do not assume"))
    ue = ev["ue_versions"]
    lines.append("  Engine release string(s): %s (from %d literal hit(s))" % (
        ", ".join(ue) if ue else "none found", ev["ue_hits"]))

    if not ue:
        lines.append("  VERDICT: VERSION-UNPROVEN — TOWNFALL, UPDATE NOT PINNED")
        lines.append("    Per protocol §2.2: until the title update is pinned, EVERY non-matching")
        lines.append("    anchor is reported VERSION-DIVERGENT, never platform-divergent.")
        lines.append("    Pin it via the store manifest cross-check (ACQUIRE.md step 2). Proceeding")
        lines.append("    without it is allowed (§2.2) but the version delta stays first-class.")
    elif PS5_REF["engine"] in ue:
        lines.append("  VERDICT: SAME-UPDATE-PLAUSIBLE — TOWNFALL, PC UE %s == PS5 UE %s" % (
            ", ".join(ue), PS5_REF["engine"]))
        lines.append("    Anchor mismatches may be argued PLATFORM-DIVERGENT per anchor, with per-anchor")
        lines.append("    evidence, once the store manifest confirms the same title update (§2.2).")
        lines.append("    Until that manifest confirmation, mismatches remain VERSION-UNPROVEN.")
    else:
        lines.append("  VERDICT: VERSION-DIVERGENT — TOWNFALL PC UE %s vs PS5 UE %s" % (
            ", ".join(ue), PS5_REF["engine"]))
        lines.append("    The version delta is FIRST-CLASS output (protocol §2.2): every non-matching")
        lines.append("    anchor is reported version-divergent, NEVER platform-divergent.")
        lines.append("    Proceed anyway (§2.2); never merge the two failure classes.")

    # Provenance receipt gates (§2.4): store + remote_path required; the rest warn.
    blockers = []
    for k in ("store", "remote_path"):
        if receipt.get(k) in (None, "", "UNKNOWN"):
            blockers.append("capture.json field '%s' missing (provenance law §2.4)" % k)
    if receipt.get("store_version") in (None, "", "UNKNOWN"):
        warnings.append("capture.json 'store_version' unfilled — title update cannot be pinned; "
                        "classification above already reflects that law")
    if byte_mismatch is not None:
        blockers.append("copy INCOMPLETE: expected_bytes=%s but actual=%d — re-copy the file"
                        % byte_mismatch)
    ready = not blockers
    return (lines_next_verdict_id(lines), lines, ready, blockers, warnings)


def lines_next_verdict_id(lines):
    for l in lines:
        if "SAME-UPDATE-PLAUSIBLE" in l:
            return "SAME_UPDATE_PLAUSIBLE"
        if "VERSION-DIVERGENT" in l:
            return "VERSION_DIVERGENT"
        if "VERSION-UNPROVEN" in l:
            return "VERSION_UNPROVEN"
    return "TOWNFALL_UNKNOWN"


# ---------------------------------------------------------------------------
# Ready-to-run comparison op list (PS5 anchors pre-filled from protocol §5)
# ---------------------------------------------------------------------------

OP_TABLE = """\
[READY-TO-RUN COMPARISON OP LIST — prioritized 5-op table, PS5 anchors FILLED]
(the PC evidence column runs only after READY; PS5 column is retained, cited, not re-verified.
 PS5 constraint that shapes every op: PS5 text was EXECUTE-ONLY in-game; the PC host reads its
 own text freely, so PC validators can be code-reading where PS5 ones were data-side only.)

 #  operation            PC method (TO COLLECT)                       | PS5 anchor IN HAND (provenance)
 --+---------------------+---------------------------------------------+------------------------------------------------
 1a GUObjectArray        count `lea rip->X` refs to the singleton; the | linked 0x93ba110 -> runtime 0x97ba110; VALIDATED
    singleton            PS5 census measured 1565 — a PC count in the | in-game; layout A, 71,959 objects, 2 chunks.
                         same order corroborates. Corroborate via     | StaticFindObject/FindObjectSlow NOT yet derived
                         FUObjectArray init constants + xrefs from    | on PS5 (PS5-side TO-DO the PC diff should inform).
                         FObjectInitializer/StaticAllocateObject.
 1b FNamePool / layout   UE5 FNamePool::Init interns "None",          | pool linked 0x9365ec0 -> runtime 0x9765ec0;
                         "ByteProperty", "IntProperty" right after    | core 0x242470, init 0x23a5d0, find 0x23c0b0,
                         pool init — find the 3 wide literals, xref   | add/probe 0x23c340; Blocks[0] @ pool+0x10,
                         the init fn; confirm stores to [pool+0x10],  | blocks pool+0x10040, ids pool+0x14040, shl-6
                         blocks pool+0x10040, ids pool+0x14040,       | 64-byte buckets. DERIVED-STATIC. Block size and
                         hash stride `shl 6`, block size in the       | offsets beyond +0x18 NOT enumerated on PS5 —
                         allocator call                               | the PC layout diff fills exactly this gap.
 2  FName ctor           wchar-in ctor census: length walk +          | ctor linked 0x23d590 -> runtime 0x63d590; bytes
    (mod's FName(name))  wide->narrow split + call into interning     | frozen 55 48 89 e5 53 50 48 89 f0 48 89 fb;
                         core; consensus windows (PS5 method: 3/4     | chain ->0x23d600->0x23e0d0->0x242470. DERIVED-
                         cluster windows picked 0x23d590 over         | STATIC. ABI DELTA: PC is Win64 x64 — FName
                         0x23d8a0)                                    | returns via hidden sret pointer in RCX, not RDI.
 3  StaticConstruct-     census FNAME_FIND-of-class-string sites,     | SCO linked 0x490aa0 -> runtime 0x890aa0,
    ObjectInternal /     follow winner into FObjectInitializer +      | RECORDED in-game; census 1,918 xrefs, call-target
    SpawnActor chain     StaticAllocateObject (corroborates 1a);      | consensus {0x490aa0, 0x493580, 0x493530}.
                         locate SetForSpawn + UWorld::SpawnActor      | SpawnActor/SetForSpawn NOT yet derived on PS5 —
                                                                      | PC comparison fills this.
 4  ProcessEvent /       UFunction lookup = GUA-walk + FName compare  | PS5: ProcessEvent address NOT derived — the
    CallFunction         + UFunction::Func deref; find via "Accessed  | widest PS5 gap. (R3 tier + vtable backend + PLH
                         None" script wrappers or Ghidra FID from     | layer are delivery machinery, unexecuted.)
                         the retained UE4SS.dll priors — verify per   | PC evidence sets the target semantic BEFORE any
                         build, never assume                          | PS5 probe spends budget.
 5  GMalloc / allocator  determine flavor FIRST: UTF-16 literals      | GMalloc linked 0x930add8 -> runtime 0x970add8
                         FMallocBinned3/2/Ansi (intake scan above     | (ARMED, launch-10); FMallocBinned3 instance
                         gives the first pass); Binned3 fingerprint   | 0x93089b8, vtable 0x7ec26e0 (RELA-reconstructed
                         constants movabs 0xc000000000/0x1000000000/  | — the step with NO PC analogue); exactly 1 store
                         0xbfc0000000/0x7fc0000000 + pool params      | / 11 consumer vtable thunks image-wide.
                         0x12c00000/0x10000; GMalloc global = unique  | DERIVED-STATIC-STRONG (worker-gmalloc-derivation).
                         bootstrap store; consumers = vtable thunks
                         (mov rdi,[rip+GM]; mov rax,[rdi]; call
                         [rax+0x30|0x40|0x50])

Cross-cutting correspondence metric per slot (protocol §5): (a) anchor identity, (b) neighborhood
shape, (c) census counts vs PS5 baselines {GUA 1565 lea refs; SCO 1,918 xrefs; GMalloc 11 thunks},
(d) derived address, (e) derivation-step parity. Classify every difference version-divergent vs
platform-divergent vs method-divergent per §2.2 — never merge the classes.

Pass-1 structural facts to record first (protocol §4, expected by construction): PE32+ vs ELF64
e_type 65040 no-section-table; PS5 had ZERO absolute data pointers (1,661,599 R_X86_64_RELATIVE at
DT_RELA file 0x932F430) vs PC .reloc+IAT with absolute pointers in .data/.rdata — so the PS5
RELA-vtable-reconstruction step has NO PC analogue: scan .data/.rdata for the vtable directly.

NEXT COMMANDS (run by the comparison owner after READY; scripts get written then — none exist yet,
honestly, because no PC image exists to develop them against):
  1. re-run this gate:            python3 <this script> <staging_dir>
  2. byte-exact promotion into    .superpowers/native-observations/townfall-pc-executable-<date>/
     + capture.json (protocol §2.3 shape; keep staging originals untouched)
  3. pass-1 structural diff (lief): PE headers/sections/imports(IAT)/exports/relocs vs the PS5
     ELF facts above
  4. ops 1a..5 in table order, each producing the §1 correspondence row:
     operation | PC evidence | PS5 evidence | difference | adaptation required
  5. outputs: CORRESPONDENCE.md + PC-PROFILE.json in the townfall-ps5-ue56.profile.json schema
     (protocol §6; same axes, so L2/L3 tooling consumes both identically)
"""


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="PC Townfall intake verifier (read-only on staging dir)")
    ap.add_argument("staging_dir")
    ap.add_argument("--json", dest="json_path", default=None,
                    help="write a machine receipt JSON to this path OUTSIDE the staging dir")
    args = ap.parse_args()

    sdir = os.path.abspath(args.staging_dir)
    run_utc = datetime.now(timezone.utc).isoformat()
    print("verify_intake.py — PC Townfall intake gate (read-only)")
    print("  protocol: outputs/2026-10-04-zcode-team/worker-re-pc-ps5-comparison/COMPARISON-PROTOCOL.md")
    print("  staging : %s" % sdir)
    print("  run_utc : %s" % run_utc)
    print("  lief    : %s" % (lief.__version__ if HAVE_LIEF else "NOT AVAILABLE — PE/ELF class checks degraded"))

    if not os.path.isdir(sdir):
        print("STATUS: NOT-READY — staging dir does not exist")
        sys.exit(2)

    names = sorted(n for n in os.listdir(sdir) if not n.startswith("."))
    files = []
    for n in names:
        p = os.path.join(sdir, n)
        if os.path.isfile(p):
            files.append((n, p))

    receipt = {}
    receipt_path = os.path.join(sdir, "capture.json")
    receipt_present = os.path.isfile(receipt_path)
    if receipt_present:
        try:
            with open(receipt_path) as f:
                receipt = json.load(f)
        except Exception as e:
            print("  WARNING: capture.json unreadable: %s" % e)

    print("")
    print("[1] PRESENCE (%d file(s); README.txt/capture.json excluded from evidence)" % len(files))
    executables, others, notes = [], [], []
    for n, p in files:
        if n == "README.txt":
            print("  %-28s placeholder (not evidence)" % n)
            continue
        if n == "capture.json":
            print("  %-28s receipt present" % n)
            continue
        size = os.path.getsize(p)
        fmt = sniff_format(p)
        ent = {"name": n, "bytes": size, "sha256": sha256_of(p), "sniff": fmt}
        if fmt == "PE":
            ent["info"] = pe_info(p)
            ent["strings"] = scan_binary(p)
            executables.append(ent)
        elif fmt == "ELF":
            ent["info"] = elf_info(p)
            executables.append(ent)
        else:
            ext = os.path.splitext(n)[1].lower()
            role = "manifest/pak candidate" if ext in (".pak", ".utoc", ".ucas", ".manifest", ".item", ".acf") else "unrecognized (no MZ/ELF magic)"
            ent["role"] = role
            others.append(ent)
        print("  %-28s %12d B  magic=%-7s sha256=%s…"
              % (n, size, fmt, ent["sha256"][:16]))
        if fmt != "PE" and fmt != "ELF":
            print("  %-28s -> %s" % ("", role))

    if receipt_present:
        print("")
        print("[2] CAPTURE.JSON receipt (protocol §2.3/§2.4 shape)")
        for k in REQUIRED_RECEIPT_FIELDS:
            v = receipt.get(k)
            print("  %-22s %s" % (k, v if v not in (None, "") else "MISSING (user must fill — see ACQUIRE.md)"))
        for k in OPTIONAL_RECEIPT_FIELDS:
            v = receipt.get(k)
            print("  %-22s %s" % (k, v if v not in (None, "") else "(optional, unfilled)"))
    else:
        print("")
        print("[2] CAPTURE.JSON receipt: ABSENT — template in ACQUIRE.md; user fields required: %s"
              % ", ".join(REQUIRED_RECEIPT_FIELDS))

    print("")
    print("[3] CLASSIFICATION (protocol §2.2 law: version-divergent vs platform-divergent, never merged)")
    if len(executables) > 1:
        print("  NOTE: %d PE/ELF executables staged; classifying the first parseable one, listing all." % len(executables))
    byte_mismatch = None
    if executables and isinstance(receipt.get("expected_bytes"), int):
        if executables[0]["bytes"] != receipt["expected_bytes"]:
            byte_mismatch = (receipt["expected_bytes"], executables[0]["bytes"])
    verdict_id, verdict_lines, ready, blockers, warnings = classify(executables, receipt, byte_mismatch)
    for l in verdict_lines:
        print(l)
    for w in warnings:
        print("  WARNING: %s" % w)

    print("")
    print("[4] VERDICT")
    if not executables and not others:
        print("  STATUS: NOT-READY — nothing staged yet (this is the honest empty state).")
        print("  Follow ACQUIRE.md; the moment files land, re-run this gate.")
        code = 1
    elif ready:
        print("  STATUS: READY — intake complete; the comparison op list below is unblocked.")
        code = 0
    else:
        print("  STATUS: NOT-READY — blockers: %s" % "; ".join(blockers))
        code = 1

    print("")
    print(OP_TABLE)

    if args.json_path:
        out = {
            "tool": "verify_intake.py",
            "run_utc": run_utc,
            "staging_dir": sdir,
            "lief": lief.__version__ if HAVE_LIEF else None,
            "files": executables + others,
            "receipt_present": receipt_present,
            "receipt": receipt,
            "verdict": verdict_id,
            "verdict_lines": verdict_lines,
            "ready": ready,
            "blockers": blockers,
            "warnings": warnings,
        }
        try:
            with open(args.json_path, "w") as f:
                json.dump(out, f, indent=2)
            print("machine receipt written: %s" % args.json_path)
        except Exception as e:
            print("WARNING: could not write --json receipt: %s" % e)

    sys.exit(code)


if __name__ == "__main__":
    main()
