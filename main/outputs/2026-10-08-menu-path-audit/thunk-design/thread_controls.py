#!/usr/bin/env python3
from pathlib import Path
import json,subprocess,os,hashlib,sys
HERE=Path(__file__).resolve().parent;OUT=HERE/(sys.argv[1] if len(sys.argv)>1 else 'thread');OUT.mkdir(exist_ok=True)
cache=HERE.parent/'source-cache'
source=OUT/'race.cpp'; source.write_text('''#include <thread>
#include <array>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*,FFrame&,void*) { ++native_calls; }
int main() {
 UFunction a; a.func=original; a.GetFunctionHookData(); auto loaded=a.func;
 std::array<UFunction,6> other;for(auto& f:other)f.func=original;
 FFrame frame;frame.current=&a;UObject context;
 std::atomic<bool> go{};
 std::thread worker([&]{while(!go.load()){}for(auto& f:other)f.GetFunctionHookData();});
 go=true;for(int i=0;i<10000;++i)loaded(&context,frame,nullptr);
 worker.join();if(native_calls!=10000)return 1;
 std::cout << "10000 dispatches / 6 registrations complete\\n";
}
''')
original=(HERE/'actual_wrapper.cpp').read_text().replace('#include "host_model.hpp"','#include "../host_model.hpp"').replace('#include "cases.cpp"',f'#include "{source}"')
candidate=(HERE/'green-v3/actual_candidate.cpp').read_text().replace('#include "../green_cases.cpp"\n#include "../extra_green_cases.cpp"',f'#include "{source}"')
rows=[]
for name,text in [('original',original),('candidate',candidate)]:
 unit=OUT/(name+'.cpp');unit.write_text(text);binary=OUT/name
 command=['clang++','-std=c++20','-O1','-g','-fsanitize=thread','-pthread',str(unit),'-o',str(binary)]
 build=subprocess.run(command,capture_output=True,text=True);(OUT/(name+'-compile.stderr')).write_text(build.stderr)
 run=None
 if build.returncode==0:
  run=subprocess.run([str(binary)],capture_output=True,text=True,env=dict(os.environ,TSAN_OPTIONS='halt_on_error=1:exitcode=66'),timeout=30)
  (OUT/(name+'.stdout')).write_text(run.stdout);(OUT/(name+'.stderr')).write_text(run.stderr)
 rows.append({'name':name,'command':command,'compile_returncode':build.returncode,'run_returncode':run.returncode if run else None,'thread_race_detected':bool(run and 'ThreadSanitizer: data race' in run.stderr),'source_sha256':hashlib.sha256(unit.read_bytes()).hexdigest()})
print(json.dumps([{k:v for k,v in row.items() if k not in ('command','source_sha256')} for row in rows]))
(OUT/'THREAD-RECEIPT.json').write_text(json.dumps({'scope':'Exact original/candidate code under controlled HOST registration/dispatch threads, no native liveness claim','candidate_pins':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (HERE/'candidate').iterdir() if p.is_file()},'results':rows},indent=2)+'\n')
