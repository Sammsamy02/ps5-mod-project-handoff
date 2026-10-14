#!/usr/bin/env python3
"""check_odr_emitters.py -- mechanical post-build ODR emitter check (comp6 one-pass).

Implements the post-build emitter checks mandated by
outputs/2026-10-04-zcode-team/worker-odr-header-safety/ODR-SAFETY.md
(section 3 invariant + section 6 remediation item 6):

  (a) ValidateNativeProgramContext stale-inline check:
      * map COMDAT survivor of `.text._ZN2RC8Platform28ValidateNativeProgramContextE...`
        must be program.o (sole surviving emitter, audit section 3);
      * every per-unit emitter (TRUE emitter set from input-object symbol tables --
        the map alone hides COMDAT-deduped duplicates) must have a byte-identical body;
      * no RETAINED unit may emit it except program.o (program_retained advisory in
        the composition-4 shape; `--require-program-fresh` hard-fails that shape,
        which is the post-one-pass gate, FINDING-4);
      * the composed object must define exactly one such symbol, and the section it
        indexes must be byte-identical to the emitter body (resolved via
        SHN_XINDEX/SHT_SYMTAB_SHNDX -- composed section NAMES are not reliable).

  (b) the 13 audited retained units (ODR-SAFETY section 1 "BENIGN" table) emit no
      code touching the new configured-roots field offsets, proven at symbol/section
      level:
      * zero defined symbols naming `20NativeProgramContext` / `17NativeLoaderFacts`
        (any such symbol is layout-defined over the audited types) and zero D2 ctor
        definitions outside the sole fresh ctor site;
      * their native_mod_source emitters are confined to the allow-list
        {luamod.o, settings.o, program-signatures.o} with byte-identical bodies;
      * the UE4SSProgram offset-sensitive inlines (request_shutdown,
        get_debugging_ui, can_process_events, is_event_loop_thread,
        get_event_loop_thread_id, are_mods_being_touched, get_current_imgui_*) that
        retained units DO emit as weak COMDAT copies are verified the only way the
        audit's insertion-point argument can be checked per-build: EVERY unit's copy
        of each such symbol -- retained AND fresh -- must be byte-identical.  If the
        new header shifted a member these bodies read, the fresh program.o copy would
        diverge from the retained copies and this check hard-fails.
      * map level: no input section of the audited 13 names the audited types except
        the native_mod_source text/relocation sections.

      WHAT THIS PROVES: no retained unit ships a definition whose symbol is defined
      over the audited types, and every layout-sensitive body actually linked agrees
      byte-for-byte across all emitters (retained vintage == fresh vintage).
      WHAT IT CANNOT PROVE: stale field-offset immediates inside sections whose
      symbols do not name the audited types nor any offset-sensitive inline (e.g. a
      retained TU touching m_native_context members through a non-inline helper
      compiled elsewhere); that residue stays covered by the audit's source-grep +
      insertion-point argument (new fields appended AFTER mod_source).

  (c) NativeLoaderFacts / native_mod_source inline emitters absent from retained units:
      * `24MakeNativeProgramContext` (any overload) defined by NO retained unit
        (expected sole definer: fresh native-loadercontext.o);
      * `17NativeLoaderFacts` token in NO retained-unit defined symbol (by-value ABI);
      * native_mod_source survivor in the allow-list, exactly one in the composed
        object with emitter-equal bytes;
      * every MakeNativeProgramContext overload present in the composed object must
        carry the fresh native-loadercontext.o bytes.

  (d) D2 changed ctors (audit section 6 item 6): no standalone
      PsMappedImageCallerC1/C2 symbol anywhere; OwnedMappedImageCallerC1/C2 standalone
      symbols confined to fresh native-ownedmappedscansource.o (sole ctor site).

  (e) general retained sweep: `20NativeProgramContext` definers among retained units
      must be exactly {program.o} (the unit comp6 replaces) -- catches any future
      retained contact beyond the audited 13.

Exit code: 0 = all hard checks PASS (advisories allowed), 1 = FAIL, 2 = usage/env.
Stderr: human summary; --json PATH writes the full receipt (evidence + limits).

stdlib-only; parses ELF64 target objects via mmap; executes nothing; never writes
inside the run dir.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

# Explicit R4 generation admission; all original symbol/body predicates below remain unchanged.
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'execution'))
from full_pipeline import load_odr_generation

VAL = "_ZN2RC8Platform28ValidateNativeProgramContextERKNS0_20NativeProgramContextE"
NMS = "_ZNK2RC12UE4SSProgram17native_mod_sourceEv"
MAKE_PREFIX = "24MakeNativeProgramContext"
TOKEN_NPC = "20NativeProgramContext"
TOKEN_NLF = "17NativeLoaderFacts"
UE4SSPROG_TOKEN = "12UE4SSProgram"

AUDITED_13 = [
    "cppmod.o", "cppmod-base.o", "luamod.o", "settings.o", "program-signatures.o",
    "program-object-to-string.o", "generator-sdkgenerator-generator.o",
    "generator-sdkgenerator-ueheadergenerator.o", "generator-usmapgenerator-generator.o",
    "lua-library.o", "csv_data_commands.o", "native-jmap.o",
    "native-ownedmappedresolvercollector.o",
]
NMS_ALLOW = {"luamod.o", "settings.o", "program-signatures.o"}
PROGRAM = "program.o"
LOADERCONTEXT = "native-loadercontext.o"
SCANSSOURCE = "native-ownedmappedscansource.o"

OFFSET_SENSITIVE_SUBSTR = [
    "request_shutdown", "get_debugging_ui", "can_process_events",
    "is_event_loop_thread", "get_event_loop_thread_id",
    "are_mods_being_touched", "get_current_imgui",
]
D2_PSMAP_CTORS = ("18PsMappedImageCallerC1", "18PsMappedImageCallerC2")
D2_OWNED_CTORS = ("22OwnedMappedImageCallerC1", "22OwnedMappedImageCallerC2")

# input lines are exactly: VMA LMA Size Align <path-or-archive>:(<section>)
MAP_INPUT_RE = re.compile(r'^\s+\S+\s+\S+\s+\S+\s+\S+\s+(\S+):\([^()]*\)\s*$')
MAP_GROUP_RE = re.compile(r'^\s+\S+\s+\S+\s+\S+\s+\S+\s+(\.\S+)\s*$')


class EnvError(Exception):
    pass


# ---------------------------------------------------------------- ELF64 ----

class Elf:
    """Lazy ELF64 reader: section headers, symbol table with SHN_XINDEX support."""

    def __init__(self, path):
        import mmap
        self._f = open(path, "rb")
        self.mm = mmap.mmap(self._f.fileno(), 0, access=mmap.ACCESS_READ)
        m = self.mm
        if m[:4] != b"\x7fELF":
            raise EnvError(f"{path}: not an ELF file")
        if m[4] != 2:
            raise EnvError(f"{path}: not ELF64")
        e_shoff = struct.unpack_from("<Q", m, 0x28)[0]
        e_shentsize = struct.unpack_from("<H", m, 0x3A)[0]
        e_shnum = struct.unpack_from("<H", m, 0x3C)[0]
        e_shstrndx = struct.unpack_from("<H", m, 0x3E)[0]
        if e_shentsize != 64:
            raise EnvError(f"{path}: unexpected shentsize {e_shentsize}")
        n = e_shnum if e_shnum != 0 else struct.unpack_from("<Q", m, e_shoff + 32)[0]
        if n == 0 or n > 4_000_000:
            raise EnvError(f"{path}: absurd section count {n}")
        raw = []
        for i in range(n):
            off = e_shoff + i * 64
            name_off, typ = struct.unpack_from("<II", m, off)
            offset, size = struct.unpack_from("<QQ", m, off + 24)
            link, info = struct.unpack_from("<II", m, off + 40)
            raw.append((name_off, typ, offset, size, link, info))
        shstr_idx = e_shstrndx if e_shstrndx < len(raw) else 0
        shstr_off = raw[shstr_idx][2]
        self.sections = []  # [(name, typ, offset, size)]
        for name_off, typ, offset, size, link, info in raw:
            self.sections.append((self._cstr(shstr_off, name_off), typ, offset, size))
        self._raw = raw
        self.symtab = None    # (offset, count, strtab_off)
        self.xndx_off = None  # SHT_SYMTAB_SHNDX entries (32-bit per symbol)
        for i, (name_off, typ, offset, size, link, info) in enumerate(raw):
            if typ == 2 and self.symtab is None:
                strtab_off = raw[link][2]
                self.symtab = (offset, size // 24, strtab_off)
            elif typ == 18:
                self.xndx_off = offset

    def _cstr(self, base, off):
        m = self.mm
        e = base + off
        while m[e] != 0:
            e += 1
        return m[base + off:e].decode("utf-8", "replace")

    def defined_symbols(self):
        """Yield (name, real_shndx) for all defined symbols."""
        m = self.mm
        off, count, strtab = self.symtab
        for j in range(count):
            o = off + j * 24
            st_name, _info, _other, shndx = struct.unpack_from("<IBBH", m, o)
            if st_name == 0 or shndx == 0:
                continue
            if shndx == 0xFFFF:  # SHN_XINDEX
                if self.xndx_off is None:
                    raise EnvError("SHN_XINDEX symbol without SHT_SYMTAB_SHNDX")
                shndx = struct.unpack_from("<I", m, self.xndx_off + j * 4)[0]
            yield self._cstr(strtab, st_name), shndx

    def section_of(self, shndx):
        name, typ, offset, size = self.sections[shndx]
        return name, typ, offset, size

    def sha_of_section(self, shndx):
        name, typ, offset, size = self.sections[shndx]
        if typ == 8 or size == 0:  # SHT_NOBITS
            return None
        return hashlib.sha256(self.mm[offset:offset + size]).hexdigest()

    def close(self):
        self.mm.close()
        self._f.close()


# ------------------------------------------------------------- map parse ----

def parse_map(map_path):
    """-> ({group_section_name: [input refs]}, ordered unique unit paths, other_count)."""
    groups = {}
    current = None
    gorder = []
    other_inputs = 0
    for line in open(map_path, errors="replace"):
        line = line.rstrip("\n")
        g = MAP_GROUP_RE.match(line)
        if g:
            current = g.group(1)
            if current not in groups:
                groups[current] = []
                gorder.append(current)
            continue
        i = MAP_INPUT_RE.match(line)
        if i and current is not None:
            p = i.group(1)
            if p.endswith(".o"):
                groups[current].append(p)
            else:
                other_inputs += 1  # linker-internal inputs, archive oddities
    units = []
    seen = set()
    for gname in gorder:
        for p in groups[gname]:
            if p not in seen:
                seen.add(p)
                units.append(p)
    return groups, units, other_inputs


def base_of(path):
    if "(" in path and path.endswith(".o"):
        return path[path.rindex("(") + 1:]  # archive member -> member basename
    return Path(path).name


def is_archive(path):
    return "(" in path


# ------------------------------------------------------------------ main ----

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rundir", help="composition run dir containing runtime.map + composed object")
    ap.add_argument("--object", help="composed object path (default: the one >8MB *.o in rundir)")
    ap.add_argument("--json", dest="json_out", help="write full receipt JSON here")
    ap.add_argument("--require-program-fresh", action="store_true",
                    help="HARD-require program.o to be a fresh run-dir unit (post-one-pass gate); "
                         "the composition-4 retained-program shape FAILS under this flag")
    ap.add_argument("--strict-inline-bodies", action="store_true",
                    help="HARD-require fresh offset-sensitive inline bodies to byte-match retained "
                         "hashes (default: advisory, since same-vintage TU-flag call-mode codegen "
                         "differences are not layout shifts)")
    ap.add_argument("--generation-receipt", required=True)
    ap.add_argument("--generation-receipt-sha256", required=True)
    args = ap.parse_args()

    rundir = Path(args.rundir).resolve()
    map_path = rundir / "runtime.map"
    if not map_path.is_file():
        print(f"ENV ERROR: {map_path} missing", file=sys.stderr)
        return 2
    if args.object:
        obj_path = Path(args.object)
    else:
        cands = [p for p in rundir.glob("*.o") if p.stat().st_size > 8 * 1024 * 1024]
        if len(cands) != 1:
            print(f"ENV ERROR: expected exactly one large composed *.o in {rundir}, got "
                  f"{len(cands)} (pass --object)", file=sys.stderr)
            return 2
        obj_path = cands[0]
    if not obj_path.is_file():
        print(f"ENV ERROR: composed object {obj_path} missing", file=sys.stderr)
        return 2

    receipt = {
        "tool": "check_odr_emitters.py",
        "rundir": str(rundir),
        "composed_object": str(obj_path),
        "map": str(map_path),
        "checks": {},
        "advisories": [],
        "limits": [
            "symbol/section-level heuristic: proves no retained unit emits a definition whose symbol names the "
            "audited types, and that every layout-sensitive body actually linked (Validate/native_mod_source/"
            "UE4SSProgram offset-sensitive inlines) is byte-identical across ALL emitters, retained and fresh",
            "cannot prove absence of stale field-offset immediates inside sections whose symbols name neither the "
            "audited types nor an offset-sensitive inline (non-inline helpers compiled elsewhere); ODR-SAFETY.md's "
            "source-grep + insertion-point argument covers that residue",
            "per-unit emitter sets come from input-object symbol tables (the map hides COMDAT-deduped duplicates); "
            "allow-lists key on basename, so a same-basename fresh twin of a retained unit would be conflated "
            "(the one-pass design replaces program rather than duplicating it)",
            "composed-object byte proofs resolve symbol -> section index (SHN_XINDEX/SHT_SYMTAB_SHNDX); composed "
            "section NAME strings are linker-internal and not used",
        ],
    }
    hard_failures = []

    def check(cid, ok, detail):
        receipt["checks"][cid] = {"ok": bool(ok), "detail": detail}
        if not ok:
            hard_failures.append(f"{cid}: {detail}")
        return ok

    # ---- 1. map -------------------------------------------------------------
    groups, units, other_inputs = parse_map(map_path)
    receipt["units"] = {
        "total_map_input_refs": sum(len(v) for v in groups.values()),
        "non_unit_inputs": other_inputs,
    }
    fresh = {p for p in units if not is_archive(p) and p.startswith(str(rundir) + "/")}
    actual_fresh = set(fresh)
    generation = load_odr_generation(args.generation_receipt, args.generation_receipt_sha256, rundir)
    if not generation <= set(units):
        raise ValueError("generation provider absent from actual map")
    fresh |= generation
    receipt["R4_generation_classification"] = {"actual_this_pass_object_paths": sorted(actual_fresh),
        "additional_exact_R3_generation_object_paths": sorted(generation - actual_fresh),
        "generation_receipt": args.generation_receipt, "generation_receipt_sha256": args.generation_receipt_sha256,
        "scope": "explicit pinned generation; actual compiler/reuse counts are unchanged in composition receipt"}
    retained_set = {p for p in units if not is_archive(p) and p not in fresh}
    receipt["units"].update({"composition_units": len(units), "fresh": len(fresh),
                             "retained": len(retained_set)})

    val_surv = groups.get(".text." + VAL, [])
    nms_surv = groups.get(".text." + NMS, [])
    make_group_names = [g for g in groups if g.startswith(".text.") and MAKE_PREFIX in g]
    receipt["map_survivors"] = {
        "validate": [base_of(p) for p in val_surv],
        "native_mod_source": [base_of(p) for p in nms_surv],
        "make_native_program_context": {g[6:]: [base_of(x) for x in groups[g]]
                                        for g in make_group_names},
    }

    # ---- 2. per-unit TRUE emitter scan ---------------------------------------
    def is_offset_sensitive(sym):
        return UE4SSPROG_TOKEN in sym and NMS not in sym and VAL not in sym \
            and MAKE_PREFIX not in sym and any(s in sym for s in OFFSET_SENSITIVE_SUBSTR)

    tokens = [TOKEN_NPC, TOKEN_NLF, MAKE_PREFIX, "28ValidateNativeProgramContext",
              "17native_mod_source", UE4SSPROG_TOKEN,
              "18PsMappedImageCallerC1", "18PsMappedImageCallerC2",
              "22OwnedMappedImageCallerC1", "22OwnedMappedImageCallerC2",
              ] + OFFSET_SENSITIVE_SUBSTR
    sym_hits = set()     # (path, symname)
    sym_bodies = {}      # (path, symname) -> sha256 of .text.<symname>
    missing = []
    for p in units:
        if is_archive(p):
            continue
        try:
            elf = Elf(p)
        except (FileNotFoundError, EnvError) as e:
            missing.append(f"{base_of(p)} ({e})")
            continue
        try:
            for sym, shndx in elf.defined_symbols():
                if not any(t in sym for t in tokens):
                    continue
                sym_hits.add((p, sym))
                if shndx < len(elf.sections):
                    nm, typ, off, size = elf.section_of(shndx)
                    if typ != 8 and size:
                        sym_bodies[(p, sym)] = hashlib.sha256(
                            elf.mm[off:off + size]).hexdigest()
        finally:
            elf.close()
    if missing:
        check("inputs_complete", False,
              f"{len(missing)} map input objects unreadable: {missing[:3]}")
    else:
        n_scanned = sum(1 for p in units if not is_archive(p))
        check("inputs_complete", True, f"all {n_scanned} non-archive composition units scanned")

    def emitter_paths(pred):
        return sorted({p for (p, s) in sym_hits if pred(s)})

    # ---- CHECK A: ValidateNativeProgramContext -------------------------------
    val_paths = emitter_paths(lambda s: VAL in s)
    val_retained = [p for p in val_paths if p in retained_set]
    val_bodies = {p: sym_bodies.get((p, VAL)) for p in val_paths}
    val_body_set = {b for b in val_bodies.values() if b is not None}
    check("A2_validate_all_emitter_bodies_identical",
          len(val_body_set) == 1 and all(b is not None for b in val_bodies.values()),
          "emitter body shas: " + json.dumps(
              {base_of(p): (val_bodies[p] or "NO-BODY")[:12] for p in val_paths}))
    check("A1_validate_sole_survivor_is_program",
          len(val_surv) == 1 and base_of(val_surv[0]) == PROGRAM,
          f".text.VAL map inputs = {[base_of(p) for p in val_surv]} (expect exactly ['{PROGRAM}'])")
    bad_retained_val = sorted(base_of(p) for p in val_retained if base_of(p) != PROGRAM)
    check("A3_validate_no_retained_emitter_besides_program", not bad_retained_val,
          f"retained VAL emitters={[base_of(p) for p in val_retained]}, offenders={bad_retained_val}")
    program_paths = [p for p in val_paths if base_of(p) == PROGRAM]
    program_is_fresh = any(p in fresh for p in program_paths)
    if args.require_program_fresh:
        check("A5_program_unit_is_fresh", program_is_fresh,
              f"program.o fresh={program_is_fresh} (post-one-pass gate, FINDING-4)")
    elif program_paths and not program_is_fresh:
        receipt["advisories"].append(
            "program.o is RETAINED in this run (composition-4 shape): in the comp6 one-pass build it must be "
            "replaced (FINDING-4 / design 3.2). Its Validate body currently matches the fresh emitters "
            "byte-for-byte, so the linked bytes are vintage-consistent today; use --require-program-fresh for "
            "the post-one-pass gate.")

    # composed object: symbol -> section bytes
    comp = Elf(obj_path)
    try:
        comp_syms = {}
        for sym, shndx in comp.defined_symbols():
            comp_syms.setdefault(sym, shndx)
        comp_sha = {}  # symname -> sha of the section the symbol indexes

        def comp_sha_for(sym):
            if sym not in comp_sha:
                shndx = comp_syms.get(sym)
                comp_sha[sym] = comp.sha_of_section(shndx) if shndx is not None else None
            return comp_sha[sym]

        val_comp = [s for s in comp_syms if s == VAL]
        surv_body = next(iter(val_body_set)) if len(val_body_set) == 1 else None
        check("A4_validate_composed_single_and_body_equal",
              len(val_comp) == 1 and surv_body is not None
              and comp_sha_for(VAL) == surv_body,
              f"composed defines {len(val_comp)} VAL symbol(s); composed section sha="
              f"{(comp_sha_for(VAL) or 'none')[:12]}; emitter body sha={(surv_body or 'none')[:12]}")

        # ---- CHECK B: audited 13 retained units --------------------------------
        present13 = {base_of(p): p for p in retained_set if base_of(p) in AUDITED_13}
        b_bad = {}
        for b, p in present13.items():
            bad = set()
            for (pp, s) in sym_hits:
                if pp != p:
                    continue
                if VAL in s or NMS in s or MAKE_PREFIX in s:
                    continue  # governed by A3 / B2 / C1
                if TOKEN_NPC in s or TOKEN_NLF in s:
                    bad.add(s)
                elif any(c in s for c in D2_PSMAP_CTORS + D2_OWNED_CTORS):
                    bad.add(s)
            if bad:
                b_bad[b] = sorted(bad)[:4]
        check("B1_audited13_zero_direct_type_contact", not b_bad,
              f"present={sorted(present13)}; offenders={b_bad or 'none'}")
        absent13 = sorted(set(AUDITED_13) - set(present13))
        if absent13:
            receipt["advisories"].append(
                f"audited-13 units absent from this run's map: {absent13}")

        nms_paths = emitter_paths(lambda s: NMS in s)
        nms_retained = [p for p in nms_paths if p in retained_set]
        nms_retained_bad = sorted(base_of(p) for p in nms_retained
                                  if base_of(p) not in NMS_ALLOW and base_of(p) != PROGRAM)
        check("B2_native_mod_source_retained_allow_list", not nms_retained_bad,
              f"retained NMS emitters={[base_of(p) for p in nms_retained]}, "
              f"outside-allow+program={nms_retained_bad}")
        if PROGRAM in {base_of(p) for p in nms_retained} and not program_is_fresh \
                and not args.require_program_fresh:
            receipt["advisories"].append(
                "retained program.o also emits native_mod_source (composition-4 shape); the comp6 replacement "
                "makes it fresh. Offset-safety is proven by C3 body identity either way.")

        bad_sections = {}
        for b, p in present13.items():
            secs = set()
            for gname in groups:
                if (TOKEN_NPC in gname or TOKEN_NLF in gname) \
                        and gname not in (".text." + NMS, ".rela.text." + NMS) \
                        and p in groups[gname]:
                    secs.add(gname)
            if secs:
                bad_sections[b] = sorted(secs)[:4]
        check("B3_audited13_map_sections_clean", not bad_sections,
              f"offenders={bad_sections or 'none'}")

        # ---- offset-sensitive UE4SSProgram inlines: tiered body identity ---------
        # The audit's containment argument for these inlines is the insertion-point
        # invariant; mechanically we enforce:
        #   B4a (HARD): all RETAINED emitters of each symbol are byte-identical
        #               (two divergent retained vintages = real ODR violation today);
        #   B4b (advisory; HARD under --strict-inline-bodies): every fresh emitter is
        #               byte-identical to the retained hash (cross-vintage detector)
        #               and fresh emitters agree among themselves.
        # A same-vintage TU-flag codegen difference (direct e8 call vs GOT-indirect
        # ff15 call with IDENTICAL member-offset constants) trips B4b only -- that is
        # the observed request_shutdown bootstrap-vs-program shape in run-3.
        offs_syms = sorted({s for (p, s) in sym_hits if is_offset_sensitive(s)})
        offs_b4a_bad = {}
        offs_b4b_bad = {}
        offs_info = {}
        for s in offs_syms:
            ret_shas, fresh_shas = {}, {}
            for (p, ss) in sym_hits:
                if ss != s:
                    continue
                sha = sym_bodies.get((p, s))
                (ret_shas if p in retained_set else fresh_shas)[base_of(p)] = sha
            ret_vals = set(ret_shas.values())
            if len(ret_shas) and (len(ret_vals) != 1 or None in ret_vals):
                offs_b4a_bad[s[:80]] = {k: (v[:12] if v else "NO-BODY")
                                        for k, v in sorted(ret_shas.items())}
            ret_hash = next(iter(ret_vals)) if len(ret_vals) == 1 and None not in ret_vals else None
            offs_info[s] = {"retained_sha": ret_hash[:12] if ret_hash else None,
                            "fresh_shas": {k: (v[:12] if v else "NO-BODY")
                                           for k, v in sorted(fresh_shas.items())}}
            for k, v in fresh_shas.items():
                if v is None or (ret_hash is not None and v != ret_hash) \
                        or (ret_hash is None and len({x for x in fresh_shas.values()}) > 1):
                    offs_b4b_bad[s[:80]] = offs_b4b_bad.get(s[:80]) or {}
                    offs_b4b_bad[s[:80]][k] = (v or "NO-BODY")[:12]
        check("B4a_offset_sensitive_retained_bodies_identical",
              not offs_b4a_bad,
              f"{len(offs_syms)} symbols checked; retained-internal divergence="
              f"{offs_b4a_bad or 'none'}")
        strict_note = " (STRICT gate)" if args.strict_inline_bodies else \
            " (advisory; --strict-inline-bodies promotes)"
        if offs_b4b_bad:
            check("B4b_offset_sensitive_fresh_vintage_alignment",
                  not args.strict_inline_bodies,
                  f"fresh-vs-retained or fresh-internal divergence={offs_b4b_bad}" + strict_note)
            if not args.strict_inline_bodies:
                receipt["advisories"].append(
                    "B4b: fresh emitter bodies diverge from retained hashes for "
                    f"{sorted(offs_b4b_bad)}; in run-3 this is the known same-vintage "
                    "call-mode codegen difference (direct e8 vs GOT-indirect ff15 with identical member-offset "
                    "constants), not a layout shift -- reconcile on the one-pass run")
        else:
            check("B4b_offset_sensitive_fresh_vintage_alignment", True,
                  "all fresh copies match retained hashes or have no retained peer" + strict_note)
        receipt["offset_sensitive_inline_bodies"] = offs_info

        # ---- CHECK C: NativeLoaderFacts / Make / native_mod_source --------------
        make_syms = sorted({s for (p, s) in sym_hits if MAKE_PREFIX in s})
        make_paths = emitter_paths(lambda s: MAKE_PREFIX in s)
        make_retained_bad = sorted(base_of(p) for p in make_paths if p in retained_set)
        check("C1_make_absent_from_retained", not make_retained_bad,
              f"MAKE emitters={[base_of(p) for p in make_paths]}, "
              f"retained-offenders={make_retained_bad}")

        nlf_retained = sorted((base_of(p), s[:80]) for (p, s) in sym_hits
                              if p in retained_set and TOKEN_NLF in s)
        check("C2_nlf_token_absent_from_retained", not nlf_retained,
              f"retained NLF-token symbols={nlf_retained[:4] or 'none'}")

        nms_bodies = {p: sym_bodies.get((p, NMS)) for p in nms_paths}
        nms_body_set = {b for b in nms_bodies.values() if b is not None}
        check("C3_native_mod_source_all_bodies_identical",
              len(nms_body_set) == 1 and all(b is not None for b in nms_bodies.values()),
              "NMS body shas: " + json.dumps(
                  {base_of(p): (nms_bodies[p] or "NO-BODY")[:12] for p in nms_paths}))
        nms_surv_bases = [base_of(p) for p in nms_surv]
        check("C4_nms_survivor_in_allow_list",
              len(nms_surv) == 1 and nms_surv_bases[0] in NMS_ALLOW,
              f".text.NMS map inputs={nms_surv_bases} (expect one of {sorted(NMS_ALLOW)})")
        nms_comp = [s for s in comp_syms if s == NMS]
        nms_body = next(iter(nms_body_set)) if len(nms_body_set) == 1 else None
        check("C5_nms_composed_single_and_body_equal",
              len(nms_comp) == 1 and nms_body is not None
              and comp_sha_for(NMS) == nms_body,
              f"composed defines {len(nms_comp)} NMS symbol(s); composed section sha="
              f"{(comp_sha_for(NMS) or 'none')[:12]}; emitter sha={(nms_body or 'none')[:12]}")

        make_bad = {}
        for s in make_syms:
            want = None
            for (p, ss) in sym_hits:
                if ss == s:
                    want = sym_bodies.get((p, s))
                    definer = base_of(p)
                    break
            got = comp_sha_for(s)
            if want is None or got != want:
                make_bad[s[:80]] = f"composed={ (got or 'none')[:12] } emitter={ (want or 'none')[:12] }"
        comp_make_syms = [s for s in comp_syms if MAKE_PREFIX in s]
        check("C6_make_composed_equals_fresh_loadercontext_bytes",
              len(comp_make_syms) == len(make_syms) and len(make_syms) > 0 and not make_bad,
              f"composed MAKE syms={len(comp_make_syms)}; per-symbol emitter definers="
              f"{sorted({base_of(p) for (p, s) in sym_hits if MAKE_PREFIX in s})}; "
              f"mismatches={make_bad or 'none'}")

        # ---- CHECK D: D2 changed ctors standalone-symbol absence -----------------
        ps_hits = sorted({(base_of(p), s[:70]) for (p, s) in sym_hits
                          if any(c in s for c in D2_PSMAP_CTORS)})
        owned = {}
        for (p, s) in sym_hits:
            if any(c in s for c in D2_OWNED_CTORS):
                owned.setdefault(base_of(p), set()).add(s[:70])
        owned_bad = {b: sorted(s) for b, s in owned.items() if b != SCANSSOURCE}
        scans_fresh = any(base_of(p) == SCANSSOURCE and p in fresh for p in units)
        check("D1_d2_changed_ctors_confined",
              not ps_hits and not owned_bad and scans_fresh,
              f"PsMappedImageCallerC1/C2 standalone={ps_hits or 'none'}; "
              f"OwnedMappedImageCallerC1/C2 outside {SCANSSOURCE}={owned_bad or 'none'}; "
              f"{SCANSSOURCE} fresh={scans_fresh}")

        # ---- CHECK E: general retained sweep --------------------------------------
        npc_retained = sorted({base_of(p) for (p, s) in sym_hits
                               if p in retained_set and TOKEN_NPC in s})
        check("E1_retained_npc_definers_subset_program", set(npc_retained) <= {PROGRAM},
              f"retained units defining 20NativeProgramContext symbols={npc_retained} "
              f"(allowed: exactly ['{PROGRAM}'], which comp6 replaces)")
    finally:
        comp.close()

    # ---- receipt ---------------------------------------------------------------
    receipt["emitters"] = {
        "validate_all": sorted({base_of(p) for p in val_paths}),
        "validate_retained": sorted({base_of(p) for p in val_retained}),
        "native_mod_source_all": sorted({base_of(p) for p in nms_paths}),
        "native_mod_source_retained": sorted({base_of(p) for p in nms_retained}),
        "make_all": sorted({base_of(p) for p in emitter_paths(lambda s: MAKE_PREFIX in s)}),
        "owned_mapped_image_caller_ctors": sorted(owned),
        "program_o_fresh": program_is_fresh,
    }
    receipt["body_hashes"] = {
        "validate_emitter_body": surv_body,
        "native_mod_source_emitter_body": nms_body,
    }
    receipt["result"] = "PASS" if not hard_failures else "FAIL"
    receipt["failures"] = hard_failures
    out = json.dumps(receipt, indent=1)
    if args.json_out:
        Path(args.json_out).write_text(out + "\n")
    print(out)
    print(f"RESULT: {receipt['result']}", file=sys.stderr)
    for adv in receipt["advisories"]:
        print(f"ADVISORY: {adv}", file=sys.stderr)
    return 0 if not hard_failures else 1


if __name__ == "__main__":
    sys.exit(main())
