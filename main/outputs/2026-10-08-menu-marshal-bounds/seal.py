#!/usr/bin/env python3
import hashlib
import json
import struct
from pathlib import Path

BASE=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):
    p=p.resolve()
    return dict(path=str(p),sha256=sha(p),size=p.stat().st_size)
def save(n,d): (OUT/n).write_text(json.dumps(d,indent=2)+'\n')
source=BASE/'.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUObject.cpp'
candidate=OUT/'candidate/LuaUObject.cpp'
assert sha(source)=='50192ef184ed2cd5267a4eb21c2466de29466419726fd1afac981258070f18e3'
assert sha(candidate)=='48aa5605354a649462f090615546d043dc268c0b06ec0a7dd226964e5a1a0638'
old=source.read_text(); new=candidate.read_text()
start=new.index('        // Reflected offsets must fit both')
end=new.index('        uint8_t num_ufunc_params = func->GetNumParms();',start)
assert new[:start]+new[end:]==old
guard=new[start:end]
assert guard.index('GetParmsSize') < guard.index('TFieldRange')
assert new.index('        constexpr auto parameter_capacity') < new.index('        DynamicUnrealFunctionData dynamic_unreal_function_data{}')
assert 'GetSize()' not in guard.replace('// Division avoids signed GetSize() multiplication overflow and','')
for call in ['InitializeValue(', 'DestroyValue(', 'SetString(', 'GetMinAlignment(']: assert call not in guard

final=BASE/'outputs/2026-10-08-r5-thread-final-integration-v2'
selection=final/'SOURCE-SELECTION.json'
composition=final/'pass1/COMPOSITION-RECEIPT.json'
dependencies=final/'pass1/ACTUAL-DEPENDENCIES.json'
assert sha(selection)=='5fb5f8d55ed8b7f42ce4734ca6d4850ca3f68b658c363ec984f16f5d3b2f1b43'
assert sha(composition)=='82bad5f6016cf8a1407a1385d231b852683f553bfba9de0c37fd0127da6b720c'
assert sha(dependencies)=='e0d4403e2890768d9cd5a73b05c291b49ee0003f5064e7262eae631bcb5bd760'
dep=json.loads(dependencies.read_text())
headers=[]
windows=[]
for suffix,a,z in [('/LuaType/LuaUFunction.hpp',14,19),
                   ('/MemberVariableLayout_HeaderWrapper_UFunction.hpp',15,22),
                   ('/MemberVariableLayout_HeaderWrapper_FProperty.hpp',1,30),
                   ('/MemberVariableLayout_SrcWrapper_UFunction.hpp',80,98),
                   ('/UnrealType.hpp',137,144)]:
    matches=[p for p in dep['files'] if p['path'].endswith(suffix)]
    assert len(matches)==1
    p=Path(matches[0]['path']); assert sha(p)==matches[0]['sha256']
    headers.append(pin(p))
    lines=p.read_text().splitlines(keepends=True)
    body=''.join(lines[a-1:z])
    windows.append(dict(path=str(p),start=a,end=z,source_sha256=sha(p),window_sha256=hashlib.sha256(body.encode()).hexdigest(),text=body))

profile=BASE/'outputs/2026-10-07-l19-local-profile-r4/profiles/townfall-build-facts.json'
facts=BASE/'outputs/2026-10-07-l19-static-layout-check/REFLECTION-FACTS.json'
pr=json.loads(profile.read_text())
expected={'UFunction::ParmsSize':(0xb6,2),'FProperty::ArrayDim':(0x2c,4),
          'FProperty::ElementSize':(0x30,4),'FProperty::Offset_Internal':(0x44,4)}
used=[]
for r in pr['required_members']:
    if r['name'] in expected:
        assert (r['offset'],r['width'])==expected[r['name']]
        used.append(r)
assert len(used)==4
save('SIZE-CONTRACTS.json',dict(classification='existing selected getter and accepted image-bound profile contract; not new native observations',
      profile=pin(profile),native_reflection_facts=pin(facts),required_fields=used,
      exact_source_windows=windows,
      GetSize_signed_product_avoided=True,
      new_native_facts=False))

def read(name): return json.loads((OUT/'tests'/name).read_text())
red=read('RED.json'); faults=read('RED-ASAN.json'); green=read('GREEN.json'); sanitized=read('GREEN-ASAN.json')
assert len(red)==10 and red[0]['returncode']==0 and all(x['returncode']==1 for x in red[1:])
assert len(faults)==2 and all(x['returncode']!=0 and 'AddressSanitizer' in x['stderr'] for x in faults)
assert len(green)==len(sanitized)==18 and all(x['returncode']==0 for x in green+sanitized)
negative=[x for x in green if 'expected_refusal=1' in x['stdout']]
positive=[x for x in green if 'expected_refusal=0' in x['stdout']]
assert len(negative)==12 and len(positive)==6
assert all('refused=1 pushes=0 engine=0' in x['stdout'] for x in negative)
target=OUT/'target/configured/RECEIPT.json'
t=json.loads(target.read_text()); assert t['returncode']==0 and t['source_sha256']==sha(candidate)
obj=Path(t['object']['path']); assert sha(obj)==t['object']['sha256']
header=obj.read_bytes()[:20]
assert header[:6]==b'\x7fELF\x02\x01'
elf_type,elf_machine=struct.unpack_from('<HH',header,16)
assert (elf_type,elf_machine)==(1,62)
command=json.loads((OUT/'target/configured/COMMAND.json').read_text())
assert [x['flag'] for x in command['delta']]==['-c','-o','-MF']
assert t['headers_or_flags_changed'] is False

