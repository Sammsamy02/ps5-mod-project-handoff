import hashlib
import json
import re
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT = ROOT / 'outputs/2026-10-07-l15-fname-adapter/audit/engine-calls'
receipt = ROOT / 'outputs/2026-10-07-l14-full-build/runs/run-l14-2/receipt.json'
units = json.loads(receipt.read_text())['selected_runtime_units']
files = {}
missing = []
def absolute(s):
    p = Path(s)
    return p if p.is_absolute() else ROOT / p
for n, u in units.items():
    files.setdefault(str(absolute(u['source'])), set()).add(n)
    dep = absolute(u['dependency_file'])
    if not dep.exists():
        missing.append(str(dep))
        continue
    body = dep.read_text().replace('\\\n', ' ').split(':', 1)[1]
    for f in body.split():
        if f.endswith(('.h', '.hpp', '.hxx', '.inl', '.cpp', '.cc', '.c')):
            p = absolute(f)
            if str(p).startswith(str(ROOT)):
                files.setdefault(str(p), set()).add(n)
texts = {p: Path(p).read_text(errors='replace') for p in files if Path(p).is_file()}
manifest = {
    'receipt': str(receipt),
    'selected_units': [{k:u.get(k) for k in ['name', 'source', 'source_sha256', 'dependency_file', 'object', 'object_sha256']} for u in units.values()],
    'local_files': [{'path':p, 'sha256':hashlib.sha256(Path(p).read_bytes()).hexdigest(), 'selected_by': sorted(files[p])} for p in sorted(texts)],
    'missing_dependency_files': missing,
    'missing_source_or_dependency_paths': sorted(set(files)-set(texts)),
}
(OUT/'source-manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
patterns = {
    'Function': r'\bFunction\s*<',
    'virtual_helper': r'IMPLEMENT_UNREAL_VIRTUAL|DEFINE_UNREAL_VIRTUAL|VIRTUAL_FUNCTION|VirtualFunctionHelper|CallVirtual|call_virtual',
    'direct_cast': r'reinterpret_cast|static_cast<[^>]*(?:\(\*|FuncPtr|FunctionPtr)|\b[A-Za-z0-9_]*(?:func|Func|function|Function|trampoline|Trampoline)[A-Za-z0-9_]*\s*\(',
    'text_width': r'FORCE_U16|CharType\s*=|using TCHAR|typedef .*TCHAR|PLATFORM_TCHAR_IS_4_BYTES|SIZEOF_WCHAR|sizeof\(.*(?:TCHAR|CharType|wchar_t|char16_t)',
}
hits = {k:[] for k in patterns}
for p, text in sorted(texts.items()):
    for i,l in enumerate(text.splitlines(),1):
        for k, pat in patterns.items():
            if re.search(pat,l):
                hits[k].append({'path':p, 'line':i, 'text':l, 'selected_by':sorted(files[p])})
(OUT/'raw-pattern-hits.json').write_text(json.dumps(hits, indent=2)+'\n')
print('units',len(units),'local files',len(texts),'missing dependency files',len(missing),flush=True)
print({k:len(v) for k,v in hits.items()},flush=True)
for h in hits['Function']:
    if '/unreal/' in h['path'] or '/ue4ss/' in h['path']:
        print(h['path'].replace(str(ROOT)+'/', ''),h['line'],h['text'].strip())
