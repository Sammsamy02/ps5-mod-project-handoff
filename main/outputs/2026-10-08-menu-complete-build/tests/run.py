#!/usr/bin/env python3
import hashlib
import json
import subprocess
from pathlib import Path

BASE=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=Path(__file__).resolve().parents[1]
OLD=BASE/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp'
assert hashlib.sha256(OLD.read_bytes()).hexdigest()=='50192ef184ed2cd5267a4eb21c2466de29466419726fd1afac981258070f18e3'
HEADER=BASE/'.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/LuaType/LuaUFunction.hpp'
assert hashlib.sha256(HEADER.read_bytes()).hexdigest()=='5368ea3051282c06667cf37d2cdfeb0e668464faeebf2cb8b35fb02e86123c0f'
TESTS=['small','boundary','empty','nonparm_ignored','array_extent','return_extent','oversized','max_size','beyond_declared','straddles_capacity','negative_offset','negative_element','zero_element','negative_count','zero_count','large_product','late_bad_parameter','bad_return']

def build(source, name, sanitize=False):
    directory=OUT/'tests'/name
    directory.mkdir(exist_ok=True)
    header=HEADER.read_text()
    frame_start=header.index('    struct DynamicUnrealFunctionData\n')
    frame_end=header.index('    };',frame_start)+7
    (directory/'actual_frame.inc').write_text(header[frame_start:frame_end]+'\n')
    text=source.read_text()
    start=text.rindex('    static auto call_ufunction_from_lua_impl(')
    brace=text.index('    {\n', start)
    end=text.index('        calling_context->ProcessEvent(func, dynamic_unreal_function_data.data);', brace)
    end=text.index('\n', end)+1
    # Replace only the conditional signature prologue; statements from the
    # original opening brace through ProcessEvent are copied without edits.
    body='static auto call_ufunction_from_lua_impl(const LuaMadeSimple::Lua& lua, FNativeUFunctionCall& Invocation) -> int\n'+text[brace:end]+'        return 0;\n    }\n'
    (directory/'actual_pre_call.inc').write_text(body)
    exe=directory/'controls'
    argv=['/usr/bin/clang++','-std=c++20','-O0','-g','-D__PROSPERO__','-I',str(directory),str(OUT/'tests/harness.cpp'),'-o',str(exe)]
    if sanitize: argv[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
    compile=subprocess.run(argv,capture_output=True,text=True)
    (directory/'compile.stdout').write_text(compile.stdout)
    (directory/'compile.stderr').write_text(compile.stderr)
    (directory/'COMPILE.json').write_text(json.dumps(dict(argv=argv,returncode=compile.returncode),indent=2)+'\n')
    assert compile.returncode==0, compile.stderr
    return exe

def run(exe, names, output):
    results=[]
    for name in names:
        r=subprocess.run([str(exe),name],capture_output=True,text=True)
        results.append(dict(case=name,returncode=r.returncode,stdout=r.stdout,stderr=r.stderr))
    (OUT/'tests'/output).write_text(json.dumps(results,indent=2)+'\n')
    return results

if __name__=='__main__':
    import sys
    mode=sys.argv[1]
    if mode=='red':
        executable=build(OLD,'original')
        # Non-UB controls show baseline accepts invalid declared sizes/extents.
        results=run(executable,['small','oversized','max_size','negative_element','zero_element','negative_count','zero_count','large_product','late_bad_parameter','bad_return'],'RED.json')
        assert results[0]['returncode']==0
        assert all(x['returncode']==1 for x in results[1:])
        asan=build(OLD,'original-asan',True)
        faults=run(asan,['straddles_capacity','negative_offset'],'RED-ASAN.json')
        assert all(x['returncode']!=0 and 'AddressSanitizer' in x['stderr'] for x in faults)
        print('RED: normal case passes; 9 invalid metadata cases accepted incorrectly; 2 real extracted-body ASan buffer faults')
    elif mode=='green':
        source=OUT/'candidate/LuaUObject.cpp'
        plain=build(source,'candidate')
        results=run(plain,TESTS,'GREEN.json')
        assert all(x['returncode']==0 for x in results), results
        asan=build(source,'candidate-asan',True)
        results=run(asan,TESTS,'GREEN-ASAN.json')
        assert all(x['returncode']==0 for x in results), results
        print('GREEN: 18 plain + 18 fatal ASan/UBSan actual pre-call body controls PASS')
    else: raise SystemExit(mode)
