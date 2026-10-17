"""Command line: ``python3 -m tools.profilegen --eboot PATH --param PATH --out DIR``."""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
from typing import List, Optional, Tuple

from . import detectors, emit_profile, emit_seedtable, emit_ue4ss, profile
from .elf import ElfError, ElfImage, EncryptedSelfError, check_not_encrypted_self
from .index import ImageIndex
from .model import Context

# exit codes (0 = done; unresolved values are listed, not fatal)
EXIT_INVALID_INPUT = 2          # unreadable / not an ELF / bad param.json / bad option: nothing is written
EXIT_VERIFIED_KEPT = 3          # the output folder holds a verified profile and --force was not given
EXIT_SEEDTABLE_REFUSED = 4      # --emit-seedtable could not be honoured (the rest of the profile was written)
EXIT_ENCRYPTED_SELF = 5         # the executable is an encrypted SELF: refused before any analysis


def _jsonable(o):
    if isinstance(o, (set, frozenset)):
        return sorted(o)
    if isinstance(o, (bytes, bytearray)):
        return o.hex()
    if isinstance(o, int):
        return o
    return str(o)


class VerifiedProfileError(RuntimeError):
    """The output folder already holds a profile that carries a console verification."""


class InputError(ValueError):
    """An input file is missing or unusable; raised before anything is analysed or written."""


def _check_executable(path: str) -> None:
    """Cheap checks on the executable before it is mapped: exists, is a file, is not empty, is not an encrypted SELF."""
    if not os.path.exists(path):
        raise InputError("executable not found: %s" % path)
    if not os.path.isfile(path):
        raise InputError("executable is not a regular file: %s" % path)
    with open(path, "rb") as fh:
        head = fh.read(4)
    if not head:
        raise InputError("executable is empty: %s" % path)
    check_not_encrypted_self(head)


def _load_param(path: str) -> Tuple[bytes, dict]:
    """``param.json`` as (bytes, object); a missing, unreadable, non-JSON or non-object file is an error, not a silent skip."""
    if not os.path.exists(path):
        raise InputError("param.json not found: %s (pass the title's param.json, or leave --param out to generate without a binding)" % path)
    if not os.path.isfile(path):
        raise InputError("param.json is not a regular file: %s" % path)
    with open(path, "rb") as fh:
        raw = fh.read()
    try:
        doc = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, ValueError) as exc:
        raise InputError("param.json is not valid UTF-8 JSON: %s: %s" % (path, exc))
    if not isinstance(doc, dict):
        raise InputError("param.json must be a JSON object, not %s: %s" % (type(doc).__name__, path))
    return raw, doc


def _check_out(out: str) -> None:
    if os.path.exists(out) and not os.path.isdir(out):
        raise InputError("output path exists and is not a directory: %s" % out)


def _guard_existing(out: str, force: bool) -> None:
    path = os.path.join(out, "runtime-profile.json")
    if force or not os.path.isfile(path):
        return
    try:
        with open(path, encoding="utf-8") as fh:
            old = json.load(fh)
    except (OSError, ValueError):
        return
    if old.get("verification") is not None or old.get("status") not in (None, "generated"):
        raise VerifiedProfileError(
            "%s carries a console verification / non-generated status; profilegen never overwrites that "
            "(use --force to replace it, which discards the verification)" % path)


class Result:
    """What ``generate`` returns: the document plus the run's context and timings."""

    def __init__(self, doc: dict, ctx: Context, total: float) -> None:
        self.doc = doc
        self.ctx = ctx
        self.total = total
        self.seed = None            # emit_seedtable.SeedTable when a header was built
        self.seed_path = None       # where it was written, if it was
        self.seed_error = None      # why SeedTable.h was not produced (text), if it was not
        self.seed_requested = False  # --emit-seedtable was given
        self.profile = None          # emit_profile record (dict) when a profile .ini was built
        self.profile_path = None     # where the profile .ini was written, if it was
        self.profile_error = None    # why the profile .ini was not produced (text), if it was not
        self.profile_requested = False  # --emit-profile was given
        self.timings = dict(sorted({**ctx.index.timings, **ctx.timings}.items()))


