# l19 bounded static layout check

LOCAL ONLY. Owned saved image SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, 194241424B. Existing capstone/lief tools were available. No executable was run, and no console/network/account action, upload, runtime test, source edit or broad memory/kernel probe occurred. All newly written artifacts are in this directory; older captures/reports remain intact.

`STATIC-FACTS.json` records exact instruction addresses, file-byte offsets, bytes, inference chains and per-field confidence. `make_report.py` checks every cited instruction window against the authenticated image and independently parses ELF program headers. All primary code records are ELF image vaddrs; leading addresses in the retained rdis dumps add the historical 0x400000 base and are not new runtime evidence. Code PT_LOAD0 maps each cited instruction to file offset=imagevaddr+0x4000.

## Fundamental layout facts recovered

| Field | Exact static value | Access width | Main instruction evidence |
|---|---:|---:|---|
| FUObjectItem item stride / UEP_TotalSize | 0x18 | — | allocate483b9f/483ba6, free4840d2/4840d9, payload0x180000 for65536items |
| FUObjectItem.Object | 0x0 | 8 |483d40 `49891c24`;4840e1/4840e5 read/compare originalobject |
| FUObjectItem.Flags | 0x8 | 4 |483d14 `41c744240800000080`;483d84 atomicdword update |
| UObjectBase.ObjectFlags | 0x8 | 4 |48545b `895708`;45985c independent ProcessEvent guard |
| UObjectBase.InternalIndex_Private | 0xC | 4 |48545e initialization;483d5e store;4840bb read and itemroundtrip |
| UObjectBase.ClassPrivate | 0x10 | 8 |485465 `48897710` stores requestedclass argument |
| UObjectBase.NamePrivate | 0x18 | 8 |4854af nameargument;4855b4 authenticated FName ctor,4855b9/4855bd outputstore |
| UObjectBase.OuterPrivate | 0x20 | 8 |485492 `4c896320` stores outerargument;45990c/326ace ownerreads |

The semantic object-base identification comes from StaticConstructObject490aa0 → allocation491760 → constructor485430 → GUObjectArray allocator483b40, and the independent registration485530 path. The allocator writes the same object index that its free routine reads to recover and compare the original UObject. These are target operands, not adopted setter templates.

Item24B is independently established by two index calculations and chunk allocation: `(index & 0xffff) *3 *8`, chunk`index>>16`, payload65536*24=0x180000, plus a separate8B arrayheader. Free zeroes16B then8B of the item. Object0 is a directqword pointer and Flags8 is a dword flag word in these routines; the buildmacro `UE_ENABLE_FUOBJECT_ITEM_PACKING` remains unclassified by name.

UObjectBase exact `UEP_TotalSize` remains unresolved. Established members prove a0x28B minimum envelope, but the bounded paths did not independently identify the exact complete size/tail. UFunction exact total similarly remains unresolved, with0xE0 minimum envelope fromnativeFunc+D8 width8. Candidate template totals0x28/0xE0 remain candidates; do not silently publish lowerbounds as sizeof.

## UFunction C8/D8 correction and selected width

The prior worker-gap-l2a report labeled the pointer atC8 as nativeFunc. Full dataflow disproves that label. ProcessEvent4598cc (`4c8bbec8000000`) loadsR15 fromfunction+C8;4598e5 (`41f687b100000080`) then reads that pointer's functionflags, and45990c (`498b7f20`) reads its owning object. A nullC8 chooses the original function instead. C8 is the event-graph UFunction target; it is not a nativecode callback. Originalfunction+D0 supplies the corresponding calloffset.

ProcessEvent459b6f calls image326ac0. This Invoke function saves selfRDI inR15 at326adb (`4989ff`), prepares Context/Frame/result inSysV RDI/RSI/RDX, then executes326b25 (`41ff97d8000000`): `call qword ptr [r15+0xd8]`. This directly establishes **native Func atD8, one8B slot**. It removes the apparent targetC8/WindowsD8 conflict because theC8 claim had mistaken a different field.

The selected source agrees with8B: Class.hpp356 `GetFunc()` returns `std::function<...>*&` throughGetFunc419; generated wrapper195 returns a pointer reference; UFunction.cpp28-35 GetFuncPtr/SetFuncPtr useGetFunc andbit_cast the pointer value. Historical compiled widthauthority16 is the maximum of anotherGetFuncBase alternative16 andGetFunc419 alternative8. It is a conservativevalidator fact, not proof that the selected read/write path touches16B. Preserve the historical fixture and provide selected-accessor evidence if widthpublication is narrowed; this check edits no authority.

ProcessEvent also directly establishes parameterwordB6 (copybounds) and returnwordB8 (parameterbase+offset,ffff meansnull). FunctionFlags is only directly observed as byteB1 here. FullwordbaseB0/width4 and NumParmsB4 remain unpromoted by this bounded pass.

## Additional grounded diagnostic

ProcessEvent45985f tests0x40000000 inObjectFlags+8. Retained `RF_AllFlags=0x0FFFFFFF` is not demonstrated as a complete targetvalidmask and would reject an object with that tested highbit set. This pass records the test and leaves the complete targetmask unresolved; it does not invent a new mask or change predicates.

## Evidence integrity and limits

The JSON contains instruction-window bytechecks and complete sourcehashes. Existing Ghidra dump `/tmp/wrfp_gap_l2a/vt2.txt` corroborates the ProcessEvent bytes/dataflow, but its old prose label is superseded only by this report, not overwritten. Existing recursive disassembler's finalcallfooter double-addsbase tocallsite addresses; use individual instructionrecords. Exploratory `UObjectBaseRegister.asm` started inside48558f and is retained as rejecteddata; confirmed functionstart485530 is saved separately and is the cited source. No live heap/layout/execution proof is added.
