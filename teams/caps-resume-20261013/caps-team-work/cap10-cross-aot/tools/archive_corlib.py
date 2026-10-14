"""Archive the verified target corlib object without linking a PRX."""
from pathlib import Path
import hashlib,json,subprocess
H=Path(__file__).resolve().parents[1]
obj=H/'artifacts/mscorlib.full-interp.ps5.o';out=H/'artifacts/libmscorlib-full-interp-ps5.a'
expected=json.loads((H/'evidence/ARTIFACT-VERIFICATION.json').read_text())['object_sha256']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(obj)==expected
assert not out.exists()
ar=Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/tools/LLVM-21.1.8-macOS-ARM64/bin/llvm-ar')
assert sha(ar)=='c2640fc0a747413fe27818531d4a22f979d91b7e299723a68225917c42574b50'
cmd=[str(ar),'rcsD',str(out),str(obj)]
r=subprocess.run(cmd,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
with (H/'logs/TARGET-ARCHIVE.log').open('x') as f:f.write(r.stdout)
assert r.returncode==0,r.stdout
p=subprocess.run([str(ar),'p',str(out),obj.name],stdout=subprocess.PIPE,check=True)
assert hashlib.sha256(p.stdout).hexdigest()==expected
record={'command':cmd,'archiver_sha256':sha(ar),'exit_code':r.returncode,'object_sha256':expected,'archive_sha256':sha(out),'archive_bytes':out.stat().st_size,'archive_member_byte_exact':True,'target_execution':False,'prx_build_or_package':False}
with (H/'evidence/STATIC-ARCHIVE.json').open('x') as f:json.dump(record,f,indent=2);f.write('\n')
print(json.dumps(record,indent=2))
