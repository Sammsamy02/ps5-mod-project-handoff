"""Render the per-game runtime PROFILE FILE (Team S1 ``PROFILE-FORMAT.md``, readiness item B4).

This is the one data file the generic PS5 runtime loads instead of the ~11 compiled-in per-game
headers (READINESS sections 2 A-D).  It is produced from the finished ``runtime-profile.json``
document (never from detector internals) plus the decrypted executable, so every value is the one
the detectors measured on the image; nothing is remembered from any game and no value is hand-typed.

What this module adds on top of the JSON the detectors already produce:

* **GMalloc for THIS runtime.**  The runtime's V1/V2 validators seed the *base allocator slot*
  (``symbols.gmalloc_base_slot``), not the dispatch variable (``symbols.gmalloc``) that
  ``FMemory::Malloc``/``Free`` dereference.  ``[seeds] gmalloc_label`` is therefore taken from the
  base slot by default (``--profile-gmalloc`` can override), which is the gap READINESS B5 flags.
* **GMalloc vtable, second channel.**  The instance constructor installs the allocator vtable with a
  ``lea rax,[rip+V]; mov [this+0], rax``; the *last* such store is the live vtable, corroborated by V
  being the start of a run of ``R_X86_64_RELATIVE`` relocations whose addends are executable (a real
  Itanium vtable).  This fills ``gmalloc_vtable_label``, which the JSON path leaves unresolved.
* **Exact-length code-pin prefixes.**  The runtime pins a fixed number of prologue bytes per code
  symbol (FName ctor 32, StaticConstructObject 25, delete-listener ops 16, thread-name accessor 33).
  The bytes are read straight from the authenticated image at the resolved label, so the prefix is as
  long as the pin needs even where the detector only recorded a shorter prologue.

Everything the detectors could not resolve generically (delete-listener ops, the thread-name accessor,
the pak-mount set, the ``LoadMap``/``InitGameState``/``CopyValuesInternal`` vtable slots and the full
``FMalloc`` slot list) is written as **absent** with its reason, and the matching ``[features]`` flag
is turned off, so the runtime switches that one feature off and still arms the rest.  A required value
that is missing makes ``required_set_complete`` false, which the runtime treats as a loud refusal
(fail closed).  Nothing is ever guessed to fill a gap.

    python3 -m tools.profilegen --eboot ... --param ... --out DIR --emit-profile DIR/profile.ini
    python3 -m tools.profilegen.emit_profile --profile DIR/runtime-profile.json --eboot ... --out profile.ini
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

from .emit_ue4ss import leaf, render_member_layout, render_vtable_layout, usable
from .elf import ElfImage

SCHEMA = 1
MIN_CONFIDENCE = "medium"
_RANK = {"low": 0, "medium": 1, "high": 2}
TARGET = "prospero-x86_64-sysv"            # PS5 ABI, a platform constant (not per-game)
BODY_SHA_KEY = "body_sha256"
# a profile must never carry a console-verification claim; the generator only ever says "generated"
FORBIDDEN_MARKER = re.compile(r"console[-_ ]?verified|verification", re.IGNORECASE)
PROFILE_GMALLOC_CHOICES = ("base_slot", "dispatch")

# runtime code-pin prefix lengths (ABI pin sizes, generic; the runtime reads exactly this many bytes)
PREFIX_BYTES = {"fname_ctor": 32, "sco": 25, "listener_add": 16, "listener_remove": 16, "thread_name": 33}

# fixed structural pak offsets the format also range-checks (engine-generic, not image addresses)
PAK_FIXED = {"slot_get_lower_level": 0x68, "slot_delegate_execute": 0x60,
             "delegate_inst_off": 0, "delegate_size_off": 8, "pak_count_offset": 0x18}


class ProfileRefused(ValueError):
    """The profile file cannot be produced (a core identity fact is missing or a marker would be written)."""


class ProfileOptions:
    def __init__(self, gmalloc: Optional[str] = None, exe_path: Optional[str] = None,
                 name: Optional[str] = None) -> None:
        if gmalloc is not None and gmalloc not in PROFILE_GMALLOC_CHOICES:
            raise ProfileRefused("--profile-gmalloc must be one of %s" % "|".join(PROFILE_GMALLOC_CHOICES))
        self.gmalloc = gmalloc or "base_slot"      # THIS runtime seeds the base slot (READINESS B5)
        self.gmalloc_explicit = gmalloc is not None
        self.exe_path = exe_path                    # console path of the readable copy (B8 / Team S2); not derivable here
        self.name = name


# ---------------------------------------------------------------------------------------------
def _num(env: Dict[str, Any]) -> int:
    v = env["value"]
    return int(v, 16) if isinstance(v, str) else int(v)


def _hx(n: int) -> str:
    return "0x%x" % n


def _val(doc: Dict[str, Any], path: str, minimum: str = MIN_CONFIDENCE) -> Optional[int]:
    env = leaf(doc, path)
    return _num(env) if usable(env, minimum) else None


def _str(doc: Dict[str, Any], path: str) -> Optional[str]:
    env = leaf(doc, path)
    if env is not None and isinstance(env.get("value"), str) and env["value"]:
        return env["value"]
    return None


class Field:
    """One emitted key: present (a value) or absent (a reason)."""
    __slots__ = ("key", "value", "present", "reason", "confidence")

    def __init__(self, key: str, value: Any = None, present: bool = False,
                 reason: str = "", confidence: str = "") -> None:
        self.key, self.value, self.present, self.reason, self.confidence = key, value, present, reason, confidence


class ProfileModel:
    def __init__(self) -> None:
        self.sections: "list[Tuple[str, List[str]]]" = []     # (section name, raw lines)
        self.features: Dict[str, bool] = {}
        self.absent: List[Tuple[str, str]] = []               # (field, reason) -> GAPS.md
        self.notes: List[str] = []
        self.required_complete = True


# ---------------------------------------------------------------------------------------------
def derive_gmalloc_vtable(elf: ElfImage, ctor_label: int) -> Optional[Tuple[int, str]]:
    """The allocator vtable the instance ctor installs: the LAST ``lea rax,[rip+V]; mov [this+0],rax``
    whose V begins a run of RELATIVE relocations with executable addends.  (label, confidence) or None."""
    try:
        from capstone import x86 as xo
        from capstone import x86_const as xc
        from .index import Decoder
    except Exception:
        return None
    try:
        dec = Decoder(elf)
    except Exception:
        return None
    ins = dec.decode(ctor_label, 0x600)
    base_regs = {"rdi"}                       # the object being constructed arrives in rdi
    lea: Dict[str, int] = {}
    cands: List[int] = []
    for i in ins:
        mn, ops = i.mnemonic, i.operands
        if mn == "ret":
            break
        if mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_REG \
                and dec.reg_name(ops[1].reg) in base_regs:
            base_regs.add(dec.reg_name(ops[0].reg))
        elif mn == "lea" and len(ops) == 2 and ops[0].type == xo.X86_OP_REG and ops[1].type == xo.X86_OP_MEM \
                and ops[1].mem.base == xc.X86_REG_RIP:
            lea[dec.reg_name(ops[0].reg)] = i.address + i.size + ops[1].mem.disp
        elif mn == "mov" and len(ops) == 2 and ops[0].type == xo.X86_OP_MEM and ops[1].type == xo.X86_OP_REG:
            m = ops[0].mem
            r = dec.reg_name(ops[1].reg)
            if m.base and dec.reg_name(m.base) in base_regs and m.index == 0 and m.disp == 0 and r in lea:
                cands.append(lea[r])
    if not cands:
        return None
    v = cands[-1]
    if not elf.is_mapped(v) or elf.is_exec(v):              # a vtable is a mapped, non-executable pointer table
        return None                                          # (on PS5 it sits in a RELRO data segment)
    run = _relative_run(elf, v)
    if run >= 8:
        return v, "high"                                     # two channels: ctor install + RELATIVE vtable run
    if run >= 1:
        return v, "medium"                                   # one structural channel (the ctor install only)
    return None


def _relative_run(elf: ElfImage, v: int) -> int:
    """Length of the run of consecutive 8-byte RELATIVE relocations at ``v`` whose addends are executable."""
    import bisect
    offs, adds = elf.relative_relocs()
    ex = [(s.vaddr, s.file_end_va) for s in elf.exec_segments]
    isx = lambda a: any(lo <= a < hi for lo, hi in ex)
    idx = bisect.bisect_left(offs, v)
    run, o = 0, v
    while idx < len(offs) and offs[idx] == o and isx(adds[idx]):
        run += 1
        o += 8
        idx += 1
    return run


def _prefix(elf: ElfImage, label: Optional[int], n: int) -> Optional[str]:
    if label is None:
        return None
    data = elf.read(label, n)
    return data.hex() if data is not None and len(data) == n else None


def _synth_name(doc: Dict[str, Any], opts: ProfileOptions) -> str:
    if opts.name:
        return opts.name
    title = _str(doc, "binding.title_id") or "game"
    maj = _val(doc, "build.engine.major") or 0
    mnr = _val(doc, "build.engine.minor")
    cfg = _str(doc, "build.configuration") or "config"
    st = leaf(doc, "build.stats")
    stats = ("stats-on" if st and st.get("value") else "stats-off") if st is not None else "stats-unknown"
    ver = "ue%d%s" % (maj, "" if mnr is None else str(mnr))
    return "%s-prospero-%s-%s-%s" % (title, ver, cfg, stats)


def _build_source(doc: Dict[str, Any], path: str, label: str) -> str:
    """A non-empty provenance string from the detector's own method/evidence (presence-only in the runtime)."""
    env = leaf(doc, path)
    if env is None:
        return label + ":unresolved"
    ev = "; ".join(str(e) for e in env.get("evidence", [])[:2])
    return ("%s:%s(%s) %s" % (label, env.get("value"), env.get("method"), ev)).strip()


