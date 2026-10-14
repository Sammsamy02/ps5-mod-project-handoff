from pathlib import Path
import json, hashlib, shutil

# [L27][EV string-seam] CHANGES sync (agent-EV 2026-10-09). Consumption law:
# candidate_sha256 MUST be recomputed after every further candidate edit
# (CHANGES.json scope note). The 18:19-18:22 seam edits touched four rowed
# files (LuaMod.cpp, NameTypes.cpp, NameTypes.hpp, UObjectGlobals.cpp) and
# introduced two header REPLACEMENTS that had no row at all —
# candidate/include/Helpers/{String,Utf16Utf8}.hpp (originals: the port's
# first/include/Helpers pair). Without those rows the parent build resolves
# <Helpers/...> from the unfixed port tree and only the NameTypes half of
# the seam fix ships. Rows are appended (order never mattered to the
# parent); copied_originals are staged under original/include/Helpers like
# every other row.

p = Path(__file__).resolve().parent.parent
rows = json.load(open(p / 'CHANGES.json'))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


new_originals = [
    Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp'),
    Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/Utf16Utf8.hpp'),
]

# 1. Recompute every candidate hash (fixes the four stale seam rows and
#    verifies the other 26+ are still exact).
fixed = []
for i, c in enumerate(rows['changes']):
    cur = sha(c['candidate'])
    if cur != c['candidate_sha256']:
        fixed.append((i, c['candidate'].split('typescan-r4/')[-1]))
        c['candidate_sha256'] = cur
for i, f in enumerate(rows['files']):
    match = next((c for c in rows['changes'] if c['candidate'] == f['candidate']), None)
    if match:
        f['candidate_sha256'] = match['candidate_sha256']

# 2. Append the two Helpers replacement rows (changes + files, same shape).
appended = []
for orig in new_originals:
    cand = p / 'candidate/include/Helpers' / orig.name
    copied = p / 'original/include/Helpers' / orig.name
    copied.parent.mkdir(parents=True, exist_ok=True)
    if not copied.exists():
        shutil.copy2(orig, copied)
    assert sha(orig) == sha(copied)
    row = {
        'original': str(orig),
        'copied_original': str(copied),
        'candidate': str(cand),
        'original_sha256': sha(orig),
        'candidate_sha256': sha(cand),
        'copied_original_sha256': sha(copied),
    }
    if not any(c['candidate'] == row['candidate'] for c in rows['changes']):
        rows['changes'].append(dict(row))
        rows['files'].append(dict(row))
        appended.append(orig.name)

(p / 'CHANGES.json').write_text(json.dumps(rows, indent=2))
print('recomputed rows:', fixed)
print('appended rows:', appended)
print('changes total:', len(rows['changes']), 'files total:', len(rows['files']))

# 3. Final verify pass: every row's hashes match the files on disk.
for c in rows['changes']:
    assert sha(c['candidate']) == c['candidate_sha256'], c['candidate']
    assert sha(c['copied_original']) == c['copied_original_sha256'], c['copied_original']
    assert sha(c['original']) == c['original_sha256'], c['original']
print('ev-changes-sync: ALL PASS')
