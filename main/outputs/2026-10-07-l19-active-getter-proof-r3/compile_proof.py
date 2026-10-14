from pathlib import Path
import hashlib, json, os, subprocess, sys

w = Path(__file__).resolve().parents[2]
o = Path(__file__).resolve().parent
source_command = w / 'outputs/2026-10-07-l19-local-profile-r2/reports/ACTIVE-WIDTHS-TARGET.json'
cmd = list(json.loads(source_command.read_text())['argv'])
for i, value in enumerate(cmd):
    if value.endswith('.cpp'):
        cmd[i] = str(o / 'target_six_getters.cpp')
cmd[cmd.index('-o') + 1] = str(o / 'target-six-getters.o')
if '-MF' in cmd:
    cmd[cmd.index('-MF') + 1] = str(o / 'target-six-getters.d')
env = dict(os.environ)
env['LLVM_CONFIG'] = str(w / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
r = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
base = w / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal'
paths = [source_command, Path(__file__).resolve(), o / 'target_six_getters.cpp',
         w / '.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp',
         base / 'include/Unreal/CoreUObject/UObject/Class.hpp', base / 'include/Unreal/FField.hpp',
         base / 'include/Unreal/CoreUObject/UObject/UnrealType.hpp',
         base / 'src/UStruct.cpp', base / 'src/FField.cpp', base / 'src/FProperty.cpp', base / 'src/UFunction.cpp']
d = {'scope': 'local selected-target public getter signature proof; frozen R2 unchanged; no getters executed',
     'argv': cmd, 'exit': r.returncode, 'stdout': r.stdout, 'stderr': r.stderr,
     'legacy_six_widths': [None, None, None, None, None, None],
     'public_six_value_widths': [8, 16, 8, 8, 8, 4] if r.returncode == 0 else None,
     'keys': ['UStruct.SuperStruct', 'FField.Owner', 'FField.Next', 'FField.NamePrivate',
              'FProperty.PropertyFlags', 'UFunction.FunctionFlags'],
     'owner_distinction': 'Native physical tagged pointer8; whole selected public FFieldVariant return value16 including legacy bool; sizeof return is binding access bound, not native ABI proof.',
     'missing_getter_controls': 6,
     'sources': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
if r.returncode == 0:
    d['object_sha256'] = hashlib.sha256((o / 'target-six-getters.o').read_bytes()).hexdigest()
(o / 'TARGET-SIX-GETTERS.json').write_text(json.dumps(d, indent=2) + '\n')
print('target six genuine public getters', r.returncode)
if r.returncode:
    print(r.stderr)
sys.exit(r.returncode)
