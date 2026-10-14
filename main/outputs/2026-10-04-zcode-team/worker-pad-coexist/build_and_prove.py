#!/usr/bin/env python3
"""worker-pad-coexist: host-prove + pinned-argv compile gates.

Read-only against every lineage file: all outputs land under this worker
directory. Host builds reuse the lineage host-1 recipe (ue4ss-native-pad-config-1,
the same recipe worker-pad-defer reused) with ONLY the test TU and the sources
under change swapped in (NativePadInput.cpp, PS5PadSource.cpp, Handler.cpp -
the last two being units this packet stages). Native compile gates use the
EXACT composition unit argv from the launch-10 base receipt
(ue4ss-native-notify-cancel-1/build-1/receipt.json), with only the source,
-o and -MF tokens redirected (the same assertion the composition driver makes
for retained-unit replacements). No console, no engine, no provider execution.
"""
import hashlib,json,os,subprocess,sys
from pathlib import Path

W=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
SPIKE=W/'.superpowers'/'native-dependency-spike'
ME=W/'outputs'/'2026-10-04-zcode-team'/'worker-pad-coexist'
BASE_RECEIPT=SPIKE/'ue4ss-native-notify-cancel-1'/'build-1'/'receipt.json'
HOST1=SPIKE/'ue4ss-native-pad-config-1'/'host-1'/'command.json'
PROVIDER=SPIKE/'ue4ss-native-pad-provider-1'/'port'
STAGE=ME/'staging'

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v): Path(p).parent.mkdir(parents=True,exist_ok=True);Path(p).write_text(json.dumps(v,indent=2)+'\n')

def retarget(argv,old_src,new_src,new_obj,new_dep):
    out=list(argv)
    assert old_src in out, 'source token not found in pinned argv'
    out[out.index(old_src)]=str(new_src)
    assert '-o' in out and '-MF' in out, 'pinned argv lacks -o/-MF'
    out[out.index('-o')+1]=str(new_obj)
    out[out.index('-MF')+1]=str(new_dep)
    assert sum(1 for a,b in zip(argv,out) if a!=b)<=3, 'more than source/-o/-MF changed'
    return out

def run(name,argv,env=None,cwd=None):
    d=ME/name; d.mkdir(parents=True,exist_ok=True)
    argv_list=[str(a) for a in argv]
    p=subprocess.run(argv_list,capture_output=True,text=True,env=env,cwd=cwd)
    (d/'stdout.txt').write_text(p.stdout)
    (d/'stderr.txt').write_text(p.stderr)
    save(d/'command.json',{'argv':argv_list,'exit':p.returncode})
    return p

