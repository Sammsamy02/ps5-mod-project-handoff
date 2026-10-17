# Unpromoted Lua UFunction parameter-bounds candidate

The candidate refuses oversized or internally inconsistent reflected parameter frames before any argument pusher or ProcessEvent. It preserves the existing 512-byte frame and every original statement after the inserted preflight. This is a separate local safety change; it is not required to repackage the current candidate and does not claim to fix the first menu.

Only [LuaUObject.cpp](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-menu-marshal-bounds/candidate/LuaUObject.cpp:199) is changed. Source SHA256 `48aa5605354a649462f090615546d043dc268c0b06ec0a7dd226964e5a1a0638`. [BOUNDS.patch](BOUNDS.patch) is the complete delta. The original selected body remains SHA256 `50192ef184ed2cd5267a4eb21c2466de29466419726fd1afac981258070f18e3`. No headers, profiles, mods, FText/GC ownership, lifecycle policy, frozen objects or package inputs were changed.

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
