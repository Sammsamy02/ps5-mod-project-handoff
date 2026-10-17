#!/usr/bin/env python3
"""finalize_manifest.py — fill a composition-change manifest's PENDING fields
from a COMPLETED one-pass composition run's actual artifacts (fail-closed).

Worker: worker-manifest-finalizer · 2026-10-04 · proposal-only tooling.

Why: the controller's change manifest
(outputs/2026-10-04-zcode-controller/comp6-integration/change-manifest.json)
carries PENDING per-unit sha256s and a PENDING measured UND import delta that
must be filled from the one-pass composition run's artifacts. Hand-editing at
trigger time is a risk; this tool does it mechanically and refuses anything it
cannot attest.

Inputs
------
  --manifest          the change manifest JSON (PENDING dialect of the
                      controller; schema "worker-gate-evolution/MANIFEST-SPEC.md v1")
  --composition-run   the completed composition run dir (receipt.json with
                      status NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED,
                      selected_runtime_units, runtime_object/runtime_sha256)
  --candidate         the completed packaging candidate dir (build-receipt.json
                      with pinned.lineage/pinned.composition_dir/pinned_hashes,
                      build/combined-supplier.o, gaps == [])
  --baseline          frozen baseline JSON (default: the gate v2 sibling
                      baseline-composition5-r11.json)
  --overlay-manifest  attested overlay manifest (default: overlay-manifest.json
                      next to the candidate's pinned.lineage overlay dir)
  --defined-baseline  OPTIONAL reference composed object (e.g. composition-4
                      run-3) for the SUPPLEMENTARY external-defined-symbol
                      delta report — never filled into import_delta

What it fills
-------------
  changed_units[i].sha256   <- measured sha256 of the source the run actually
                               consumed (run receipt selected_runtime_units
                               [unit].source), re-measured on disk and
                               cross-checked against the receipt attestation.
  packaging_units[i].sha256 <- measured sha256 of the staged copy inside the
                               candidate overlay dir (pinned.lineage/<rel>),
                               cross-checked against the overlay manifest's
                               attested sha and the .sha256 sidecar if present.
  import_delta.added/removed <- EXACT itemized UND delta of the candidate's
                               build/combined-supplier.o vs the baseline
                               combined_und, computed with the SAME pure-python
                               parse gate_check_composition5_v2.py uses for I1+.
                               Fill happens ONLY if the measured sets equal the
                               manifest's declared sets; the expected-empty case
                               fills empty lists and records the measurement.
                               Anything undeclared/absent -> REFUSE.

Declared-value rules (fail-closed)
----------------------------------
  * PENDING-ish (contains "pending", any case) or a truncated prefix
    ("abcdef12...")  -> filled from the measurement; every replacement is
    diff-recorded in the summary/receipt/finalization block.
  * full 64-hex sha  -> must EQUAL the measurement or the tool refuses
    (declaration drifted from what the run actually staged/built).
  * anything else    -> refuse (uninterpretable declaration).

Refusal = exit 2, NO finalized manifest written. Refusals include: bogus unit
with no artifact source; packaging rel missing from the overlay; overlay
manifest sha mismatch; run status/object-hash drift; candidate gaps non-empty;
candidate's pinned composition_dir != the given run; baseline_id mismatch;
measured UND delta != declared; any PENDING remaining after processing.

Honesty: declared-vs-consumed source path/content divergence is never silently
accepted — each unit records consumed path + sha, path divergence and content
divergence flags in the receipt, the finalized manifest's "finalization" block,
and the diff summary. (The dry rehearsal legitimately consumed stand-in
sources; the REAL one-pass run must show zero content divergences.)

No console contact, no target execution, no nested subagents. This worker's
host tests write only under worker-manifest-finalizer/; at a real trigger the
default --out writes the finalized manifest ALONGSIDE the input manifest (the
controller's own directory) — pass --out/--json explicitly to redirect.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

MY = Path(__file__).resolve().parent
TEAM = MY.parent
ROOT = MY.parents[2]  # the worktree root (…/what); relative artifact paths in
                      # run receipts are recorded ROOT-relative by the drivers
DEFAULT_BASELINE = TEAM / 'worker-gate-evolution' / 'baseline-composition5-r11.json'

RUN_STATUS = 'NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED'
SHT_SYMTAB = 2
STB_LOCAL = 0

PENDING_RE = re.compile(r'pending', re.IGNORECASE)
TRUNCATED_SHA_RE = re.compile(r'^[0-9a-f]{4,63}\.\.\.$')
FULL_SHA_RE = re.compile(r'^[0-9a-f]{64}$')


class Refuse(Exception):
    """Cumulative refusal: the tool collects every reason before giving up."""

    def __init__(self):
        self.reasons: list[str] = []
        super().__init__('refused')

    def add(self, reason: str) -> None:
        self.reasons.append(reason)

    @property
    def ok(self) -> bool:
        return not self.reasons


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def resolve_repo(p) -> Path:
    """Resolve a receipt-recorded path: absolute as-is, relative against the
    worktree ROOT (driver convention), CWD fallback if ROOT-relative misses."""
    p = Path(p)
    if p.is_absolute():
        return p.resolve()
    cand = ROOT / p
    if cand.exists():
        return cand.resolve()
    return p.resolve()


# --------------------------------------------------------------- ELF parsing
# Mirrors worker-gate-evolution/gate_check_composition5_v2.py semantics exactly
# (extended-numbering safe; UND == st_shndx == 0 across ALL symtabs) so the
# filled import_delta is measured the same way the gate will verify it.

def elf_sections(data: bytes) -> list[dict] | None:
    if len(data) < 64 or data[:4] != b'\x7fELF' or data[4] != 2 or data[5] != 1:
        return None
    (e_shoff,) = struct.unpack_from('<Q', data, 0x28)
    e_shentsize, e_shnum = struct.unpack_from('<HH', data, 0x3A)
    e_shstrndx = struct.unpack_from('<H', data, 0x3E)[0]
    if e_shoff == 0 or e_shentsize < 64:
        return None
    count = e_shnum
    if count == 0:
        count = struct.unpack_from('<Q', data, e_shoff + 32)[0]
    if count > 5_000_000:
        return None
    secs = []
    for i in range(count):
        off = e_shoff + i * e_shentsize
        if off + 64 > len(data):
            return None
        sh_name, sh_type = struct.unpack_from('<II', data, off)
        sh_offset, sh_size = struct.unpack_from('<QQ', data, off + 24)
        sh_link, sh_info = struct.unpack_from('<II', data, off + 40)
        secs.append(dict(nameoff=sh_name, type=sh_type, offset=sh_offset,
                         size=sh_size, link=sh_link, info=sh_info))
    strtab = b''
    if 0 < e_shstrndx < len(secs):
        s = secs[e_shstrndx]
        strtab = data[s['offset']:s['offset'] + s['size']]
    for s in secs:
        o = s['nameoff']
        s['name'] = strtab[o:strtab.find(b'\x00', o)].decode('utf-8', 'replace') \
            if o < len(strtab) else ''
    return secs


def symbol_sets(path: Path) -> tuple[set[str], set[str]] | None:
    """(undefined, external-defined) name sets across ALL symtabs.

    UND set == gate v2 symtab_und_defined's `und` (identical semantics).
    The defined set is EXTERNAL (binding != LOCAL) — the llvm-nm
    `--extern-only --defined-only` semantics the dry-build receipt's +3/-0
    marker table uses; local .L*/.LCPI*/GCC_except_table churn is excluded.
    """
    try:
        data = path.read_bytes()
    except OSError:
        return None
    secs = elf_sections(data)
    if secs is None:
        return None
    und: set[str] = set()
    defined: set[str] = set()
    for st in (s for s in secs if s['type'] == SHT_SYMTAB):
        if st['link'] >= len(secs):
            continue
        strsec = secs[st['link']]
        strdata = data[strsec['offset']:strsec['offset'] + strsec['size']]
        for j in range(st['size'] // 24):
            off = st['offset'] + j * 24
            st_name, st_info, _, st_shndx = struct.unpack_from('<IBBH', data, off)
            if st_name >= len(strdata):
                continue
            end = strdata.find(b'\x00', st_name)
            name = strdata[st_name:end].decode('utf-8', 'replace')
            if not name:
                continue
            if st_shndx == 0:
                und.add(name)
            elif (st_info >> 4) != STB_LOCAL:
                defined.add(name)
    return und, defined


# ----------------------------------------------------------------- helpers

def load_json(path: Path, what: str, ref: Refuse) -> dict | None:
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError) as e:
        ref.add(f'{what} unreadable: {path} ({e})')
        return None


def declared_sha_action(declared: str | None, measured: str, where: str,
                        diff: list, ref: Refuse) -> str:
    """Apply the declared-value rules; return the sha that must end up in the
    manifest (always the measured one) or register a refusal."""
    if declared is None:
        diff.append(f'{where}: (no declared sha) -> {measured} (filled)')
        return measured
    if PENDING_RE.search(declared):
        diff.append(f'{where}: {declared!r} -> {measured} (pending filled)')
        return measured
    if TRUNCATED_SHA_RE.match(declared):
        if measured.startswith(declared[:-3]):
            diff.append(f'{where}: {declared!r} -> {measured} (prefix confirmed)')
        else:
            diff.append(f'{where}: {declared!r} -> {measured} '
                        f'(TRUNCATED-PREFIX REPLACED — declared prefix did not '
                        f'match what the run actually staged/built)')
        return measured
    if FULL_SHA_RE.match(declared):
        if declared == measured:
            diff.append(f'{where}: {declared} (confirmed)')
        else:
            ref.add(f'{where}: declared FULL sha {declared} != measured {measured} '
                    f'— the manifest attests content the run did not stage/build')
        return measured
    ref.add(f'{where}: uninterpretable declared sha {declared!r} '
            f'(expected PENDING marker, "abcdef12..." prefix, or 64-hex sha)')
    return measured


def find_unit_object(run_dir: Path, unit: str) -> str | None:
    for cand in (f'{unit}.o', f'extra-{unit}.o', f'runtime-{unit}.o',
                 f'replaced-{unit}.o', f'bootstrap-{unit}.o'):
        if (run_dir / cand).is_file():
            return cand
    return None


# -------------------------------------------------------------------- main

def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--manifest', type=Path, required=True,
                    help='change manifest JSON with PENDING fields')
    ap.add_argument('--composition-run', type=Path, required=True,
                    help='completed composition run dir (receipt.json + unit artifacts)')
    ap.add_argument('--candidate', type=Path, required=True,
                    help='completed packaging candidate dir (build-receipt.json, '
                         'build/combined-supplier.o)')
    ap.add_argument('--baseline', type=Path, default=DEFAULT_BASELINE,
                    help='frozen baseline JSON (default: %(default)s)')
    ap.add_argument('--overlay-manifest', type=Path, default=None,
                    help='attested overlay manifest (default: overlay-manifest.json '
                         'next to the candidate pinned.lineage overlay dir)')
    ap.add_argument('--defined-baseline', type=Path, default=None,
                    help='OPTIONAL reference composed object for the supplementary '
                         'external-defined-symbol delta report (e.g. the composition-4 '
                         'run-3 composed object)')
    ap.add_argument('--consumed-source-override', action='append', default=[],
                    metavar='UNIT=PRESERVED_PATH',
                    help='Scoped post-hoc recovery: if the live file at the run-recorded '
                         'consumed path drifted/was lost AFTER the run, measure the unit '
                         'from this PRESERVED copy instead — accepted ONLY if its sha256 '
                         'equals the run receipt attestation (never a bypass); the drift '
                         'is recorded in the diff summary, receipt and finalization block')
    ap.add_argument('--out', type=Path, default=None,
                    help='finalized manifest path (default: <manifest-dir>/'
                         '<stem>.finalized.json — alongside the input manifest)')
    ap.add_argument('--json', type=Path, default=None,
                    help='finalization receipt path (default: <out>.receipt.json)')
    args = ap.parse_args()

    ref = Refuse()
    diff: list[str] = []
    manifest_path = args.manifest.resolve()
    run_dir = args.composition_run.resolve()
    cand_dir = args.candidate.resolve()
    out_path = (args.out or manifest_path.with_name(manifest_path.stem + '.finalized.json')).resolve()
    receipt_path = (args.json or out_path.with_suffix(out_path.suffix + '.receipt.json')).resolve()

    print('== finalize_manifest ==')
    print('  manifest        :', manifest_path)
    print('  composition run :', run_dir)
    print('  candidate       :', cand_dir)
    print('  baseline        :', args.baseline.resolve())
    print('  output          :', out_path)

    manifest = load_json(manifest_path, 'manifest', ref)
    baseline = load_json(args.baseline.resolve(), 'baseline', ref)
    if manifest is None or baseline is None:
        return refuse(ref, receipt_path, args, None, diff)

    manifest_input_sha = sha256_file(manifest_path)

    if manifest.get('baseline_id') != baseline.get('baseline_id'):
        ref.add(f"manifest baseline_id {manifest.get('baseline_id')!r} != baseline "
                f"baseline_id {baseline.get('baseline_id')!r} (gate M0 semantics)")

    # ---- composition run receipt -------------------------------------------
    run_receipt = load_json(run_dir / 'receipt.json', 'run receipt', ref)
    run_receipt_sha = sha256_file(run_dir / 'receipt.json') \
        if (run_dir / 'receipt.json').is_file() else None
    selected = {}
    if run_receipt is not None:
        if run_receipt.get('status') != RUN_STATUS:
            ref.add(f'run status drift: {run_receipt.get("status")!r} != {RUN_STATUS!r}')
        selected = run_receipt.get('selected_runtime_units') or {}
        if not selected:
            ref.add('run receipt has no selected_runtime_units; nothing is attestable')
        obj = Path(run_receipt.get('runtime_object', ''))
        if not obj.is_file():
            ref.add(f'composed runtime object missing: {obj}')
        else:
            got = sha256_file(obj)
            if got != run_receipt.get('runtime_sha256'):
                ref.add(f'composed runtime object hash drift: on disk {got} != '
                        f"receipt {run_receipt.get('runtime_sha256')}")
            else:
                print(f"  composed object : {obj.name} sha {got[:16]}... "
                      f"({run_receipt.get('runtime_bytes')} bytes)")

    # ---- candidate build receipt -------------------------------------------
    br_path = cand_dir / 'build-receipt.json'
    build_receipt = load_json(br_path, 'candidate build-receipt', ref)
    build_receipt_sha = sha256_file(br_path) if br_path.is_file() else None
    overlay_dir = None
    if build_receipt is not None:
        pinned = build_receipt.get('pinned') or {}
        overlay_dir = pinned.get('lineage')
        if not overlay_dir:
            ref.add('candidate build-receipt pinned.lineage missing (no overlay dir)')
        else:
            overlay_dir = Path(overlay_dir)
            if not overlay_dir.is_dir():
                ref.add(f'overlay dir missing: {overlay_dir}')
        comp_dir = pinned.get('composition_dir')
        if comp_dir and Path(comp_dir).resolve() != run_dir:
            ref.add(f'candidate pinned.composition_dir {comp_dir} != given '
                    f'composition run {run_dir}')
        ph = build_receipt.get('pinned_hashes') or {}
        if run_receipt is not None and ph.get('composition_object_sha256') \
                    != run_receipt.get('runtime_sha256'):
            ref.add(f'candidate pinned composition_object_sha256 '
                    f'{ph.get("composition_object_sha256")} != run receipt '
                    f'runtime_sha256 {run_receipt.get("runtime_sha256")}')
        if build_receipt.get('gaps'):
            ref.add(f'candidate build-receipt reports gaps: {build_receipt["gaps"]!r}')

    overlay_manifest_path = args.overlay_manifest
    if overlay_manifest_path is None and overlay_dir is not None:
        candidate_om = overlay_dir.parent / 'overlay-manifest.json'
        if candidate_om.is_file():
            overlay_manifest_path = candidate_om
    overlay_manifest = load_json(overlay_manifest_path.resolve(), 'overlay manifest', ref) \
        if overlay_manifest_path else None

    combined = cand_dir / 'build' / 'combined-supplier.o'
    combined_sha = sha256_file(combined) if combined.is_file() else None

    # ---- fill changed_units -------------------------------------------------
    changed_fills: dict[str, dict] = {}
    divergences: list[str] = []
    overrides: dict[str, Path] = {}
    for spec in args.consumed_source_override:
        unit, sep, path = spec.partition('=')
        if not sep or not unit:
            ref.add(f'bad --consumed-source-override spec (UNIT=PATH): {spec}')
            continue
        overrides[unit] = resolve_repo(path)
    for u in (manifest.get('changed_units') or []):
        name = u.get('unit', '?')
        where = f'changed_units[{name}]'
        entry = selected.get(name)
        if entry is None:
            ref.add(f'{where}: no artifact source — unit not in the run receipt '
                    f'selected_runtime_units ({len(selected)} units known); a '
                    f'PENDING sha here has no measured source')
            changed_fills[name] = {'status': 'NO-ARTIFACT-SOURCE'}
            continue
        consumed = resolve_repo(entry.get('source', ''))
        attested = entry.get('source_sha256')
        live_sha = sha256_file(consumed) if consumed.is_file() else None
        drifted = live_sha is not None and attested is not None and live_sha != attested
        measured = None if drifted else live_sha
        measure_src = consumed
        restored = None
        if measured is None and name in overrides:
            snap = overrides[name]
            if not snap.is_file():
                ref.add(f'{where}: consumed-source-override snapshot missing: {snap}')
            else:
                snap_sha = sha256_file(snap)
                if not attested or snap_sha != attested:
                    ref.add(f'{where}: consumed-source-override REFUSED — snapshot '
                            f'{snap} sha {snap_sha} != run-receipt attestation '
                            f'{attested}; an override may only restore bytes the run '
                            f'itself attested')
                else:
                    measured = snap_sha
                    measure_src = snap
                    restored = {'live_consumed_path': str(consumed),
                                'live_state': 'MISSING' if live_sha is None else
                                              f'POST-RUN-DRIFT (live sha {live_sha})',
                                'snapshot_path': str(snap),
                                'snapshot_matches_run_attestation': True}
                    line = (f'{where}: consumed source restored from attested snapshot '
                            f'{snap} (live {consumed} drifted/missing post-run; snapshot '
                            f'sha == run attestation {attested})')
                    divergences.append(line)
                    print(f'  [DRIFT-RESTORED] {line}')
        if measured is None:
            if drifted:
                ref.add(f'{where}: consumed source drifted after the run: {consumed} '
                        f'live {live_sha} != run-receipt attestation {attested} '
                        f'(pass a matching --consumed-source-override to restore the '
                        f'attested bytes from a preserved copy)')
            else:
                ref.add(f'{where}: consumed source missing on disk: {consumed} '
                        f'(no attested --consumed-source-override given)')
            changed_fills[name] = {'status': 'DRIFT' if drifted else 'SOURCE-MISSING',
                                   'consumed_source': str(consumed)}
            continue
        declared_sha = u.get('sha256')
        filled = declared_sha_action(declared_sha, measured, where, diff, ref)
        u['sha256'] = filled
        declared_source = u.get('source')
        path_div = bool(declared_source) and resolve_repo(declared_source) != consumed
        content_ident = None
        if declared_source and resolve_repo(declared_source).is_file():
            content_ident = sha256_file(resolve_repo(declared_source)) == measured
        rec = {'status': 'FILLED', 'declared_sha256': declared_sha,
               'filled_sha256': filled, 'consumed_source': str(consumed),
               'measured_from': str(measure_src),
               'consumed_source_restored_from_snapshot': restored,
               'receipt_attested_sha256': attested,
               'declared_source': declared_source,
               'path_divergent': path_div,
               'content_identical_to_declared_source': content_ident,
               'unit_object_in_run_dir': find_unit_object(run_dir, name)}
        changed_fills[name] = rec
        if path_div:
            line = (f'{where}: declared source {declared_source} vs consumed '
                    f'{consumed} (sha {measured[:12]}...)')
            if content_ident is False:
                line += ' — CONTENT DIVERGENCE (declared file differs from what ' \
                        'the run compiled)'
                divergences.append(line)
                print(f'  [DIVERGENCE] {line}')
            else:
                divergences.append(line + ' — path-only (content identical)'
                                          if content_ident else
                                          f'{line} — declared source not on disk')
                print(f'  [note] {line}')

    # ---- fill packaging_units ----------------------------------------------
    packaging_fills: dict[str, dict] = {}
    for u in (manifest.get('packaging_units') or []):
        rel = u.get('unit', '?')
        where = f'packaging_units[{rel}]'
        if overlay_dir is None:
            continue  # already refused above
        staged = overlay_dir / rel
        if not staged.is_file():
            ref.add(f'{where}: staged copy missing in the candidate overlay: {staged}')
            packaging_fills[rel] = {'status': 'NO-STAGED-COPY', 'staged_path': str(staged)}
            continue
        measured = sha256_file(staged)
        om_entry = (overlay_manifest or {}).get('units', {}).get(rel)
        om_sha = om_entry.get('sha256') if isinstance(om_entry, dict) else None
        if overlay_manifest is not None:
            if om_entry is None:
                ref.add(f'{where}: not attested in overlay manifest '
                        f'{overlay_manifest_path}')
            elif om_sha != measured:
                ref.add(f'{where}: overlay manifest attests {om_sha} != measured '
                        f'{measured} ({staged})')
        sidecar = staged.with_name(staged.name + '.sha256')
        sidecar_sha = None
        if sidecar.is_file():
            sidecar_sha = sidecar.read_text().strip()
            if sidecar_sha != measured:
                ref.add(f'{where}: sidecar {sidecar.name} attests {sidecar_sha} != '
                        f'measured {measured}')
        declared_sha = u.get('sha256')
        filled = declared_sha_action(declared_sha, measured, where, diff, ref)
        u['sha256'] = filled
        rec = {'status': 'FILLED', 'declared_sha256': declared_sha,
               'filled_sha256': filled, 'staged_path': str(staged),
               'overlay_manifest_sha256': om_sha,
               'overlay_manifest_source': (om_entry or {}).get('source')
                                          if isinstance(om_entry, dict) else None,
               'sidecar_sha256': sidecar_sha,
               'declared_source': u.get('source')}
        packaging_fills[rel] = rec
        if rec['overlay_manifest_source'] and rec['declared_source'] and \
                resolve_repo(rec['declared_source']) != resolve_repo(rec['overlay_manifest_source']):
            line = (f'{where}: declared source {rec["declared_source"]} vs staged '
                    f'{rec["overlay_manifest_source"]}')
            divergences.append(line)
            print(f'  [note] {line}')

    # ---- measure + fill import_delta ---------------------------------------
    base_und = set(baseline.get('combined_und') or [])
    if not base_und:
        ref.add('baseline has no combined_und set')
    import_meas: dict = {}
    if combined.is_file() and base_und:
        parsed = symbol_sets(combined)
        if parsed is None:
            ref.add(f'combined object unreadable by the ELF census: {combined}')
        else:
            new_und, _ = parsed
            act_added = sorted(new_und - base_und)
            act_removed = sorted(base_und - new_und)
            declared = manifest.get('import_delta') or {}
            want_added = sorted(declared.get('added') or [])
            want_removed = sorted(declared.get('removed') or [])
            import_meas = {'combined_object': str(combined),
                           'combined_object_sha256': combined_sha,
                           'measured_und_count': len(new_und),
                           'baseline_id': baseline.get('baseline_id'),
                           'baseline_und_count': len(base_und),
                           'measured_added': act_added,
                           'measured_removed': act_removed,
                           'declared_added': want_added,
                           'declared_removed': want_removed}
            if act_added != want_added or act_removed != want_removed:
                undecl_add = sorted(set(act_added) - set(want_added))
                undecl_rem = sorted(set(act_removed) - set(want_removed))
                phantom_add = sorted(set(want_added) - set(act_added))
                phantom_rem = sorted(set(want_removed) - set(act_removed))
                ref.add('import_delta: measured UND delta +{}/-{} != declared +{}/-{} '
                        '(UNDECLARED ADDITIONS: {}; UNDECLARED REMOVALS: {}; '
                        'DECLARED BUT ABSENT: +{}/-{}) — refusing to fill'.format(
                            act_added, act_removed, want_added, want_removed,
                            undecl_add, undecl_rem, phantom_add, phantom_rem))
            else:
                declared['added'] = act_added
                declared['removed'] = act_removed
                declared['measurement'] = {
                    'method': 'UND(build/combined-supplier.o) vs baseline '
                              'combined_und — gate_check_composition5_v2.py I1+ '
                              'census semantics',
                    'baseline_id': baseline.get('baseline_id'),
                    'combined_object_sha256': combined_sha,
                    'measured_und_count': len(new_und),
                    'baseline_und_count': len(base_und),
                    'utc': now_utc()}
                diff.append(f'import_delta: declared +{want_added}/-{want_removed} ; '
                            f'measured +{act_added}/-{act_removed} (UND {len(new_und)} '
                            f'vs baseline {len(base_und)}) -> FILLED')
            print(f'  UND delta       : +{act_added}/-{act_removed} '
                  f'(combined UND {len(new_und)} vs baseline {len(base_und)})')
    elif not combined.is_file():
        ref.add(f'combined object missing: {combined}')

    # ---- supplementary defined-symbol delta --------------------------------
    defined_delta: dict | None = None
    if args.defined_baseline is not None:
        refobj = args.defined_baseline.resolve()
        if run_receipt is not None and Path(run_receipt['runtime_object']).is_file():
            fresh_parse = symbol_sets(Path(run_receipt['runtime_object']))
            base_parse = symbol_sets(refobj) if refobj.is_file() else None
            if fresh_parse is None:
                ref.add(f'fresh composed object unreadable by the ELF census: '
                        f'{run_receipt["runtime_object"]}')
            elif base_parse is None:
                ref.add(f'--defined-baseline unreadable/missing: {refobj}')
            else:
                _, fresh_def = fresh_parse
                _, base_def = base_parse
                d_add, d_rem = sorted(fresh_def - base_def), sorted(base_def - fresh_def)
                defined_delta = {'reference_object': str(refobj),
                                 'fresh_external_defined_count': len(fresh_def),
                                 'reference_external_defined_count': len(base_def),
                                 'added': d_add, 'removed': d_rem,
                                 'semantics': 'external (non-LOCAL, defined) symbol '
                                              'sets of the composed objects — '
                                              'supplementary report only; NOT part '
                                              'of import_delta'}
                print(f'  defined delta   : +{len(d_add)}/-{len(d_rem)} vs {refobj.name}'
                      + (f' {d_add}' if d_add else '')
                      + (f' removed {d_rem}' if d_rem else ''))
                diff.append(f'defined_symbol_delta vs {refobj.name}: '
                            f'+{len(d_add)}/-{len(d_rem)}'
                            + (f' {d_add}' if d_add else ''))

    # ---- final PENDING scan --------------------------------------------------
    leftover = sorted(find_pending(manifest))
    if leftover:
        ref.add(f'PENDING remains after processing at: {leftover}')

    finalization = {
        'tool': 'worker-manifest-finalizer/finalize_manifest.py',
        'tool_sha256': sha256_file(MY / 'finalize_manifest.py'),
        'utc': now_utc(),
        'manifest_input_sha256': manifest_input_sha,
        'gate_v2_dialect_note': {
            'observed_schema_field': manifest.get('schema'),
            'gate_v2_M0_requires': ['manifest_version == 1', 'baseline_id match',
                                    'non-empty justification'],
            'present': {k: (k in manifest) for k in
                        ('manifest_version', 'justification', 'manifest_id')},
            'note': 'the controller manifest is the PENDING-fill dialect; '
                    'MANIFEST-SPEC-v1/M0 field reconciliation (manifest_version, '
                    'justification, manifest_id, per-unit receipt/sha256_after '
                    'shapes) is a separate conscious step — this tool fills only '
                    'and never invents those fields',
        },
        'composition_run': str(run_dir),
        'run_receipt_sha256': run_receipt_sha,
        'candidate': str(cand_dir),
        'build_receipt_sha256': build_receipt_sha,
        'baseline': str(args.baseline.resolve()),
        'combined_object_sha256': combined_sha,
        'import_delta_measurement': import_meas,
        'defined_symbol_delta_vs_reference': defined_delta,
        'changed_units_fill': changed_fills,
        'packaging_units_fill': packaging_fills,
        'overlay_manifest': str(overlay_manifest_path.resolve())
                            if overlay_manifest_path else None,
        'divergences': divergences,
    }
    manifest['finalization'] = finalization

    if not ref.ok:
        return refuse(ref, receipt_path, args, finalization, diff)

    # ---- write finalized manifest (atomic) -----------------------------------
    out_path.parent.mkdir(parents=True, exist_ok=True)
    payload = json.dumps(manifest, indent=1, ensure_ascii=False) + '\n'
    with tempfile.NamedTemporaryFile('w', dir=str(out_path.parent),
                                     delete=False) as tf:
        tf.write(payload)
        tmp = Path(tf.name)
    tmp.chmod(0o644)
    tmp.replace(out_path)
    finalized_sha = sha256_file(out_path)
    finalization['finalized_manifest_sha256'] = finalized_sha

    print('== diff summary ==')
    for line in diff:
        print('  *', line)
    if divergences:
        print(f'  [!] {len(divergences)} declared-vs-consumed divergence(s) recorded '
              f'(see finalization block / receipt); the REAL one-pass run must '
              f'explain or eliminate every content divergence')
    print(f'WROTE {out_path} (sha {finalized_sha[:16]}...)')

    write_receipt(receipt_path, args, finalization, True, [], diff, out_path,
                  finalized_sha)
    print(f'RECEIPT {receipt_path}')
    return 0


def find_pending(node, path='$'):
    hits = []
    if isinstance(node, dict):
        for k, v in node.items():
            if k == 'finalization':
                continue
            hits += find_pending(v, f'{path}.{k}')
    elif isinstance(node, list):
        for i, v in enumerate(node):
            hits += find_pending(v, f'{path}[{i}]')
    elif isinstance(node, str) and PENDING_RE.search(node):
        hits.append(f'{path} = {node!r}')
    return hits


def now_utc() -> str:
    return datetime.now(timezone.utc).isoformat()


def write_receipt(path: Path, args, finalization: dict | None, ok: bool,
                  reasons: list[str], diff: list, out_path: Path | None,
                  finalized_sha: str | None) -> None:
    doc = {'tool': 'finalize_manifest.py',
           'tool_sha256': sha256_file(MY / 'finalize_manifest.py'),
           'utc': now_utc(),
           'ok': ok,
           'refusals': reasons,
           'inputs': {'manifest': str(args.manifest.resolve()),
                      'composition_run': str(args.composition_run.resolve()),
                      'candidate': str(args.candidate.resolve()),
                      'baseline': str(args.baseline.resolve()),
                      'overlay_manifest': str(args.overlay_manifest.resolve())
                                          if args.overlay_manifest else None,
                      'defined_baseline': str(args.defined_baseline.resolve())
                                          if args.defined_baseline else None},
           'finalized_manifest': str(out_path) if out_path else None,
           'finalized_manifest_sha256': finalized_sha,
           'diff': diff,
           'finalization': finalization}
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + '\n')


def refuse(ref: Refuse, receipt_path: Path, args, finalization: dict | None,
           diff: list) -> int:
    print('REFUSED — no finalized manifest written:', file=sys.stderr)
    for r in ref.reasons:
        print('  -', r, file=sys.stderr)
    write_receipt(receipt_path, args, finalization, False, ref.reasons, diff,
                  None, None)
    print(f'REFUSAL RECEIPT {receipt_path}', file=sys.stderr)
    return 2


if __name__ == '__main__':
    sys.exit(main())
