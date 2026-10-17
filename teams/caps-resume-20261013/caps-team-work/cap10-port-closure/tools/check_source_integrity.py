#!/usr/bin/env python3
"""Bind owned source changes to all files in the exact public source archives."""
from pathlib import Path
import argparse,hashlib,json,tarfile
HERE=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--dependency-base',type=Path,default=HERE.parent/'cap10-mono-build')
a=p.parse_args()
def sha(data):return hashlib.sha256(data).hexdigest()
def inspect(name,work):
 archive=a.dependency_base/'downloads'/(name+'.tar.gz');source=a.dependency_base/'sources'/name
 mismatch=[];modified=[];count=0
 with tarfile.open(archive) as tf:
  for entry in tf:
   parts=Path(entry.name).parts
   if not entry.isfile() or len(parts)<2:continue
   relative=Path(*parts[1:]);expected=sha(tf.extractfile(entry).read());count+=1
   orig=source/relative
   if not orig.is_file() or sha(orig.read_bytes())!=expected:mismatch.append(str(relative))
   dest=work/relative
   if not dest.is_file():modified.append(dict(path=str(relative),change='missing'))
   elif sha(dest.read_bytes())!=expected:modified.append(dict(path=str(relative),source_sha256=expected,derived_sha256=sha(dest.read_bytes())))
 return dict(archive=str(archive),archive_sha256=sha(archive.read_bytes()),files_verified=count,immutable_mismatches=mismatch,work_changes_to_upstream_files=modified)
records=[inspect('mono-0cbf0e290c31adb476f9de0fa44b1d8829affa40',HERE/'target-work'),inspect('corefx-c4eeab9fc2faa0195a812e552cd73ee298d39386',HERE/'target-work/external/corefx')]
expected={item['path']:item for item in json.loads((HERE/'evidence/CONFIGURATION-STAGE.json').read_text())['bindings'] if item['path']!='config.sub'}
loader=json.loads((HERE/'evidence/STRICT-LOADER-PATCH.json').read_text())
expected['mono/utils/mono-dl.c']=dict(source_sha256=loader['original_sha256'],derived_sha256=loader['derived_sha256'])
changes=records[0]['work_changes_to_upstream_files']
ok=all(not r['immutable_mismatches'] for r in records) and not records[1]['work_changes_to_upstream_files'] and {c['path'] for c in changes}==set(expected)
for c in changes:
 if c['path'] not in expected or any(c.get(key)!=expected[c['path']].get(key) for key in ['source_sha256','derived_sha256']):ok=False
result=dict(verdict='PINNED_SOURCE_INTEGRITY_PASS' if ok else 'PINNED_SOURCE_INTEGRITY_FAIL',archives=records)
(HERE/'evidence/SOURCE-INTEGRITY.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
raise SystemExit(0 if ok else 1)
