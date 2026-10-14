#!/usr/bin/env python3
"""L10LM replace-receipt regeneration (worker-luamod-rebase, 2026-10-04).

Derives this worker's composition-6 replace-receipt from the r3-armed lineage
receipt (99 files) with EXACTLY four changes, all fail-closed:

  1-3. the three initializer staging admissions are re-pointed from
       worker-launch10-r3-armed/staging/initializer-l10-r3/ to THIS worker's
       byte-verified re-stage (staging/initializer-l10-r3/) - same content,
       same shas, new paths (scope isolation; L10LM E2);
   4. the rebased luamod source (staging/luamod-l10rebase/LuaMod.cpp,
      4540593a...) is ADDED as the attested retained-unit replacement source
      (L10LM E3; the frozen-script fresh-dependency gate for the luamod unit is
      fully pre-covered by the existing pin layers - measured 2026-10-04:
      1306/1306 pinned luamod.d deps covered, 0 rr additions needed).

include_dir_map carries verbatim. Refuses on any hash drift, missing file, or
unexpected staging key set. Writes only this worker's receipts dir. The r3-armed
receipt and staging are read-only inputs.
"""
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
SRC_RECEIPT = TEAM / 'worker-launch10-r3-armed/receipts/composition6-replace-receipt.json'
DST_RECEIPT = TEAM / 'worker-luamod-rebase/receipts/composition6-replace-receipt.json'
OLD_STAGE = TEAM / 'worker-launch10-r3-armed/staging/initializer-l10-r3'
NEW_STAGE = TEAM / 'worker-luamod-rebase/staging/initializer-l10-r3'
LUAMOD_SOURCE = TEAM / 'worker-luamod-rebase/staging/luamod-l10rebase/LuaMod.cpp'
STAGING_FILES = ['UnrealInitializer.cpp', 'SeedTable.h', 'SeedValidation.hpp']


def sha(p: Path) -> str:
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main() -> int:
    src = json.loads(SRC_RECEIPT.read_text())
    files = src['files']
    old_keys = {str((OLD_STAGE / f).resolve()): f for f in STAGING_FILES}
    for k in old_keys:
        if k not in files:
            print(f'STAGING KEY MISSING in source receipt: {k}', file=sys.stderr)
            return 1

    new_files = {}
    changes = []
    for k, h in files.items():
        if k in old_keys:
            f = old_keys[k]
            nk = str((NEW_STAGE / f).resolve())
            if not (NEW_STAGE / f).is_file():
                print(f're-staged file missing: {nk}', file=sys.stderr)
                return 1
            got = sha(NEW_STAGE / f)
            if got != h:
                print(f're-staged file DRIFTED vs r3-armed receipt: {f} {got} != {h}', file=sys.stderr)
                return 1
            new_files[nk] = got
            changes.append({'action': 'repoint-staging', 'file': f,
                            'from': k, 'to': nk, 'sha256': got})
        else:
            new_files[k] = h

    lp = str(LUAMOD_SOURCE.resolve())
    if lp in new_files:
        print('luamod source already present (unexpected)', file=sys.stderr)
        return 1
    if not LUAMOD_SOURCE.is_file():
        print(f'luamod source missing: {LUAMOD_SOURCE}', file=sys.stderr)
        return 1
    new_files[lp] = sha(LUAMOD_SOURCE)
    changes.append({'action': 'add-luamod-replacement-source', 'path': lp,
                    'sha256': new_files[lp]})

    # self-verify with the frozen receipt-loader semantics
    for k, h in new_files.items():
        p = Path(k)
        if not p.is_file() or sha(p) != h:
            print(f'SELF-VERIFY FAIL: {k}', file=sys.stderr)
            return 1

    out = {
        'bundle': ('composition-6 launch-10 L10LM (worker-luamod-rebase 2026-10-04): '
                   'attested retained-unit replacements of "program" (unchanged lineage) AND '
                   '"luamod" (rebased DIFF-1 FuncPtr-marshal drain on the notify-cancel overlay '
                   'lineage 0f0697a3..., no SettingsManager.hpp change) + the r3-armed staged '
                   'initializer re-staged in this worker dir + every dependency-walk file not '
                   'already pinned (luamod fresh-dep walk measured fully pre-covered: 1306/1306)'),
        'provenance': {
            'generated_by': 'worker-luamod-rebase/regen_receipt_l10lm.py',
            'derived_from': str(SRC_RECEIPT),
            'derived_from_sha256': sha(SRC_RECEIPT),
            'changes': changes,
            'luamod_fresh_dep_gate': '1306/1306 pinned luamod.d deps covered by validate_inputs pins + originals + W globs + base unit sources (measured 2026-10-04, 0 rr additions required beyond the replacement source itself)',
        },
        'files': dict(sorted(new_files.items())),
        'file_reasons': {k: 'carried verbatim from r3-armed lineage receipt' for k in new_files},
        'include_dir_map': src.get('include_dir_map', {}),
        'include_dir_map_note': src.get('include_dir_map_note', ''),
    }
    DST_RECEIPT.parent.mkdir(parents=True, exist_ok=True)
    DST_RECEIPT.write_text(json.dumps(out, indent=1) + '\n')
    print(f'receipt written: {DST_RECEIPT}')
    print(f'  files: {len(new_files)} (3 staging re-pointed, 1 luamod source added, {len(new_files)-4} carried)')
    print(f'  source receipt sha: {sha(SRC_RECEIPT)[:16]}...')
    return 0


if __name__ == '__main__':
    sys.exit(main())
