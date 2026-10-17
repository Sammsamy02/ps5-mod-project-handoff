from pathlib import Path
import json,subprocess,hashlib,os,sys
root=Path(__file__).resolve().parents[1];w=root.parents[1];out=root/'tests/target-active-width';out.mkdir(exist_ok=True)
legacy=w/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/src/NativeLayoutWidths.cpp'
source=out/'active_width_controls.cpp'
source.write_text('#include "'+str(legacy)+'"\n#include "NativeLayoutActiveWidths.hpp"\n'+r'''
namespace RC::Platform::NativeLayouts {
static_assert(!Find("UFunction","Func"));
static_assert(Find("UStruct","MinAlignment")==4);
static_assert(ActiveWidthDetail::FunctionWidth<Unreal::UFunction>()==8);
static_assert(ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>()==2);
static_assert(!Find("UClass","ClassCastFlags"));
static_assert(!Find("FFieldClass","CastFlags"));
static_assert(ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>()==8);
static_assert(ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>()==8);
static_assert(ActiveWidthDetail::SuperStructWidth<Unreal::UStruct>()==8);
static_assert(ActiveWidthDetail::FieldOwnerWidth<Unreal::FField>()==16);
static_assert(ActiveWidthDetail::FieldNextWidth<Unreal::FField>()==8);
static_assert(ActiveWidthDetail::FieldNameWidth<Unreal::FField>()==8);
static_assert(ActiveWidthDetail::PropertyFlagsWidth<Unreal::FProperty>()==8);
static_assert(ActiveWidthDetail::FunctionFlagsWidth<Unreal::UFunction>()==4);
struct UnknownGetter;
static_assert(!ActiveWidthDetail::FunctionWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::MinAlignmentWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::ClassCastFlagsWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldCastFlagsWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::SuperStructWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldOwnerWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldNextWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FieldNameWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::PropertyFlagsWidth<UnknownGetter>());
static_assert(!ActiveWidthDetail::FunctionFlagsWidth<UnknownGetter>());
extern "C" [[gnu::used]] const uint32_t ActiveWidthControlFacts[]{
Find("UFunction","Func").value_or(0),*Find("UStruct","MinAlignment"),
*ActiveWidthDetail::FunctionWidth<Unreal::UFunction>(),
*ActiveWidthDetail::MinAlignmentWidth<Unreal::UStruct>(),
*ActiveWidthDetail::ClassCastFlagsWidth<Unreal::UClass>(),
*ActiveWidthDetail::FieldCastFlagsWidth<Unreal::FFieldClass>(),
*ActiveWidthDetail::SuperStructWidth<Unreal::UStruct>(),
*ActiveWidthDetail::FieldOwnerWidth<Unreal::FField>(),
*ActiveWidthDetail::FieldNextWidth<Unreal::FField>(),
*ActiveWidthDetail::FieldNameWidth<Unreal::FField>(),
*ActiveWidthDetail::PropertyFlagsWidth<Unreal::FProperty>(),
*ActiveWidthDetail::FunctionFlagsWidth<Unreal::UFunction>()};
}
''')
commands=json.loads((w/'outputs/2026-10-07-l18-full-build/runs/run-l18-2/commands.json').read_text())
cmd=list(next(c['argv'] for c in commands if c['name']=='unreal-unrealinitializer'))
cmd[1:1]=['-I',str(root/'units'),'-I',str(w/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include'),'-I',str(w/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/include')]
for i,v in enumerate(cmd):
    if v.endswith('.cpp'):cmd[i]=str(source)
cmd[cmd.index('-o')+1]=str(out/'active-width-controls.o')
if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(out/'active-width-controls.d')
env=dict(os.environ);env['LLVM_CONFIG']=str(w/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
receipt={'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'static_assertions':['actual legacy Func/ClassCastFlags/CastFlags null; MinAlignment4','ten genuine public value widths:8/2/8/8/8/16/8/8/8/4','ten missing active getters unknown'],'owner_distinction':'whole public variant return16 bounds copy; measured native tagged-pointer storage8 unchanged','source_hashes':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [source,legacy,root/'units/NativeLayoutActiveWidths.hpp']}}
(root/'reports/ACTIVE-WIDTHS-TARGET.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('active target widths',r.returncode)
if r.returncode:print(r.stderr)
sys.exit(r.returncode)
