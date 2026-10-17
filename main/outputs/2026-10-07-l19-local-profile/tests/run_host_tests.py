from pathlib import Path
import hashlib,json,subprocess,shutil
root=Path(__file__).resolve().parent.parent
out=root/'reports/HOST-TESTS.json'
if out.exists():
 old=root/'reports/HOST-TESTS-initial.json'
 if not old.exists():shutil.copy2(out,old)
rows=[]
for test in ('adapter','checks'):
 for san in (False,True):
  exe=root/'tests'/f'{test}-{"san" if san else "test"}'
  args=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-I',str(root/'units')]
  if san:
   args+=['-fsanitize=address,undefined']
   if test=='adapter':args+=['-DSKIP_LEGACY_NEGATIVE=1']
  args+=[str(root/'tests'/f'test_{test}.cpp'),'-o',str(exe)]
  c=subprocess.run(args,text=True,capture_output=True)
  row={'test':test,'sanitizer':san,'legacy_negative':test=='adapter' and not san,'compile_argv':args,'compile_exit':c.returncode,'compile_stderr':c.stderr}
  if c.returncode==0:
   r=subprocess.run([str(exe)],text=True,capture_output=True)
   row.update(run_exit=r.returncode,stdout=r.stdout,stderr=r.stderr)
  rows.append(row)
  print(test,'san' if san else 'normal',row.get('run_exit',c.returncode),row.get('stdout','').strip())
out.write_text(json.dumps({'runs':rows,'source_hashes':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'tests/test_adapter.cpp',root/'tests/test_checks.cpp',root/'units/NativeFNameAdapter.hpp',root/'units/NativeFNameChecks.hpp']},'sanitizer_policy':'ASan and all UBSan checks enabled. Deliberate legacy wrong-function-call negative runs only in isolated normal-build child; sanitized build omits that UB experiment.'},indent=2)+'\n')
assert all(r.get('compile_exit')==0 and r.get('run_exit')==0 for r in rows)