# ---------------------------------------------------------------------------------------------
def build(doc: Dict[str, Any], elf: ElfImage, opts: Optional[ProfileOptions] = None) -> ProfileModel:
    opts = opts or ProfileOptions()
    m = ProfileModel()

    # ---- identity ---------------------------------------------------------------------------
    sha = _str(doc, "binding.executable.sha256")
    if sha is None or not re.fullmatch(r"[0-9a-f]{64}", sha):
        raise ProfileRefused("the profile needs a 64-hex image sha256 (binding.executable.sha256)")
    size = _val(doc, "binding.executable.bytes", "low")
    extent = _val(doc, "load.image_extent.vaddr_end", "low")
    base = _val(doc, "load.base", "low")
    digest = str(doc.get("profile_digest") or "")
    if not re.fullmatch(r"[0-9a-f]{64}", digest):
        raise ProfileRefused("the profile has no evidence digest (profile_digest)")
    for need, label in ((size, "image_size"), (extent, "image_extent"), (base, "image_base")):
        if need is None:
            raise ProfileRefused("the profile is missing the required identity value %s" % label)

    prof = ["schema=%d" % SCHEMA, "name=%s" % _synth_name(doc, opts), "digest=%s" % digest,
            "%s=__BODY__" % BODY_SHA_KEY, "image_sha256=%s" % sha, "image_size=%d" % size,
            "image_extent=%s" % _hx(extent), "image_base=%s" % _hx(base)]
    if opts.exe_path:
        prof.append("exe_path=%s" % opts.exe_path)
    else:
        prof.append("; exe_path NOT emitted: the console path of the readable executable copy is Team S2 / B8"
                    " (paused for the user's decision); the runtime takes it from config")
        m.absent.append(("profile.exe_path", "console path of the readable copy (B8, paused for user); not derivable from the image"))
    m.sections.append(("profile", prof))

    # ---- build ------------------------------------------------------------------------------
    maj = _val(doc, "build.engine.major")
    mnr = _val(doc, "build.engine.minor")
    rev = _str(doc, "build.engine.revision_string")
    bid = _str(doc, "build.build_id")
    cfg_env = leaf(doc, "build.configuration")
    st_env = leaf(doc, "build.stats")
    cfg_map = {"shipping": 1, "development": 2, "test": 3}
    if maj is None or mnr is None or rev is None or bid is None or not usable(cfg_env) or not usable(st_env):
        raise ProfileRefused("the profile is missing a required [build] identity value (version/revision/build id/config/stats)")
    build_configuration = cfg_map.get(cfg_env["value"])
    if build_configuration is None:
        raise ProfileRefused("unknown build configuration %r" % cfg_env["value"])
    stats = 2 if st_env["value"] else 1
    fname_case = leaf(doc, "symbols.fname_ctor_wchar.case_preserving")
    case_preserving = 1 if (fname_case and fname_case.get("value")) else 0
    fname_bytes = _val(doc, "structs.FName.size", "low") or 8
    build_lines = [
        "format=1", "pointer_size=8",
        "engine_major=%d" % maj, "engine_minor=%d" % mnr, "target=%s" % TARGET,
        "engine_revision=%s" % rev, "build_id=%s" % bid,
        "build_configuration=%d" % build_configuration,
        "build_source=%s" % _build_source(doc, "build.configuration", "measured-config"),
        "stats=%d" % stats,
        "stats_source=%s" % _build_source(doc, "build.stats", "measured-stats"),
        "non_shipping=%d" % (0 if build_configuration == 1 else 1),
        "stats_layout=%d" % (1 if stats == 2 else 0),
        "fname_case_preserving=%d" % case_preserving,
        "fname_output_bytes=%d" % fname_bytes, "fname_output_alignment=8",
    ]
    m.sections.append(("build", build_lines))

    # ---- pool -------------------------------------------------------------------------------
    pool_map = [("current_block_offset", "structs.FNamePool.current_block"),
                ("current_byte_offset_offset", "structs.FNamePool.current_byte_offset"),
                ("blocks_offset", "structs.FNamePool.blocks"),
                ("none_id_offset", "structs.FNamePool.none_id"),
                ("byte_property_id_offset", "structs.FNamePool.byte_property_id"),
                ("block_offset_bits", "structs.FNamePool.block_offset_bits"),
                ("max_blocks", "structs.FNamePool.max_blocks")]
    pool_lines = []
    for key, path in pool_map:
        v = _val(doc, path, "low")
        if v is None:
            raise ProfileRefused("the profile is missing the required pool constant %s (%s)" % (key, path))
        pool_lines.append("%s=%s" % (key, _hx(v)))
    m.sections.append(("pool", pool_lines))

    # ---- seeds ------------------------------------------------------------------------------
    gmalloc_src = {"base_slot": "symbols.gmalloc_base_slot", "dispatch": "symbols.gmalloc"}[opts.gmalloc]
    seed_specs = [("fname_pool_label", "symbols.name_pool", True),
                  ("guobject_array_label", "symbols.object_array", True),
                  ("fname_ctor_label", "symbols.fname_ctor_wchar", True),
                  ("gmalloc_label", gmalloc_src, True),
                  ("gmalloc_instance_label", "symbols.gmalloc_instance", False),
                  ("sco_label", "symbols.static_construct_object_internal", True)]
    seed_lines = ["generated=1", "__RSC__"]
    present = {}
    for key, sym, _req in seed_specs:
        v = _val(doc, sym + ".label")
        present[key] = v is not None
        if v is not None:
            seed_lines.append("%s=%s" % (key, _hx(v)))
        else:
            seed_lines.append("; %s ABSENT: %s.label is unresolved or below %s confidence" % (key, sym, MIN_CONFIDENCE))
            m.absent.append(("seeds.%s" % key, "%s.label not resolved at >= %s" % (sym, MIN_CONFIDENCE)))

    # gmalloc vtable (second channel: instance-ctor install + RELATIVE vtable run)
    ctor = _val(doc, "symbols.gmalloc_instance_ctor.label")
    vt = derive_gmalloc_vtable(elf, ctor) if ctor is not None else None
    if vt is not None:
        seed_lines.append("gmalloc_vtable_label=%s" % _hx(vt[0]))
        m.notes.append("gmalloc_vtable_label=%s derived from the instance ctor's vtable install (%s)" % (_hx(vt[0]), vt[1]))
        present["gmalloc_vtable_label"] = True
    else:
        seed_lines.append("; gmalloc_vtable_label ABSENT: the instance constructor's vtable install could not be decoded")
        m.absent.append(("seeds.gmalloc_vtable_label", "instance-ctor vtable install not decodable; no second channel"))
        present["gmalloc_vtable_label"] = False

    # presence / derivation flags for the 4 arming slots
    slot_flags = {"guobject_array": "symbols.object_array", "fname_ctor": "symbols.fname_ctor_wchar",
                  "gmalloc": gmalloc_src, "sco": "symbols.static_construct_object_internal"}
    for slot, sym in slot_flags.items():
        env = leaf(doc, sym + ".label")
        ok = usable(env)
        seed_lines.append("%s_present=%d" % (slot, 1 if ok else 0))
        deriv = (json.dumps({"method": env["method"], "confidence": env["confidence"],
                             "evidence": [str(e) for e in env.get("evidence", [])[:2]]}, separators=(",", ":"))
                 if ok else "unresolved")
        seed_lines.append("%s_derivation=%s" % (slot, deriv))
    required_slots = ["fname_pool_label", "guobject_array_label", "fname_ctor_label", "gmalloc_label", "sco_label"]
    rsc = all(present.get(k) for k in required_slots)
    m.required_complete = rsc
    seed_lines[seed_lines.index("__RSC__")] = "required_set_complete=%d" % (1 if rsc else 0)
    m.sections.append(("seeds", seed_lines))

    # ---- fname ------------------------------------------------------------------------------
    fp = _prefix(elf, _val(doc, "symbols.fname_ctor_wchar.label"), PREFIX_BYTES["fname_ctor"])
    if fp is None:
        raise ProfileRefused("could not read the FName ctor prologue from the image")
    m.sections.append(("fname", ["ctor_prefix=%s" % fp, "ctor_prefix_size=%d" % PREFIX_BYTES["fname_ctor"]]))

    # ---- code_pins --------------------------------------------------------------------------
    sco_p = _prefix(elf, _val(doc, "symbols.static_construct_object_internal.label"), PREFIX_BYTES["sco"])
    cp = []
    if sco_p is not None:
        cp += ["sco_prefix=%s" % sco_p, "sco_prefix_size=%d" % PREFIX_BYTES["sco"]]
    else:
        raise ProfileRefused("could not read the StaticConstructObject prologue from the image")
    # listener and thread prefixes need labels the detectors do not produce -> absent (feature off)
    cp.append("; listener_add_prefix / listener_remove_prefix ABSENT: delete-listener ops are not detected (see [features])")
    cp.append("; thread_name_prefix ABSENT: the thread-name accessor is not detected (see [features])")
    m.sections.append(("code_pins", cp))

    # ---- listener (feature) -----------------------------------------------------------------
    m.features["delete_listener"] = False
    m.sections.append(("listener", [
        "; delete-listener ops (AddUObjectDeleteListener / Remove...) are NOT produced by this generator",
        "; version; no generic detector exists yet (READINESS C2).  delete_listener is OFF in [features];",
        "; the runtime switches only that feature off and still arms the seeds."]))
    m.absent.append(("listener.*", "no generic delete-listener detector (READINESS C2); feature off, fail closed"))

    # ---- tostring (constants only; pin removed B5) ------------------------------------------
    # the probe constants are engine-generic for any UE >= 4.23 shipping name table (checked live)
    m.sections.append(("tostring", ["probe_id=0x3", "probe_expect=ByteProperty",
                                     "probe_expect_units=12", "out_units=1035"]))

    # ---- thread (feature) -------------------------------------------------------------------
    m.features["thread_name"] = False
    m.sections.append(("thread", [
        "; the thread-name accessor and the name storages are NOT produced (READINESS C3 / S6).",
        "; thread_name is OFF in [features]; the runtime binds no thread name and still runs."]))
    m.absent.append(("thread.*", "no generic thread-name detector (READINESS C3/S6); feature off, fail closed"))

    # ---- pak (feature) ----------------------------------------------------------------------
    m.features["pak_mount"] = False
    pak_lines = ["; pak-mount pins (vtable, platform-manager getters, mount delegate, 6 code proofs) are",
                 "; NOT produced by this generator version (READINESS C4/C5).  pak_mount is OFF in [features]:",
                 "; outfit-pak mounting is disabled and fails closed; Lua mods and other features still arm.",
                 "; the engine-generic structural offsets are recorded for when the detector lands:"]
    for key, val in PAK_FIXED.items():
        pak_lines.append(";   %s=%s" % (key, _hx(val) if val else "0"))
    m.sections.append(("pak", pak_lines))
    m.absent.append(("pak.*", "no generic pak-mount detector (READINESS C4/C5); feature off, fail closed"))

    # ---- layout.members / layout.vtables ----------------------------------------------------
    members = render_member_layout(doc, extended=True)
    vtables = render_vtable_layout(doc, extended=True)
    if members is None or vtables is None:
        raise ProfileRefused("the layout identity header or every layout class is incomplete; no staged pair can be written")
    m.sections.append(("layout.members", ["<<<", members.rstrip("\n"), ">>>"]))
    m.sections.append(("layout.vtables", ["<<<", vtables.rstrip("\n"), ">>>"]))
    for want, why in (("vtables.UEngine.LoadMap", "no pure-virtual stub string anchors UEngine::LoadMap"),
                      ("vtables.AGameModeBase.InitGameState", "no string/call-sequence anchor for InitGameState"),
                      ("vtables.FProperty.CopyValuesInternal", "callers are thin memcpy wrappers; slot not identified"),
                      ("vtables.FMalloc.<slots>", "only Malloc/Free are derived; the full FMalloc slot list is not")):
        m.absent.append((want, why + " (READINESS D2)"))

    # ---- features ---------------------------------------------------------------------------
    m.sections.append(("features", ["%s=%d" % (k, 1 if v else 0) for k, v in m.features.items()]))

    # guard: nothing in the file may claim a console verification
    for _name, lines in m.sections:
        for ln in lines:
            mm = FORBIDDEN_MARKER.search(ln)
            if mm:
                raise ProfileRefused("profile text would write %r; the profile only ever says 'generated'" % mm.group(0))
    return m