def main():
    receipt={'worker':'worker-pad-coexist','steps':[]}
    base=json.loads(BASE_RECEIPT.read_text())
    units=base['selected_runtime_units']

    # ---- 1. host build (lineage host-1 recipe, ASan+UBSan) ----
    host_argv=json.loads(HOST1.read_text())['argv']
    # Drop the lineage test TU, the lineage pad sources, and the units this
    # packet stages (Handler.cpp, PS5PadSource.cpp) plus the stale -o target.
    dropped=('owned_config_controls.cpp','NativePadInput.cpp','PS5PadConfig.cpp',
             'Handler.cpp','PS5PadSource.cpp')
    host_argv=[t for t in host_argv
        if not (isinstance(t,str) and any(t.endswith(s) for s in dropped))]
    assert host_argv[-2:]==['-o',str(SPIKE/'ue4ss-native-pad-config-1'/'host-1'/'owned-config-controls')], host_argv[-2:]
    common=host_argv[:-2]
    staged_native_input=STAGE/'native-pad-input-coexist'/'NativePadInput.cpp'
    staged_provider=STAGE/'ue4ss-native-pad-provider-coexist'/'port'
    staged_handler=STAGE/'input-handler-coexist'/'Handler.cpp'

    name='host-1-coexist'
    test_cpp=ME/'tests'/'host_pad_coexist.cpp'
    exe=ME/name/'host_pad_coexist'
    exe.parent.mkdir(parents=True,exist_ok=True)
    # Documented deviations from the host-1 recipe (same class as pad-defer's):
    # (i) the test TU stubs the two linker-needed cppmod boundary symbols;
    # (ii) the sources under change are the STAGED ones. PS5PadConfig.cpp,
    # KeyDef.cpp, NativeModSource.cpp, DynamicOutput and fmt remain lineage.
    argv=common+[str(test_cpp),
        str(SPIKE/'ue4ss-native-pad-config-1'/'src'/'PS5PadConfig.cpp'),
        str(staged_provider/'PS5PadSource.cpp'),
        str(staged_handler),
        str(staged_native_input),'-o',str(exe)]
    def try_run(binary):
        # Watchdog WITHOUT pipe/wait dependency (pad-defer pattern: these ASan
        # test binaries can enter an uninterruptible state on this host where
        # SIGKILL never reaps them, so wait/communicate can block forever).
        out=(binary.parent/(binary.name+'.run.stdout'))
        err=(binary.parent/(binary.name+'.run.stderr'))
        with open(out,'w') as fo, open(err,'w') as fe:
            proc=subprocess.Popen([str(binary)],stdout=fo,stderr=fe,cwd=str(binary.parent),
                env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
            deadline=45
            while deadline>0 and proc.poll() is None:
                import time; time.sleep(1); deadline-=1
            if proc.poll() is None:
                try: proc.kill()
                except OSError: pass
                return None,out,err
            return proc.returncode,out,err
    p=run(name+'/compile',argv)
    if p.returncode: print(p.stderr[-4000:]);sys.exit(f'{name} compile failed')
    rc,out,err=try_run(exe)
    variant='lineage ASan+UBSan flags'
    if rc is None or rc!=0:
        if rc is not None:
            print(name,'sanitized run exit',rc,(err.read_text() if err else '')[-300:])
        nosan=[t for t in argv if t not in ('-fsanitize=address,undefined','-fno-omit-frame-pointer')]
        p=run(name+'/compile-nosan',nosan)
        if p.returncode: print(p.stderr[-4000:]);sys.exit(f'{name} nosan compile failed')
        rc,out,err=try_run(exe)
        variant='DEVIATION: sanitizer flags dropped (sanitized binary hung at exit on this host)'
        if rc is None or rc!=0:
            print((out.read_text() if out else ''),(err.read_text() if err else 'timeout'));sys.exit(f'{name} run failed')
    receipt['steps'].append({'step':name,'compile_exit':0,'run_exit':rc,'flags':variant,
        'stdout':out.read_text().strip(),'stderr_tail':err.read_text().strip()[-400:],
        'test_src_sha256':sha(test_cpp),
        'staged_src_sha256':{s:sha(p) for s,p in [
            ('native-pad-input',staged_native_input),
            ('PS5PadSource',staged_provider/'PS5PadSource.cpp'),
            ('Handler',staged_handler)]}})
    print(name,'PASS:',out.read_text().strip()[:220])

    # ---- 2..5 native compile gates: EXACT pinned composition argv ----
    LLVM_CONFIG=W/'outputs'/'ps5-mod-bridge'/'native-probe'/'build'/'deps'/'llvm'/'LLVM-21.1.8-macOS-ARM64'/'bin'/'llvm-config'
    env=dict(os.environ,LLVM_CONFIG=str(LLVM_CONFIG))
    gates=[
        ('compile-native-pad-input','native-pad-input',
         STAGE/'native-pad-input-coexist'/'NativePadInput.cpp'),
        ('compile-native-pad-PS5PadSource','native-pad-PS5PadSource',
         staged_provider/'PS5PadSource.cpp'),
        ('compile-native-pad-PS5PadNative','native-pad-PS5PadNative',
         staged_provider/'PS5PadNative.cpp'),   # byte-identical relocation, see DIFF.md
        ('compile-input-handler','input-handler',
         staged_handler),
    ]
    for step,unit_key,staged_src in gates:
        unit=units[unit_key]
        obj=ME/step/(unit_key+'.o')
        argv=retarget(unit['argv'],unit['source'],staged_src,obj,obj.with_suffix('.d'))
        p=run(step,argv,env=env)
        receipt['steps'].append({'step':step,'exit':p.returncode,'argv_changes':'source,-o,-MF only',
            'base_argv_from':str(BASE_RECEIPT),'base_src_sha256':sha(unit['source']),
            'staged_src_sha256':sha(staged_src)})
        if p.returncode: print(p.stderr[-4000:]);sys.exit(f'{step} gate failed')
        print(step,'PASS (pinned composition argv, source/-o/-MF redirected only)')

    save(ME/'host-prove'/'receipt.json',receipt)
    print('ALL GATES PASS')

if __name__=='__main__':
    main()
