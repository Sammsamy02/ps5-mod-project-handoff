# worker-expected-bytes — REPORT (2026-10-04)

## 1. Retained capture located
The static survey's input binary is not inside `run-release-1/` itself (that dir holds only
`receipt.json`/stdout/stderr); the receipt's argv points at the retained capture:

- `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
- 194,241,424 bytes (0xb93e390), sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
- ELF64 LSB x86-64, type 65040 (0xFE10, Orbis/PS4 eboot), OS ABI FreeBSD

## 2. ELF program headers (verified)
`e_phoff=0x40, e_phentsize=56, e_phnum=14`. ELF magic `7f 45 4c 46` verified at offset 0.
Five PT_LOAD segments; the code segment is LOAD0: `off=0x4000 vaddr=0x0 filesz=0x7479b1c flags=R+E align=0x4000`.
RW segments: 0x7ebc000 (filesz 0x127e4e8), 0x913c000 (filesz 0x1cc5f2, memsz 0x4f7418 → BSS tail
0x93085f2..0x9633418), 0x9633420 (rodata, 0x262cea8).

Offset formula (LOAD0): `file_offset = p_offset + (vaddr − p_vaddr) = vaddr + 0x4000`.

Boundary check: each anchor vaddr ∈ [0x0, 0x7479b1c) and computed file_offset + 32 ≤ 0x747db1c < file size. PASS.

## 3. Expected anchor bytes (first 32 bytes each)

| anchor | vaddr | file_offset | bytes (hex, spaced) |
|---|---|---|---|
| UGameEngine::Tick | 0x2fac8c0 | 0x2fb08c0 | 55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec 88 00 00 00 4c 8b 2d 2d 56 18 06 c5 f0 57 c9 c5 |
| StaticConstructObject | 0x490aa0 | 0x494aa0 | 55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec 58 02 00 00 48 8b 05 4d 14 ca 08 48 89 fb 48 8b |
| FName ctor call-site | 0x23d590 | 0x241590 | 55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74 33 0f b7 38 48 89 c6 31 c9 66 85 ff 74 2a 66 90 |

## 4. Prologue verification (capstone x86-64 disassembly)
- **Tick**: `push rbp; mov rbp,rsp; push r15/r14/r13/r12/rbx; sub rsp,0x88; mov r13,[rip+0x618562d]; vxorps xmm1,xmm1,xmm1` — canonical frame-setup entry. VALID.
- **StaticConstructObject**: same shape, `sub rsp,0x258`, then `mov rax,[rip+0x8ca144d]; mov rbx,rdi`. VALID.
- **FName ctor candidate**: `push rbp; mov rbp,rsp; push rbx; push rax; mov rax,rsi; mov rbx,rdi; test rsi,rsi; je +0x33; movzx edi,word[rax]; …; je +0x2a; nop` — function entry with null/EName early-outs consistent with an FName constructor. VALID; the call-site candidate resolves to a real function start.

All three match the survey's `status=0` link results (tick 0x2fac8c0, sco 0x490aa0, fname_ctor_call 0x23d590 in run-release-1/stdout.txt).

## 5. FUObjectArray (GUA) template for live-read comparison
Sources: port per-version generated layouts at
`.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/FunctionBodies/*_MemberVariableLayout_DefaultSetter_{FUObjectArray,TUObjectArray,FUObjectItem}.cpp`
(via `UObjectArray.hpp` → `UObject425.hpp`-era versioned containers).

First 0x30 bytes are identical across the 4.25 and 5.06 templates:
- 0x00 ObjFirstGCIndex i32 | 0x04 ObjLastNonGCIndex i32 | 0x08 MaxObjectsNotConsideredByGC i32 | 0x0C OpenForDisregardForGC bool(+3 pad)
- 0x10 ObjObjects (TUObjectArray, size 0x20): 0x10 `Objects` FUObjectItem** (chunk-table ptr, non-null RW), 0x18 `PreAllocatedObjects` FUObjectItem* (usually NULL), 0x20 MaxElements i32, 0x24 NumElements i32 (live count, ≤ MaxElements), 0x28 MaxChunks i32, 0x2C NumChunks i32 (≥1 at runtime)

Then they diverge: 4.25 template puts UObjectCreateListeners@0xE0, UObjectDeleteListeners@0xF0, MasterSerialNumber@0x128, total 0x130; 5.06 template (the binary self-reports engine 5.6) puts ObjAvailableList@0x58, listeners@0x68/0x78, PrimarySerialNumber@0xB0, bShouldRecycleObjectIndices@0xB4, total 0xB8.
FUObjectItem slot: Object@0x0, Flags@0x8, ClusterRootIndex@0xC, SerialNumber@0x10 (+RefCount@0x14 in 5.06), size 0x18.

Caveats (honest):
- Survey-linked GUObjectArray vaddr **0x93ba110 is BSS** (past LOAD3 p_filesz end 0x93085f2, within memsz) → its expected bytes are runtime-only; compare live reads against the template, not file bytes.
- Port models leave 0x30..0xE0 (4.25) / 0x30..0x58 (5.06) unnamed beyond the 0x20-byte TUObjectArray; stock UE would place ObjAvailable TArray<int32> at 0x30 (4.25). Treat port offsets as authoritative for the accessor layer; flag disagreement rather than forcing an interpretation.

## 6. Outputs
- `EXPECTED-BYTES.json` — anchors {vaddr, file_offset, bytes_hex, disassembly_summary, prologue_valid}, ELF verification, load-segment table, GUA templates + validity criteria. JSON-validated.
- `bytes_tick.bin`, `bytes_sco.bin`, `bytes_fname.bin` — raw 32-byte evidence.
- Raw capture untouched (read-only); disassembler: capstone 5.0.x (llvm-mc absent from Xcode toolchain).

No source/engine files modified; all writes confined to this worker directory.
