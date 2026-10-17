#!/usr/bin/env python3
"""Keep immutable run receipts for actual bridge and selected owner tests."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess,sys,uuid
root=Path(__file__).resolve().parents[1]
workspace=root.parents[1]
label=sys.argv[1] if len(sys.argv)>1 else 'GREEN'
run=root/'tests/runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+label+'-'+uuid.uuid4().hex[:8])
run.mkdir(parents=True)
def section(text,a,b):
 start=text.index(a);return text[start:text.index(b,start)]
bridge=(root/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text()
fixture=(root/'tests/test_retained.cpp').read_text()
supplier=workspace/'outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp'
generated={
 'sink_extract.inc':section(bridge,'// ---- HOST-TEST-EXTRACT-BEGIN','// Legacy explicit diagnostics only.').replace('std::chrono::steady_clock::now()','TestClock::now()'),
 'stop_extract.inc':section(bridge,'extern "C" int modbridge_runtime_stop(void)','extern "C" int modbridge_runtime_start_with_activation').replace('bridge_startup_resources_release();','observed_fallback_release();'),
 'kernel_adapters.inc':fixture[:fixture.index('namespace modbridge { namespace native {')],
 'supplier_stop.inc':section(supplier.read_text(),'auto StopRuntimeWithAck(','\n#ifndef NATIVE_ACK_SUPPLIER_HOST_TEST').replace('StopRuntimeWithAck','RealStopRuntimeWithAck',1),
 'control_d_extract.inc':section(bridge,'    // ---- Control D: candidate-memory copy','    // B failed: D is deliberately SKIPPED'),
}
if 'int bridge_copyin_initializer_enter(void)' not in bridge:
 generated['sink_extract.inc']+='\nextern "C" int bridge_copyin_initializer_enter(void){return 0;}\nextern "C" void bridge_copyin_initializer_complete(void){}\nextern "C" void bridge_copyin_initializer_leave(void){}\n'
for name,body in generated.items():(run/name).write_text(body)
rows=[]
modes=[('plain',[]),('san',['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'])]
if label!='RED':modes.append(('tsan',['-fsanitize=thread','-fno-sanitize-recover=all','-fno-omit-frame-pointer']))
for name,extra in modes:
 binary=run/name
 cmd=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-O1',*extra,'-I',str(run),'-I',str(root/'units'),'-I',str(workspace/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/activation'),str(root/'tests/test_lifecycle.cpp'),str(root/'units/runtime_owner.cpp'),'-pthread','-o',str(binary)]
 c=subprocess.run(cmd,capture_output=True,text=True)
 r=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30) if c.returncode==0 else None
 row={'name':name,'compile_argv':cmd,'compile_exit':c.returncode,'compile_stderr':c.stderr,'run_argv':[str(binary)],'run_exit':r.returncode if r else None,'stdout':r.stdout if r else '', 'stderr':r.stderr if r else '', 'cases':r.stdout.count('PASS ') if r else 0}
 rows.append(row);print(name,'compile',c.returncode,'run',row['run_exit']);print(row['stdout']);print(row['stderr'] or c.stderr)
receipt={'scope':'actual extracted bridge and selected RuntimeOwner; local only','red_compatibility':'missing lifecycle hooks receive test-only no-op definitions (enter returns0); frozen l16 can run but these stubs cannot satisfy l17 lifecycle assertions','sources':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [root/'units/runtime_start_bridge_r14_diag_spanpub.cpp',root/'units/runtime_owner.cpp',root/'units/CopyinStartup.hpp',root/'tests/test_retained.cpp',root/'tests/test_lifecycle.cpp']},'runs':rows}
(run/'RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
(root/'reports'/('LIFECYCLE-'+label+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
sys.exit(0 if all(r['compile_exit']==0 and r['run_exit']==0 for r in rows) else 1)