def render(model: ProfileModel) -> str:
    out: List[str] = []
    for name, lines in model.sections:
        out.append("[%s]" % name)
        out.extend(lines)
    # body hash: sha256 of the file with the single body_sha256 line removed (and its newline), else verbatim
    body_idx = out.index("%s=__BODY__" % BODY_SHA_KEY)
    without = out[:body_idx] + out[body_idx + 1:]
    body_sha = hashlib.sha256(("\n".join(without) + "\n").encode("ascii")).hexdigest()
    out[body_idx] = "%s=%s" % (BODY_SHA_KEY, body_sha)
    return "\n".join(out) + "\n"


def emit(doc: Dict[str, Any], elf: ElfImage, path: str, opts: Optional[ProfileOptions] = None) -> Dict[str, Any]:
    model = build(doc, elf, opts)
    text = render(model)
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    data = text.encode("ascii")
    with open(path, "wb") as fh:
        fh.write(data)
    return {"path": path, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "required_set_complete": model.required_complete,
            "features": dict(model.features), "absent": list(model.absent), "notes": list(model.notes)}


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python3 -m tools.profilegen.emit_profile", description=__doc__)
    ap.add_argument("--profile", required=True, help="runtime-profile.json produced by profilegen")
    ap.add_argument("--eboot", required=True, help="the same decrypted executable the profile describes")
    ap.add_argument("--out", required=True, help="where to write the profile .ini")
    ap.add_argument("--profile-gmalloc", choices=PROFILE_GMALLOC_CHOICES,
                    help="which GMalloc variable seeds [seeds] gmalloc_label: base_slot (default, what THIS runtime's "
                         "V1/V2 seed) or dispatch (what FMemory::Malloc/Free load)")
    ap.add_argument("--exe-path", help="console path of the readable executable copy (B8 / Team S2); omitted otherwise")
    ap.add_argument("--name", help="override the synthesized profile name")
    ns = ap.parse_args(argv)
    try:
        with open(ns.profile, encoding="utf-8") as fh:
            doc = json.load(fh)
    except (OSError, ValueError) as exc:
        print("emit_profile: cannot read %s: %s" % (ns.profile, exc), file=sys.stderr)
        return 2
    try:
        elf = ElfImage.open(ns.eboot)
        rec = emit(doc, elf, ns.out, ProfileOptions(ns.profile_gmalloc, ns.exe_path, ns.name))
    except (ProfileRefused, OSError, ValueError) as exc:
        print("emit_profile: %s" % exc, file=sys.stderr)
        return 2
    print("emit_profile: wrote %s (%d bytes, sha %s)" % (rec["path"], rec["bytes"], rec["sha256"][:16]))
    print("  required_set_complete=%s  features=%s" % (
        rec["required_set_complete"], ", ".join("%s=%d" % (k, 1 if v else 0) for k, v in rec["features"].items())))
    for fld, why in rec["absent"]:
        print("  absent %s: %s" % (fld, why))
    return 0


if __name__ == "__main__":
    sys.exit(main())