def _function_map_note(ctx: Context) -> None:
    """Say so when the function map the detectors relied on is too sparse to trust (stripped / non-frame-pointer code)."""
    funcs = getattr(ctx.index, "_funcs", None)
    if funcs is not None and not funcs.usable:
        ctx.fail("analysis.function_map",
                 "only %d function starts recovered from %d bytes of code (needs a frame-pointer prologue or int3 padding "
                 "between functions): values that depend on function boundaries are unresolved or capped" % (
                     len(funcs.starts), funcs.exec_bytes), "more-analysis")


def _emit_seedtable(doc: dict, out: Optional[str], path: Optional[str], options: "emit_seedtable.SeedOptions"
                    ) -> Tuple[List[dict], object, Optional[str], Optional[str]]:
    """(emit records, model, written path, error) for an explicit ``--emit-seedtable`` path.  The header is written even
    when required slots are absent (it says which, and ``kRequiredSetComplete`` is false); it is refused only when the
    load base, the FNamePool label or the executable hash is missing, or an option cannot be honoured."""
    if path is None:
        return [], None, None, None
    try:
        text, model = emit_seedtable.render_profile(doc, options)
    except emit_seedtable.SeedTableRefused as exc:
        return [], None, None, str(exc)
    record = emit_seedtable.write_text(text, path)
    records = []
    rel = os.path.relpath(os.path.abspath(path), os.path.abspath(out)) if out else ".."
    if not rel.startswith(".."):                       # only files inside the profile folder belong in emit[]
        records.append({"path": rel.replace(os.sep, "/"), "bytes": record["bytes"], "sha256": record["sha256"]})
    return records, model, path, None


def _emit_profile_ini(doc, elf, out, path, options, result) -> Tuple[List[dict], Optional[str]]:
    """Write the per-game runtime PROFILE FILE (Team S1 format, B4) to ``path``; return (emit records, error)."""
    if path is None:
        return [], None
    try:
        rec = emit_profile.emit(doc, elf, path, options or emit_profile.ProfileOptions())
    except emit_profile.ProfileRefused as exc:
        return [], str(exc)
    result.profile, result.profile_path = rec, path
    rel = os.path.relpath(os.path.abspath(path), os.path.abspath(out)) if out else ".."
    if not rel.startswith(".."):                       # only files inside the profile folder belong in emit[]
        return [{"path": rel.replace(os.sep, "/"), "bytes": rec["bytes"], "sha256": rec["sha256"]}], None
    return [], None


def generate(eboot: str, param: Optional[str] = None, out: Optional[str] = None, load_base: Optional[int] = None,
             strict: bool = False, only: Optional[List[str]] = None, game_profiles: Optional[str] = None,
             utc: Optional[str] = None, evidence: bool = True, force: bool = False,
             seedtable: Optional[str] = None, seed_options: Optional["emit_seedtable.SeedOptions"] = None,
             profile_ini: Optional[str] = None,
             profile_options: Optional["emit_profile.ProfileOptions"] = None) -> Result:
    """Run every detector and (when ``out`` is given) write the profile folder.

    Inputs are checked first and nothing is written when one is unusable (``InputError``, ``ElfError``,
    ``EncryptedSelfError``).  ``seedtable`` is an explicit path for ``SeedTable.h`` (not written unless asked for).
    ``profile_ini`` is an explicit path for the per-game runtime PROFILE FILE (Team S1 format, B4)."""
    t0 = time.time()
    _check_executable(eboot)
    param_bytes = None
    param_doc = None
    if param:
        param_bytes, param_doc = _load_param(param)
    if out:
        _check_out(out)
        _guard_existing(out, force)
    elf = ElfImage.open(eboot)
    index = ImageIndex(elf)
    options = {"param_bytes": param_bytes, "load_base": load_base, "game_profiles_dir": game_profiles}
    ctx = Context(elf, index, param_doc, options)
    detectors.run_all(ctx, strict=strict, only=set(only) if only else None)
    _function_map_note(ctx)
    doc = profile.assemble(ctx, utc=utc)
    result = Result(doc, ctx, 0.0)
    result.seed_requested = seedtable is not None
    seed_records, model, written, err = _emit_seedtable(doc, out, seedtable, seed_options or emit_seedtable.SeedOptions())
    result.seed, result.seed_path, result.seed_error = model, written, err
    if model is not None:
        ctx.evidence_files["seedtable"] = {"notes": model.notes, "complete": model.complete, "written": written is not None}
    result.profile_requested = profile_ini is not None
    profile_records, result.profile_error = _emit_profile_ini(doc, elf, out, profile_ini, profile_options, result)
    if result.profile is not None:
        ctx.evidence_files["profile"] = {"required_set_complete": result.profile["required_set_complete"],
                                         "features": result.profile["features"], "absent": result.profile["absent"],
                                         "notes": result.profile["notes"], "sha256": result.profile["sha256"]}
    if out:
        os.makedirs(out, exist_ok=True)
        emitted = emit_ue4ss.emit(doc, out) + seed_records + profile_records
        doc["emit"] = emitted
        for need, text in (("emit.VTableLayout", "VTableLayout.ini"), ("emit.MemberVariableLayout", "MemberVariableLayout.ini")):
            if not any(e["path"].endswith(text) for e in emitted):
                doc["unresolved"].append({"id": need, "why": "identity header incomplete or no layout class fully derived",
                                          "needs": "more-analysis"})
        doc["unresolved"].sort(key=lambda u: u["id"])
        with open(os.path.join(out, "runtime-profile.json"), "w", encoding="ascii") as fh:
            json.dump(doc, fh, indent=2, ensure_ascii=True)
            fh.write("\n")
        if evidence:
            ev_dir = os.path.join(out, "evidence")
            os.makedirs(ev_dir, exist_ok=True)
            for name, payload in sorted(ctx.evidence_files.items()):
                with open(os.path.join(ev_dir, name + ".json"), "w", encoding="ascii") as fh:
                    json.dump(payload, fh, indent=2, ensure_ascii=True, default=_jsonable, sort_keys=True)
                    fh.write("\n")
            if ctx.notes:
                with open(os.path.join(ev_dir, "detector-errors.txt"), "w", encoding="utf-8") as fh:
                    fh.write("\n".join(ctx.notes))
    result.total = round(time.time() - t0, 3)
    result.timings["total"] = result.total
    if out and evidence:
        with open(os.path.join(out, "evidence", "timing.json"), "w", encoding="ascii") as fh:
            json.dump(result.timings, fh, indent=2, sort_keys=True)
            fh.write("\n")
    return result


