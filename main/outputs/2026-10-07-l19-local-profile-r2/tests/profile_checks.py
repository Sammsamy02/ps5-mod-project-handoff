from pathlib import Path
import subprocess,sys,json,hashlib
root=Path(__file__).resolve().parents[1];rows=[]
for name,extra in [('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]:
 out=root/'tests'/('profile-'+name);cmd=['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1',*extra,'-I',str(root/'units'),str(root/'tests/test_profile.cpp'),'-o',str(out)];r=subprocess.run(cmd,capture_output=True,text=True);rows.append({'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
 if r.returncode:print(r.stderr);break
 r=subprocess.run([str(out)],capture_output=True,text=True);rows.append({'argv':[str(out)],'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(name,r.stdout,r.stderr)
 if r.returncode:break
(root/'reports/PROFILE-CHECKS-GREEN.json').write_text(json.dumps({'commands':rows,'source_hashes':{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ['units/BuildFactsChecks.hpp','units/RuntimeBuildProfile.hpp','tests/test_profile.cpp']}},indent=2)+'\n');sys.exit(r.returncode)
