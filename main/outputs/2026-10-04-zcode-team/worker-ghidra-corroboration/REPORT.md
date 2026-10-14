# REPORT — worker-ghidra-corroboration (RESTART, completed)

Date: 2026-10-05 · Target: townfall decrypted-eboot.bin (sha256 3b6cfad4…22fc, re-verified)
Workspace: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-ghidra-corroboration/`

## What was found on restart

The lost prior attempt had already finished the expensive part: the Ghidra 12.1.4 headless project at `/tmp/wrfp_ghidra_proj/fnamepool` contains the fully imported+analyzed eboot (330 MB db, committed at 10:48), the postScript `FnamepoolDecomp.java` ran to completion (`decomp/SUMMARY.txt`: imageBase=0, mappingDelta=0x0, all 10 `CORROB_*` functions created and decompiled, none truncated), and the LOAD0 phdr audit artifacts existed (`phdr_load0_check.json`, `tools_phdr_check.py`). Missing were the verdict documents. No re-import or re-run of Ghidra was needed; this session performed the independent cross-checks and wrote the deliverables.

## Verdicts (full detail + excerpts in CORROBORATION.md, machine-readable CLAIM.json)

1. **FName ctor 0x23d590 → FindOrStore 0x23d600 → core 0x242470 — CONFIRMED.** Byte-exact against the RE quotes; decompilation matches all described semantics (OR-reduction + wide flag bit32, bextr wchar length, 0x400-cap SSE narrowing, `_NNN` suffix parse, lazy-init flag byte at pool−0x20 = 0x9365ea0, bucket `pool + hash*0x40 + 0x10040`).
2. **FNamePool::Init 0x23a5d0 — CONFIRMED.** `memset(pool+8,0,0x10008)`; `Malloc(0x20000,2)` → `Blocks[0]` at pool+0x10; 256 buckets at +0x10040 stride 0x40 with per-bucket mutex; `memset(pool+0x14040,0,0xafc)`; None→+0x14040, ByteProperty→+0x14044 (raw strings re-read from the binary). One minor correction: **+0x1404C is BoolProperty; FloatProperty is +0x14050** (RE said FloatProperty@+0x1404C). Validator offsets unaffected.
3. **CurrentBlock getter 0x23d570 — CONFIRMED byte-for-byte:** `mov eax,[0x9365ec8]; shr edi,0x10; inc eax; cmp eax,edi; seta al` ≡ `id>>16 <= CurrentBlock`. Reserve 0x246090 decomp confirms CurrentBlock is a 0-based open-block index (0 valid).

## LOAD0 +0x4000 trap audit

Recomputed with struct-only phdr parsing (`tools_phdr_check.py`, receipt saved): PT_LOADs at phdr idx 0,1,3,7 carry `file_off = vaddr + 0x4000`; idx 8 carries `−0x324000` — exactly the RE doc's map. All 34 pinned RE addresses classified (`phdr_load0_check.json`); every pinned address lives in a +0x4000 load, and the pool/flag/GMalloc/allocator-table addresses are memsz-only BSS (no file bytes). **Ghidra exposure: none** — the postScript's signature check proved Ghidra addressed the program in vaddr space (mappingDelta 0x0), so its outputs are directly comparable to the RE file vaddrs.

## Label artifacts found in RE outputs (semantics unaffected, do not copy these labels)

- RE md §2.1 getter quote block is **+0x3C00 too high**: the quoted bytes live at 0x23f661–0x23f697 inside fn **0x23f640** (decompiled; rip-targets hit exactly 0x9365ec0). The claimed 0x243240 contains unrelated code.
- RE CLAIM.json sample "site 0x23d0a2" is **+0x4000-shifted**: bytes are at **0x2390a2** (lea → 0x9365ec0 confirmed genuine).
- Several PLT labels are runtime-form in a file-vaddr doc (`0x4b7a20`=file 0xb7a20 etc.); displacements all recomputed and consistent.
- Cosmetic bugs in `tools_phdr_check.py` noted (runtime-form PINNED entry; getter byte-check compares wrong offset — receipt's one "MISMATCH" actually shows the correct prologue).

## Verification method

Three independent legs: (1) Ghidra 12.1.4 decompilation/disassembly via the postScript (prior-attempt output, verified complete); (2) this worker's raw-byte re-verification of every RE-quoted instruction sequence, string constant and call-target displacement using struct-parsed phdrs — no capstone/lief reuse (`byte_checks.receipt.txt`, 40+ OK lines); (3) struct-only phdr recompute for the trap audit. All three agree.

## Still unresolved (unchanged)

The launch-9 runtime observation (Blocks[0] inside module-BSS page range) vs the now-Ghidra-corroborated static GMalloc/binned chain; deep binned-allocator internals corroborated only at entry points; wrapper installation and slow-path syscall source statically undecidable. A fresh runtime read of the qword at runtime 0x9765ed0 remains the deciding test.

## Deliverables (all in the worker directory)

- `CORROBORATION.md` — full per-claim verdicts with decompiled excerpts
- `CLAIM.json` — machine-readable verdicts, corrections, trap audit
- `FnamepoolDecomp.java` — the postScript (prior attempt, verified complete)
- `decomp/` — Ghidra asm + C for 10 functions, SUMMARY.txt, memory_blocks.txt
- `byte_checks.receipt.txt` — independent byte-level verification log
- `tools_phdr_check.py` + `phdr_check.receipt.txt` + `phdr_load0_check.json` — trap audit
