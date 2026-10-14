#!/usr/bin/env python3
"""Compare every upstream archive file to immutable source and report working source changes."""
from pathlib import Path
import tarfile,hashlib,json,sys
BASE=Path(__file__).resolve().parents[1]
def sha_bytes(raw):return hashlib.sha256(raw).hexdigest()
def inspect(name,work):
 archive=BASE/'downloads'/(name+'.tar.gz');source=BASE/'sources'/name
 mismatches=[];modified=[];count=0
 with tarfile.open(archive) as tf:
  for entry in tf:
   parts=Path(entry.name).parts
   if not entry.isfile() or len(parts)<2:continue
   relative=Path(*parts[1:]);expected=sha_bytes(tf.extractfile(entry).read());count+=1
   p=source/relative
   if not p.is_file() or sha_bytes(p.read_bytes())!=expected:mismatches.append(str(relative))
   wp=work/relative
   if not wp.is_file():modified.append({'path':str(relative),'change':'missing'})
   elif sha_bytes(wp.read_bytes())!=expected:modified.append({'path':str(relative),'source_sha256':expected,'derived_sha256':sha_bytes(wp.read_bytes())})
 return {'archive':str(archive.relative_to(BASE)),'archive_sha256':sha_bytes(archive.read_bytes()),'source':str(source),'files_verified':count,'immutable_mismatches':mismatches,'work_changes_to_upstream_files':modified}
def main():
 records=[inspect('mono-0cbf0e290c31adb476f9de0fa44b1d8829affa40',BASE/'runtime-work'),inspect('corefx-c4eeab9fc2faa0195a812e552cd73ee298d39386',BASE/'runtime-work/external/corefx')]
 allowed={'mono/metadata/icall.c','mono/utils/mono-dl.c'}
 ok=all(not r['immutable_mismatches'] for r in records) and {p['path'] for p in records[0]['work_changes_to_upstream_files']}==allowed and not records[1]['work_changes_to_upstream_files']
 result={'verdict':'PINNED_SOURCE_INTEGRITY_PASS' if ok else 'PINNED_SOURCE_INTEGRITY_FAIL','archives':records}
 (BASE/'evidence/SOURCE-INTEGRITY.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
 if not ok:raise SystemExit(1)
if __name__=='__main__':main()