save('SOURCE-SELECTION.json',dict(status='LOCAL_CANDIDATE_ONLY_NOT_SELECTED',baseline_selection=pin(selection),baseline_composition=pin(composition),
    source_role=dict(name='lua-luauobject',original=pin(source),candidate=pin(candidate)),
    public_or_config_headers_changed=False,emitted_dependency_authority=pin(dependencies),headers=headers,
    changes='one bounds-validation insertion before the existing marshalling body',
    profile_changed=False,mod_changed=False,selected_or_frozen_sources_changed=False,automatic_integration=False))
summary=dict(status='PASS_local_fail_first_bounds_candidate',
    old_normal_passes=1,old_invalid_metadata_acceptance_failures=9,old_actual_body_asan_faults=2,
    positive_cases_per_mode=6,refusal_cases_per_mode=12,plain_controls=18,fatal_sanitizer_controls=18,
    before_pusher_and_ProcessEvent=True,target_whole_TU_compile=True,ELF_type=elf_type,ELF_machine=elf_machine,
    tests_are='actual extracted pre-call statements and actual selected frame declaration; Lua/reflection/ProcessEvent host seams',
    no_native_schema_or_success_claim=True,no_ownership_alignment_or_lifecycle_change=True)
save('TEST-SUMMARY.json',summary)

report=f'''# Unpromoted Lua UFunction parameter-bounds candidate

The candidate refuses oversized or internally inconsistent reflected parameter frames before any argument pusher or ProcessEvent. It preserves the existing 512-byte frame and every original statement after the inserted preflight. This is a separate local safety change; it is not required to repackage the current candidate and does not claim to fix the first menu.

Only [LuaUObject.cpp]({candidate}:199) is changed. Source SHA256 `{sha(candidate)}`. [BOUNDS.patch](BOUNDS.patch) is the complete delta. The original selected body remains SHA256 `{sha(source)}`. No headers, profiles, mods, FText/GC ownership, lifecycle policy, frozen objects or package inputs were changed.

The existing GetParmsSize contract is uint16 and is pinned at UFunction+0xB6/2 bytes. FProperty Offset_Internal, ElementSize and ArrayDim are existing signed int32 getters at +0x44/+0x30/+0x2C. [SIZE-CONTRACTS.json](SIZE-CONTRACTS.json) pins their actual selected declarations/getter and accepted image-bound facts. Native measurements are inherited, not repeated. GetSize multiplies two int32 values, so the guard avoids its possibly overflowing product: it checks positive dimensions, nonnegative offset and count <= (ParmsSize-offset)/ElementSize.

Preflight first checks ParmsSize <= sizeof the actual DynamicUnrealFunctionData::data. It then validates all CPF_Parm properties, including return/out slots, before the first original writer. Non-parameter fields are ignored. A bad later parameter therefore cannot permit an earlier writer to run. Missing getter/map contracts retain the existing getter exception behavior; no fallback size is introduced.

| Control | Original | Candidate |
|---|---|---|
| Normal small frame | passes | passes |
| 9 invalid declared-size/dimension/late-slot cases | accepted incorrectly by the extracted original pre-call body | refuse before writers/engine |
| Negative/straddling buffer offset | two real ASan faults in the extracted original body | refuse before writers/engine |
| Small, exact 512-byte boundary, empty, ignored non-param, array extent, valid return slot | original normal case retained | 6 positive cases in each mode pass |
| 12 invalid size/offset/dimension/product/late-slot/return cases | original lacks this preflight | 12 cases in each mode refuse with pushes=0 and engine=0 |

Fail-first outputs are [RED.json](tests/RED.json) and [RED-ASAN.json](tests/RED-ASAN.json). The candidate passes 18 plain and 18 fatal ASan/UBSan controls: [GREEN.json](tests/GREEN.json), [GREEN-ASAN.json](tests/GREEN-ASAN.json). The test compiles actual source statements from the opening implementation brace through its first ProcessEvent and the exact selected frame declaration. The post-call result/out-parameter suffix is excluded by a disclosed return seam; Lua/reflection objects and ProcessEvent are host test seams. This does not model a successful native menu or re-review cleanup behavior.

The whole candidate translation unit compiled with the final selected SDK recipe and unchanged include/flag configuration: [target receipt](target/configured/RECEIPT.json). Only -c/-o/-MF paths differ. LLVM_CONFIG selects the existing retained LLVM21 delegate, as in the existing target controls. The first local compiler invocation without that environment failed before compilation and is preserved in target/RECEIPT.json; the configured target output is ELF64 x86-64 ET_REL. No link, package, native execution or automatic integration was performed.

This bounds refusal depends on trustworthy reflected metadata already required by the runtime. It does not prove that any original menu function exceeds 512, validate alignment, initialize/destroy nontrivial values, or bound a buggy pusher that writes more than its property's declared extent. Those remain separate issues; no speculative remedy is included.

Focused reproduction (use a new copy of this directory to preserve sealed logs):

```sh
python3 tests/run.py red
python3 prepare.py
python3 tests/run.py green
python3 target_compile.py
python3 seal.py
```

All writes from these scripts stay within their own report directory. Review/admission would be required before selecting this one-file candidate; the current linked/packaged runtime remains unchanged.
'''
(OUT/'REPORT.md').write_text(report)
files=[p for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='FINAL-RECEIPT.json']
save('FINAL-RECEIPT.json',dict(status=summary['status'],summary=summary,producer_files=[pin(p) for p in files],
    source_selection=pin(OUT/'SOURCE-SELECTION.json'),candidate_source=pin(candidate),target_receipt=pin(target),
    console_contact=False,native_execution=False,automatic_integration=False,first_menu_fixed_claim=False))
print(json.dumps(dict(status=summary['status'],source_sha256=sha(candidate),receipt_sha256=sha(OUT/'FINAL-RECEIPT.json'),
                     plain=18,sanitized=18,target_whole_TU=True)))
