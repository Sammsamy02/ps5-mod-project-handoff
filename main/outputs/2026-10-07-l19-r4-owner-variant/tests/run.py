"""Actual selected owner-variant method regressions; no console contact.

Only engine object/version/offset storage is replaced. The class definition,
constructors, mutators, bounded owner read and traversal are extracted unchanged
from whichever source pair is passed. Expectations are literal native fixtures.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
WORKTREE = ROOT.parents[1]
SELECTED = WORKTREE / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal'


def block(text, anchor):
    start = text.index(anchor)
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[start:end]


def generate(header, source):
    h, c = header.read_text(), source.read_text()
    klass = block(h, 'class RC_UE_API FFieldVariant') + ';'
    methods = [
        'auto FField::GetOwnerVariant() -> FFieldVariant',
        'auto FField::GetFFieldOwnerUnsafe() -> FFieldVariant',
        'UObject* FField::GetOutermostOwner()',
        'void FField::SetOwnerVariant(FField* field)',
        'void FField::SetOwnerVariant(UObject* object)',
        'auto FFieldVariant::GetOwnerVariant() -> FFieldVariant',
        'void FFieldVariant::Set(FField* Field)',
        'void FFieldVariant::Set(UObject* Object)',
    ]
    return r'''
#include <cstdint>
#include <bit>
#include <cstring>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#define RC_UE_API
#define UE_FORCEINLINE_HINT inline
namespace RC::Unreal {
using int32 = int32_t;
struct UClass; struct FFieldClass;
struct alignas(8) UObject { uintptr_t reserved{}; UObject* GetOuterPrivate(); };
class FFieldVariant;
class FField {
public:
    unsigned char* owner{};
    auto GetOwner() -> FFieldVariant&;
    auto GetFFieldOwnerUnsafe() -> FFieldVariant;
    auto GetOwnerVariant() -> FFieldVariant;
    auto GetOutermostOwner() -> UObject*;
    void SetOwnerVariant(FField*);
    void SetOwnerVariant(UObject*);
    auto AsUFieldUnsafe() -> UObject* {return nullptr;}
};
namespace Version {
inline int major=5, minor=6;
bool IsAtLeast(int a,int b) {return major>a || (major==a && minor>=b);}
bool IsBelow(int a,int b) {return !IsAtLeast(a,b);}
}
''' + klass + r'''
auto FField::GetOwner() -> FFieldVariant& {return *reinterpret_cast<FFieldVariant*>(owner);}
inline UObject* expected_self{};
inline UObject* expected_outer{};
inline bool masked_self{};
UObject* UObject::GetOuterPrivate() {
    masked_self=this==expected_self;
    return masked_self ? expected_outer : nullptr;
}
''' + '\n'.join(block(c, anchor) for anchor in methods) + r'''
}
using namespace RC::Unreal;
static_assert(sizeof(FFieldVariant)==16); // retained legacy wrapper, not native size
bool check(bool ok,const char* detail) {
    if(!ok)std::cerr<<"FAIL "<<detail<<"\n";
    return ok;
}
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    std::string mode=argv[1];
    const bool legacy=mode.starts_with("legacy_");
    Version::major=mode.starts_with("legacy24_")?4:5;
    Version::minor=mode.starts_with("legacy24_")?24:(legacy?2:6);
    UObject object{},outer{};
    FField field{};
    bool ok=true;
    if(mode.ends_with("construct_object")) {
        FFieldVariant v(&object);
        ok=check(v.IsUObject()&&v.ToUObject()==&object&&v.ToField()==nullptr,
                 "object constructor must preserve owner kind and pointer");
    } else if(mode.ends_with("construct_field")) {
        FFieldVariant v(&field);
        ok=check(!v.IsUObject()&&v.ToField()==&field&&v.ToUObject()==nullptr,
                 "field constructor must preserve owner kind and pointer");
    } else if(mode.ends_with("null_object")) {
        FFieldVariant v(static_cast<UObject*>(nullptr));
        ok=check(!v.IsValid()&&v.ToUObject()==nullptr,"null remains invalid");
    } else if(mode.ends_with("set_object_local")) {
        FFieldVariant v(&field);v.Set(&object);
        ok=check(v.IsUObject()&&v.ToUObject()==&object,"Set object retains kind");
    } else if(mode.ends_with("set_field_local")) {
        FFieldVariant v(&object);v.Set(&field);
        ok=check(!v.IsUObject()&&v.ToField()==&field,"Set field retains kind");
    } else if(mode=="native_set_object_adjacent"||mode=="native_set_field_adjacent"||
              mode=="native_wrapper_set_object"||mode=="native_wrapper_set_field") {
        struct alignas(8) NativeOwnerAndNext {uintptr_t owner,next;} native{0,0x1122334455667788ULL};
        auto& view=*reinterpret_cast<FFieldVariant*>(&native.owner);
        field.owner=reinterpret_cast<unsigned char*>(&native.owner);
        const bool object_kind=mode=="native_set_object_adjacent"||mode=="native_wrapper_set_object";
        if(mode=="native_wrapper_set_object")field.SetOwnerVariant(&object);
        else if(mode=="native_wrapper_set_field")field.SetOwnerVariant(&field);
        else if(object_kind)view.Set(&object);else view.Set(&field);
        auto wanted=object_kind ? reinterpret_cast<uintptr_t>(&object)|1 : reinterpret_cast<uintptr_t>(&field);
        ok=check(native.owner==wanted&&native.next==0x1122334455667788ULL,
                 "native Set must change only8 owner bytes, preserve Next sentinel");
    } else if(mode=="native_owner_read_exact8") {
        auto native=std::make_unique<uintptr_t>(reinterpret_cast<uintptr_t>(&object)|1);
        field.owner=reinterpret_cast<unsigned char*>(native.get());
        auto v=field.GetFFieldOwnerUnsafe();
        ok=check(v.IsUObject()&&v.ToUObject()==&object,"native exact8 read");
    } else if(mode=="legacy_owner_read16") {
        Version::minor=2;FFieldVariant stored(&object);
        field.owner=reinterpret_cast<unsigned char*>(&stored);
        auto v=field.GetFFieldOwnerUnsafe();
        ok=check(v.IsUObject()&&v.ToUObject()==&object,"legacy16 read retains bool");
    } else if(mode=="native_tagged_traversal") {
        auto tagged=reinterpret_cast<uintptr_t>(&object)|1;
        FFieldVariant v(reinterpret_cast<FField*>(tagged));
        expected_self=&object;expected_outer=&outer;
        auto parent=v.GetOwnerVariant();
        ok=check(masked_self&&parent.IsUObject()&&parent.ToUObject()==&outer,
                 "traversal must mask UObject tag before method call");
    } else if(mode=="legacy_traversal") {
        FFieldVariant v(&object);expected_self=&object;expected_outer=&outer;
        auto parent=v.GetOwnerVariant();
        ok=check(masked_self&&parent.IsUObject()&&parent.ToUObject()==&outer,
                 "legacy traversal uses untagged pointer");
    } else if(mode=="native_null_traversal") {
        FFieldVariant v(static_cast<FField*>(nullptr));
        auto parent=v.GetOwnerVariant();
        ok=check(!parent.IsValid(),"null owner traversal must not dereference");
    } else if(mode=="native_null_outermost") {
        auto native=std::make_unique<uintptr_t>(0);
        field.owner=reinterpret_cast<unsigned char*>(native.get());
        ok=check(field.GetOutermostOwner()==nullptr,"null field has no outermost object");
    } else if(mode=="transition_53") {
        Version::minor=3;FFieldVariant v(&object);
        ok=check(v.IsUObject()&&v.ToUObject()==&object&&FFieldVariant::StaticSize()==8,
                 "version5.3 tag transition");
    } else return 2;
    if(ok)std::cout<<"PASS "<<mode<<"\n";
    return ok?0:1;
}
'''


def main():
    mode = sys.argv[1]
    assert mode in ('red', 'green', 'review_red')
    if mode == 'red':
        header = SELECTED / 'include/Unreal/FField.hpp'
        source = SELECTED / 'src/FField.cpp'
    elif mode == 'review_red':
        header, source = ROOT / 'before-review/FField.hpp', ROOT / 'before-review/FField.cpp'
    else:
        header, source = ROOT / 'candidate/FField.hpp', ROOT / 'candidate/FField.cpp'
    build = ROOT / 'tests' / mode
    build.mkdir(exist_ok=True)
    cpp = build / 'actual_owner_methods.cpp'
    cpp.write_text(generate(header, source))
    cases = ['native_construct_object','native_construct_field','native_null_object',
             'native_set_object_local','native_set_field_local','native_set_object_adjacent',
             'native_set_field_adjacent','native_owner_read_exact8','native_tagged_traversal',
             'legacy_construct_object','legacy_construct_field','legacy_null_object',
             'legacy_set_object_local','legacy_set_field_local','legacy_owner_read16',
             'legacy_traversal','transition_53','legacy24_construct_object','legacy24_construct_field',
             'legacy24_set_object_local','legacy24_set_field_local','native_wrapper_set_object',
             'native_wrapper_set_field','native_null_traversal','native_null_outermost']
    records = []
    for sanitized in (False, True):
        exe = build / ('controls-san' if sanitized else 'controls-plain')
        args = ['clang++','-std=c++20','-Wall','-Wextra','-Werror','-O1',str(cpp),'-o',str(exe)]
        if sanitized:
            args += ['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        c = subprocess.run(args,capture_output=True,text=True)
        records.append(dict(kind='compile',sanitized=sanitized,argv=args,exit=c.returncode,stdout=c.stdout,stderr=c.stderr))
        if c.returncode:
            print(c.stderr)
            break
        for case in cases:
            args=[str(exe),case]
            try:
                r=subprocess.run(args,capture_output=True,text=True,timeout=3)
            except subprocess.TimeoutExpired:
                r=subprocess.CompletedProcess(args,124,'','case exceeded3-second timeout')
            records.append(dict(kind='case',sanitized=sanitized,name=case,argv=args,exit=r.returncode,stdout=r.stdout,stderr=r.stderr))
    receipt=dict(mode=mode,scope='actual extracted class/methods; engine storage/version doubles; Mac-only',
                 sources={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),header,source,cpp)},
                 cases=len(cases),records=records)
    (ROOT / f'{mode.upper()}-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
    failures=[r for r in records if r['exit']]
    print(f'{mode}: {len(records)} commands; {len(failures)} failing; receipt saved')
    print('failed cases:',','.join(sorted({r.get('name','compile') for r in failures})))
    return int(bool(failures))


if __name__=='__main__':
    sys.exit(main())
