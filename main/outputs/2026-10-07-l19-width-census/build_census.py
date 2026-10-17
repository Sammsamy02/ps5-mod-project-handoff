from pathlib import Path
import json,subprocess,hashlib,os,sys
r=Path('.').resolve();o=r/'outputs/2026-10-07-l19-width-census';p=r/'outputs/2026-10-07-l19-local-profile-r2';legacy=r/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp'
profile=json.loads((p/'profiles/townfall-build-facts.json').read_text())
rows=[x for x in profile['required_members'] if 'UEP_TotalSize' not in x['name']]
expr=[]
for x in rows:
 cls,key=x['name'].split('::');
 special={('UFunction','Func'):'FunctionWidth<Unreal::UFunction>',('UStruct','MinAlignment'):'MinAlignmentWidth<Unreal::UStruct>',('UClass','ClassCastFlags'):'ClassCastFlagsWidth<Unreal::UClass>',('FFieldClass','CastFlags'):'FieldCastFlagsWidth<Unreal::FFieldClass>'}.get((cls,key))
 expr.append('ActiveWidthDetail::'+special+'().value_or(0)' if special else 'Find('+json.dumps(cls)+','+json.dumps(key)+').value_or(0)')
src=o/'census.cpp';src.write_text('#include '+json.dumps(str(legacy))+'\n#include "NativeLayoutActiveWidths.hpp"\nnamespace RC::Platform::NativeLayouts { extern "C" [[gnu::used]] const uint32_t FullAccessorWidthFacts[]{\n'+',\n'.join(expr)+'\n}; }\n')
cmd=list(json.loads((p/'reports/ACTIVE-WIDTHS-TARGET.json').read_text())['argv'])
for i,x in enumerate(cmd):
 if x.endswith('.cpp'):cmd[i]=str(src)
cmd[cmd.index('-o')+1]=str(o/'census.o');cmd[cmd.index('-MF')+1]=str(o/'census.d')
env=dict(os.environ);env['LLVM_CONFIG']=str(r/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
z=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
rec={'scope':'Local compile-only all-required-accessor-width census, no frozen source changes/game execution','argv':cmd,'exit':z.returncode,'stdout':z.stdout,'stderr':z.stderr,'fields':rows,'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'profile_sha256':hashlib.sha256((p/'profiles/townfall-build-facts.json').read_bytes()).hexdigest()}
(o/'COMPILE.json').write_text(json.dumps(rec,indent=2)+'\n');print('compile',z.returncode,'fields',len(rows));print(z.stderr[-1200:]);sys.exit(z.returncode)
