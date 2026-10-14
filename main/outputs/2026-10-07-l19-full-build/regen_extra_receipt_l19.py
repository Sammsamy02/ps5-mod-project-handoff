#!/usr/bin/env python3
"""Regenerate the composition-6 extra-receipt (RESOLUTION-DESIGN §3.3 step 2).

Produces the hash-pinned attestation consumed by build_runtime_composition6.py
via --extra-receipt for the extra-unit channel (proven proven by rehearsals
R2/R3 in worker-composition-replace-mechanics; the fixed script records this
receipt into the run receipt and refuses to run extra units without it):

  files = carry-forward of composition4-extra-receipt.json (query harness +
          ack supplier, byte-drift REFUSES) + the adapter source
          (--adapter-source; the §2.5-edited lmf_to_native_loader_facts.cpp,
          canonical W/src path or a redirected staging copy) + its contract
          header (--adapter-header; the §2.4-edited lmf hpp) + any further
          --file entries (e.g. co-staged headers next to a redirected copy).

Run AFTER the controller's §2.4/§2.5 adapter edits land; re-run whenever any
attested file changes. Fail-closed; no console contact; writes only its
--output under this worker's dir.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OWNER = ROOT / '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1'
CARRY_DEFAULT = (ROOT / 'outputs/2026-10-04-zcode-team/worker-query-s2-arm/'
                 'integration-patch/receipts/composition4-extra-receipt.json')
DEFAULT_OUTPUT = (Path(__file__).resolve().parent / 'receipts/composition6-extra-receipt.json')


def sha(p: Path) -> str:
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--output', type=Path, default=DEFAULT_OUTPUT)
    ap.add_argument('--adapter-source', type=Path, default=OWNER / 'src/lmf_to_native_loader_facts.cpp',
                    help='the (edited) adapter TU compiled as extra unit lmf-loader-adapter')
    ap.add_argument('--adapter-header', type=Path, default=OWNER / 'include/lmf_to_native_loader_facts.hpp',
                    help='the (edited) adapter contract header (§2.4)')
    ap.add_argument('--file', action='append', default=[],
                    help='additional file to attest (repeatable)')
    ap.add_argument('--carry-forward', type=Path, default=CARRY_DEFAULT,
                    help='composition4-extra-receipt.json; its files are re-attested '
                         'unmodified (drift refuses)')
    args = ap.parse_args()

    for p in (args.adapter_source, args.adapter_header, args.carry_forward):
        if not p.is_file():
            raise SystemExit(f'required input missing: {p}')

    files = {}
    reasons = {}
    carry = json.loads(args.carry_forward.read_text())
    for k, h in carry['files'].items():
        kp = Path(k)
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
    for what, p in [('adapter source (lmf-loader-adapter extra unit)', args.adapter_source),
                    ('adapter contract header (§2.4)', args.adapter_header)]:
        rp = str(p.resolve())
        files[rp] = sha(p)
        reasons[rp] = what
    for f in args.file:
        fp = Path(f)
        if not fp.is_file():
            raise SystemExit(f'--file missing: {f}')
        rp = str(fp.resolve())
        files[rp] = sha(fp)
        reasons[rp] = 'explicit --file'

    receipt = {
        'bundle': 'composition-6 extra units (lmf-loader-adapter Resolution-B §2.4/§2.5 '
                  'revision + unchanged query harness + ack supplier)',
        'provenance': {
            'generated_by': 'worker-comp6-driver/regen_extra_receipt.py',
            'carry_forward': str(args.carry_forward),
            'adapter_channel': 'proven R2/R3 (worker-composition-replace-mechanics): '
                               'new source bytes + this updated hash attestation; the '
                               'extra unit recompiles fresh every composition run',
        },
        'files': dict(sorted(files.items())),
        'file_reasons': {k: reasons[k] for k in sorted(files)},
    }
    if args.output.exists():
        raise SystemExit(f'output exists (move it aside consciously first): {args.output}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(receipt, indent=1) + '\n')
    print(f'extra-receipt written: {args.output}')
    print(f'  files: {len(files)}')
    for k in sorted(files):
        print(f'  {files[k][:12]}... {reasons[k]}: {k}')


if __name__ == '__main__':
    main()
