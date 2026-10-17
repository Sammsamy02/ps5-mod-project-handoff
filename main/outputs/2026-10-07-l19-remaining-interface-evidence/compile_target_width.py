from pathlib import Path
import hashlib, json, os, subprocess, sys

w = Path(__file__).resolve().parents[2]
o = Path(__file__).resolve().parent
command_source = w / 'outputs/2026-10-07-l19-local-profile-r2/reports/ACTIVE-WIDTHS-TARGET.json'
cmd = list(json.loads(command_source.read_text())['argv'])
for i, value in enumerate(cmd):
    if value.endswith('.cpp'):
        cmd[i] = str(o / 'target_interface_width.cpp')
cmd[cmd.index('-o') + 1] = str(o / 'target-interface-width.o')
if '-MF' in cmd:
    cmd[cmd.index('-MF') + 1] = str(o / 'target-interface-width.d')
env = dict(os.environ)
env['LLVM_CONFIG'] = str(w / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
r = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
paths = [command_source, Path(__file__).resolve(), o / 'target_interface_width.cpp',
         w / '.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp',
         w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp',
         w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UClass.hpp']
d = {'scope': 'local selected-target compile only; no getters executed; frozen R2 unchanged',
     'argv': cmd, 'exit': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr,
     'static_assertions': ['actual legacy provider UClass.Interfaces=16', 'actual public GetInterfaces value width=16',
                           'selected FImplementedInterface stride16 Class0 PointerOffset8 bImplementedByK2C'],
     'sources': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
if r.returncode == 0:
    d['object_sha256'] = hashlib.sha256((o / 'target-interface-width.o').read_bytes()).hexdigest()
(o / 'TARGET-WIDTH.json').write_text(json.dumps(d, indent=2) + '\n')
print('target interface width', r.returncode)
if r.returncode:
    print(r.stderr)
sys.exit(r.returncode)
