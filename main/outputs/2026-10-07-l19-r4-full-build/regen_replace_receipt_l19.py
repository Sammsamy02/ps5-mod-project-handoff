#!/usr/bin/env python3
"""Regenerate the composition-6 replace-receipt (RESOLUTION-DESIGN §3.3 step 2).

Produces the hash-pinned receipt consumed by build_runtime_composition6.py /
build_runtime_composition6_fixed.py via --replace-receipt:

  files = carry-forward of composition4-replace-receipt.json (bootstrap
          replacement deps + staged UnrealInitializer.cpp + supplier port
          headers; byte-drift REFUSES — those must not change for composition-6)
        + REQUIRED: the staged program copy (--program-source), the port-tree
          Platform/NativeProgramContext.hpp (--odr-header, the §2.1-edited
          header the recompiled program unit consumes), and the
          NativeLoaderContext.cpp TU (--loadercontext-source)
        + every existing ROOT-resolved dependency in the given .d files
          (--dependency-d) that is NOT already covered by the composition
          pin set: validate_inputs() files (pinned-external-inputs.json +
          snapshot-plan.json), the base build input-hashes.json originals,
          the W/include + W/src globs, and the base receipt unit sources.
          This is exactly "pre-seeded replace-receipt pins for every file the
          fresh program.d dependency walk touches" (§3.2.1).

include_dir_map is carried forward verbatim (bootstrap -I rewrite depends on
it). Run AFTER the controller's §2.1–§2.7 edits land; re-run whenever any
attested file changes. Fail-closed; no console contact; writes only its
--output under this worker's dir.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
SPIKE = ROOT / '.superpowers/native-dependency-spike'
OWNER = SPIKE / 'ue4ss-native-tick-file-owner-1'
CR1 = SPIKE / 'native-coherent-runtime-1'
BASE = SPIKE / 'ue4ss-native-notify-cancel-1/build-1/receipt.json'
CARRY_DEFAULT = (ROOT / 'outputs/2026-10-04-zcode-team/worker-query-s2-arm/'
                 'integration-patch/receipts/composition4-replace-receipt.json')
RUN3 = OWNER / 'ue4ss-native-supplier-composition-4/run-3'
PROGRAM_D = CR1 / 'coherent-full-1/program.d'
DEFAULT_OUTPUT = (Path(__file__).resolve().parent / 'receipts/composition6-replace-receipt.json')


def sha(p: Path) -> str:
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def deps(p: Path):
    return {str(Path(x).resolve()) for x in Path(p).read_text().replace('\\\n', ' ').split()[1:]
            if Path(x).is_file()}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    ap.add_argument('--program-source', type=Path,
                    default=ROOT / 'outputs/2026-10-04-zcode-team/worker-comp6-driver/'
                              'staging/comp6-program/UE4SSProgram.cpp',
                    help='staged (edited) UE4SSProgram.cpp copy for the retained '
                         '"program" replacement')
    ap.add_argument('--odr-header', type=Path,
                    default=CR1 / 'source/port/ue4ss/include/Platform/NativeProgramContext.hpp',
                    help='port-tree NativeProgramContext.hpp (edited in place by §2.1)')
    ap.add_argument('--loadercontext-source', type=Path,
                    default=OWNER / 'src/NativeLoaderContext.cpp',
                    help='NativeLoaderContext.cpp TU (edited in place by §2.3)')
    ap.add_argument('--carry-forward', type=Path, default=CARRY_DEFAULT,
                    help='composition4-replace-receipt.json; its files are re-attested '
                         'unmodified (drift refuses)')
    ap.add_argument('--dependency-d', action='append', default=[],
                    help='.d file whose walk must be covered (default: base program.d + '
                         'the run-3 fresh-unit .d files)')
    ap.add_argument('--file', action='append', default=[],
                    help='additional path to rr-admit (repeatable); use e.g. when the '
                         'port-tree UE4SSProgram.cpp is ALSO edited in place beside the '
                         'staged replacement copy')
    ap.add_argument('--base-receipt', type=Path, default=BASE)
    args = ap.parse_args()

    dep_files = [Path(x) for x in args.dependency_d] if args.dependency_d else \
        [PROGRAM_D, *sorted(RUN3.glob('*.d'))]
    for d in dep_files:
        if not d.is_file():
            raise SystemExit(f'dependency file missing: {d}')
    for p in (args.program_source, args.odr_header, args.loadercontext_source, args.carry_forward):
        if not p.is_file():
            raise SystemExit(f'required input missing: {p} (controller edits not landed?)')

    # Pin coverage set, mirroring build_runtime_composition6_fixed.py exactly.
    covered = set()
    pe = json.loads((CR1 / 'pinned-external-inputs.json').read_text())['files']
    covered |= {str(Path(ROOT / k).resolve()) for k in pe}
    sp = json.loads((CR1 / 'snapshot-plan.json').read_text())['files']
    covered |= {str((CR1 / 'source' / r['destination']).resolve()) for r in sp}
    base = json.loads(args.base_receipt.read_text())
    covered |= {str(Path(BASE.parent / 'input-hashes.json').resolve())}
    originals = json.loads((BASE.parent / 'input-hashes.json').read_text())
    covered |= {str(Path(k).resolve()) for k in originals}
    for k in originals:
        covered.add(str(Path(k).resolve()))
    covered |= {str(x) for x in [*OWNER.glob('include/*'), *OWNER.glob('src/*'),
                                 OWNER / 'build_runtime.py',
                                 SPIKE / 'ue4ss-native-tick-witness-1/include/patternsleuth_tick_file_witness.h']}
    for u in base['selected_runtime_units'].values():
        covered.add(str(Path(u['source']).resolve()))

    files = {}
    reasons = {}

    # 1. carry-forward (drift refuses) — EXCEPT files this change-set
    #    deliberately edits (the §2 set below): their carry-forward entries
    #    are superseded by the rr attestation of the EDITED content.
    #    Controller fix 2026-10-04: without this, the port header trips the
    #    composition-4 sha before section 2 can classify it as edited.
    edited_set = {str(x.resolve()) for x in
                  (args.program_source, args.odr_header, args.loadercontext_source)}
    carry = json.loads(args.carry_forward.read_text())
    for k, h in carry['files'].items():
        kp = Path(k)
        if str(kp.resolve()) in edited_set:
            continue
        if not kp.is_file():
            raise SystemExit(f'carry-forward file missing: {k}')
        got = sha(kp)
        if got != h:
            raise SystemExit(f'carry-forward file CHANGED since composition-4: {k}\n'
                             f'  measured {got}\n  attested {h}\n'
                             'composition-6 does not authorize this change; reconcile '
                             'provenance first')
        files[str(kp.resolve())] = got
        reasons[str(kp.resolve())] = 'carry-forward (composition-4 attestation, unmodified)'

    # 2. required Resolution-B files (current content; the edits are the point)
    for what, p in [('program staged copy', args.program_source),
                    ('port NativeProgramContext.hpp (ODR pair)', args.odr_header),
                    ('NativeLoaderContext.cpp', args.loadercontext_source)]:
        rp = str(p.resolve())
        files[rp] = sha(p)
        reasons[rp] = what

    # 3. explicit --file admissions
    for f in args.file:
        fp = Path(f)
        if not fp.is_file():
            raise SystemExit(f'--file missing: {f}')
        rp = str(fp.resolve())
        files[rp] = sha(fp)
        reasons[rp] = 'explicit --file admission'

    # 4. uncovered dependency-walk files
    uncovered = []
    for d in dep_files:
        for dp in deps(d):
            if dp in covered or dp in files:
                continue
            files[dp] = sha(dp)
            reasons[dp] = f'dependency-walk coverage ({d.name})'
            uncovered.append(dp)

    receipt = {
        'bundle': 'composition-6 (Resolution-B configured roots, worker-configured-'
                  'roots-design §3.2/§3.3): attested retained-unit replacement of '
                  '"program" + edited port header + loadercontext TU + every '
                  'dependency-walk file not already pinned',
        'provenance': {
            'generated_by': 'worker-comp6-driver/regen_replace_receipt.py',
            'carry_forward': str(args.carry_forward),
            'dependency_files': [str(d) for d in dep_files],
            'pin_coverage': 'validate_inputs (pinned-external-inputs.json + '
                            'snapshot-plan.json) + base input-hashes.json + W/include '
                            '+ W/src globs + base unit sources',
        },
        'files': dict(sorted(files.items())),
        'file_reasons': {k: reasons[k] for k in sorted(files)},
        'include_dir_map': carry.get('include_dir_map', {}),
        'include_dir_map_note': carry.get('include_dir_map_note', ''),
    }
    if args.output.exists():
        raise SystemExit(f'output exists (move it aside consciously first): {args.output}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(receipt, indent=1) + '\n')
    print(f'replace-receipt written: {args.output}')
    print(f'  files: {len(files)} (carry-forward {len(carry["files"])}, required 3, '
          f'uncovered-deps {len(uncovered)})')
    for k in sorted(files):
        tag = reasons[k]
        print(f'  {files[k][:12]}... {tag}: {k}')


if __name__ == '__main__':
    main()