def _parse_load_base(text: Optional[str]) -> Optional[int]:
    if not text:
        return None
    try:
        value = int(text, 0)
    except ValueError:
        raise InputError("--load-base must be an integer (hex, page-aligned), not %r" % text)
    if value < 0 or value >= (1 << 40) or value % 0x1000:
        raise InputError("--load-base 0x%x is not a plausible page-aligned load base" % value if value >= 0
                         else "--load-base must not be negative")
    return value


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(prog="python3 -m tools.profilegen", description=__doc__)
    ap.add_argument("--eboot", required=True, help="decrypted PS5 executable (ELF, e_type 0xFE10)")
    ap.add_argument("--param", help="param.json of the title (binding: title id, content version)")
    ap.add_argument("--out", required=True, help="output folder (runtime-profile.json, ue4ss/, evidence/)")
    ap.add_argument("--load-base", help="runtime load base if known (hex); default: PS5 platform convention")
    ap.add_argument("--game-profiles", help="folder with modbridge.game-profile/1 files for the version map")
    ap.add_argument("--only", action="append", help="run only this detector (repeatable)")
    ap.add_argument("--strict", action="store_true", help="let detector exceptions propagate")
    ap.add_argument("--no-evidence", action="store_true", help="do not write evidence/*.json")
    ap.add_argument("--utc", help="fixed generated.utc (for byte-reproducible output)")
    ap.add_argument("--force", action="store_true", help="overwrite an existing profile even if it carries a verification")
    ap.add_argument("--emit-seedtable", metavar="PATH",
                    help="also write SeedTable.h (SEED-TABLE-FORMAT.md v2) to PATH, even if some required slots are absent "
                         "(the header says which); not written unless asked for. Inside --out it is listed in emit[]")
    ap.add_argument("--seed-gmalloc", choices=emit_seedtable.GMALLOC_CHOICES,
                    help="GMalloc variable slot 3 seeds: dispatch = what FMemory::Malloc/Free load (the generator's value, "
                         "default) or base_slot = the raw allocator pointer; the header names both")
    ap.add_argument("--seed-heap-span", metavar="unbounded|omit|LO:HI",
                    help="kHeapLo/kHeapHi in SeedTable.h: the profile has no heap span, so the default is unbounded")
    ap.add_argument("--emit-profile", metavar="PATH",
                    help="also write the per-game runtime PROFILE FILE (Team S1 PROFILE-FORMAT.md, B4) to PATH: the one data "
                         "file the generic runtime loads instead of the compiled per-game headers. Inside --out it is in emit[]")
    ap.add_argument("--profile-gmalloc", choices=emit_profile.PROFILE_GMALLOC_CHOICES,
                    help="which GMalloc variable [seeds] gmalloc_label takes: base_slot (default, what THIS runtime's V1/V2 "
                         "seed) or dispatch (what FMemory::Malloc/Free load)")
    ap.add_argument("--exe-path", help="console path of the readable executable copy for the profile (B8 / Team S2); "
                                       "omitted otherwise")
    ap.add_argument("--profile-name", help="override the synthesized profile name")
    ap.add_argument("--quiet", action="store_true")
    ns = ap.parse_args(argv)
    unknown = [n for n in (ns.only or []) if n not in {name for name, _fn in detectors.ORDER}]
    if unknown:
        print("profilegen: unknown detector(s): %s (known: %s)" % (
            ", ".join(unknown), " ".join(name for name, _fn in detectors.ORDER)), file=sys.stderr)
        return EXIT_INVALID_INPUT
    try:
        base = _parse_load_base(ns.load_base)
        seed_options = emit_seedtable.SeedOptions(ns.seed_gmalloc, ns.seed_heap_span)
        profile_options = emit_profile.ProfileOptions(ns.profile_gmalloc, ns.exe_path, ns.profile_name)
    except (InputError, emit_seedtable.SeedTableRefused, emit_profile.ProfileRefused) as exc:
        print("profilegen: %s" % exc, file=sys.stderr)
        return EXIT_INVALID_INPUT
    try:
        result = generate(ns.eboot, ns.param, ns.out, base, ns.strict, ns.only, ns.game_profiles, ns.utc,
                          not ns.no_evidence, ns.force, ns.emit_seedtable, seed_options,
                          ns.emit_profile, profile_options)
    except VerifiedProfileError as exc:
        print("profilegen: %s" % exc, file=sys.stderr)
        return EXIT_VERIFIED_KEPT
    except EncryptedSelfError as exc:
        print("profilegen: refused: %s" % exc, file=sys.stderr)
        return EXIT_ENCRYPTED_SELF
    except (InputError, ElfError, OSError, ValueError) as exc:
        print("profilegen: %s" % exc, file=sys.stderr)
        return EXIT_INVALID_INPUT
    doc = result.doc
    counts = profile.confidence_counts(doc)
    if not ns.quiet:
        print("profilegen: %s  status=%s  digest=%s" % (doc["profile_id"], doc["status"], doc["profile_digest"][:16]))
        print("  values: %d high, %d medium, %d low;  unresolved: %d;  files: %s" % (
            counts["high"], counts["medium"], counts["low"], len(doc["unresolved"]),
            ", ".join(e["path"] for e in doc["emit"]) or "none"))
        t = result.timings
        print("  time: %.1fs total (%s)" % (result.total, ", ".join("%s %.1f" % (k.replace("detector.", ""), v)
                                                                   for k, v in t.items() if k != "total" and v >= 0.5)))
        if result.seed is not None and result.seed_path:
            print("  SeedTable.h: %s  (%d of %d required slots, kRequiredSetComplete=%s)" % (
                result.seed_path, sum(1 for s in result.seed.slots if s.present), len(result.seed.slots),
                str(result.seed.complete).lower()))
            for n in result.seed.notes:
                print("    seedtable: " + n)
        if result.profile is not None and result.profile_path:
            print("  profile.ini: %s  (required_set_complete=%s, features: %s)" % (
                result.profile_path, str(result.profile["required_set_complete"]).lower(),
                ", ".join("%s=%d" % (k, 1 if v else 0) for k, v in result.profile["features"].items()) or "none"))
            for fld, why in result.profile["absent"]:
                print("    profile absent %s: %s" % (fld, why))
        for u in doc["unresolved"]:
            print("  unresolved %s: %s" % (u["id"], u["why"]))
    if result.seed_requested and result.seed_error:
        print("profilegen: SeedTable.h not written: %s" % result.seed_error, file=sys.stderr)
        return EXIT_SEEDTABLE_REFUSED
    if result.profile_requested and result.profile_error:
        print("profilegen: profile.ini not written: %s" % result.profile_error, file=sys.stderr)
        return EXIT_SEEDTABLE_REFUSED
    return 0
