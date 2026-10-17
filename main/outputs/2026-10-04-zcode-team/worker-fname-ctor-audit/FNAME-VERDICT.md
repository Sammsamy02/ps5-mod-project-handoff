# FNAME-VERDICT.md — worker-fname-ctor-audit (2026-10-04)

Question: **can `FindFirstOf("ItemDatabaseSubsystem")` work after composition-6?**

## Verdict

**NO as composed today — YES it depends on exactly one missing fact (X): a bound,
validated engine FName constructor.** Two honest remedies exist; both use only
data-side in-game validation because code reads are exec-blocked on this target.

## 1. What the port does today (resolution chain, PROSPERO)

1. `FindFirstOf(const CharType*)` constructs an FName from the string
   (`port/unreal/src/UObjectGlobals.cpp:481-484`) and matches via FName index
   equality only (`port/unreal/src/UObjectGlobals.cpp:454-478`,
   `NameTypes.hpp:334-337`).
2. The string ctor delegates to `construct_with_string`
   (`port/unreal/include/Unreal/NameTypes.hpp:246-267`), which **silently
   returns** when `!ConstructorInternal.is_ready() && !FunctionAddressOverride`
   (:248) — leaving `ComparisonIndex{}` = 0 = NAME_None (:218). Search yields
   nullptr; the negative is ambiguous (same hazard independently recorded in
   `worker-rung3-reflection-prep/REPORT.md:41-47`).
3. `ConstructorInternal` is assigned in the second init pass from
   `results.fname_ctor_wchar` (`port/unreal/src/UnrealInitializer.cpp:842-845`).
   A null result is assigned anyway; `assign_address(nullptr)` leaves the
   Function not-ready (`port/first/include/Function/Function.hpp:57-63`,
   `m_is_ready = func_address`).
4. `results.fname_ctor_wchar` has exactly two possible sources:
   - **Lua AOB script** `UE4SS_Signatures/FName_Constructor.lua`
     (`port/ue4ss/src/Signatures.cpp:222-285`) — installed only by
     `setup_lua_scan_overrides` when the script file exists
     (`port/ue4ss/src/UE4SSProgram.cpp:1034`). No such directory is staged on
     console; the script's own verification path also requires
     `VerifyFNameConstructor` (detour hook, `UnrealInitializer.cpp:878-941`)
     and a code patch — exec-blocked (see 5).
   - **Native ps_scan backend** (patternsleuth) — explicitly *optional* and not
     wired: `port/ue4ss/include/Platform/NativeProgramContext.hpp:64-66`
     ("Optional concrete scanner owner for the selected native ps_scan
     backend"), `UE4SSProgram.cpp:964` passthrough only.
   No in-tree signature container constructs the ctor otherwise
   (`port/unreal/src/Signatures.cpp:107-111` — container only from
   ScanOverrides).

## 2. Scan target: code or data?

Non-Windows scans run over `config.ModuleSource->ReadableRanges()`
(`port/unreal/src/Signatures.cpp:83` and `:148`) — the mapped in-game image.
In-game, the eboot maps as:

| span | prot | meaning (PS5: PROT_READ=1, PROT_WRITE=2, PROT_EXEC=4) |
|---|---|---|
| 0x400000–0x787c000 | 0x4 | **execute-only** (text) |
| 0x787c000–0x82bc000 | 0x1 | read-only |
| 0x82bc000–0x953c000 | 0x1 | read-only (RELRO-covered LOAD3) |
| 0x953c000–0x9a34000 | 0x3 | read-write |

Sources: igf6 `g5_vmmap` (`worker-parser-validation/run2/igf6-results.parsed.jsonl`),
igf3 `v1_vmmap_full` prot histogram + `v1_vmmap_raw` headers; cross-checked
against the capture's own program headers (LOAD0 flags=1 exec-only
[0x0,0x7479b1c), LOAD3 RW + GNU_RELRO identical span, LOAD7 RW
[0x913c000,0x9633418 file / 0x4f7418 mem]) under the observed uniform 0x400000
base (`worker-fname-fc1-materialize/REPORT.md:66-69`).

**Therefore every code-pattern scan — patternsleuth `FNameCtorWchar`, Lua AOB,
and detour-based hook verification — reads exec-only pages in-game and joins
the exec-only dead list.** This was already visible statically: the frozen
survey returned `fname_ctor_original status=3` (absent) and
`fname_to_string status=3` (`worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json:198-250`),
and code-anchor reads from a ploader module are blocked
(`worker-checkpoint-reconcile/CURRENT-STATE.md:9`).

**Failure semantics: silent.** Missing Lua script → no override → no container →
result 0 → `assign_address(0)` → not-ready → FName no-op → FindFirstOf quietly
returns nullptr. Nothing loud anywhere in the chain.

## 3. In-game evidence for the pool: none

No retained igf/anchor/checkpoint record pins the FName pool. The in-game
records cover vmmap, module list, layout predicates and GUA array-header words
only; no class or name string has ever been read in-game
(`worker-rung3-reflection-prep/REPORT.md:70-77`,
`worker-anchor-validation/REPORT.md:75`). GUA facts (layout A, 0x97ba110,
71,959 objects, 2 chunks — `worker-checkpoint-reconcile/CURRENT-STATE.md:7`)
are pool-independent.

## 4. Offline derivation from the retained image (the honest method)

Capture: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
sha256 `3b6cfad4…3622fc` re-verified this session (== profile E-capture).
ELF64 x86-64; the section-table header offset (0x6f48a0a8) lies beyond EOF, so
**no symbols** — program headers + operand analysis only.

Chain (all vaddrs file-image; runtime = +0x400000):

1. **Ctor candidate 0x23d590** (`55 48 89 e5 53 50 48 89 f0 48 89 fb…`, exact
   bytes already frozen by `worker-fname-fc1-materialize/REPORT.md:46-47`):
   `rdi`=sret out, `rsi`=wchar_t* name, `edx`=EFindName; length walk, wide
   flag, `call 0x23d600`, then `mov [rbx], rax` — a complete 8-byte FName
   store. Matches the SysV explicit-output ABI
   (`NameTypes.hpp:228-235`) and the 9 call sites' window shape
   (`worker-fname-fc1-materialize/REPORT.md:37-53`).
2. 0x23d590 → 0x23d600 (suffix split + wide→narrow) → 0x23e0d0 →
   **0x242470** (the interning core; ANSI variants 0x23d8a0/0x23d9b0 also call
   it at 0x23d94f).
3. **0x242470 references the FNamePool singleton by direct LEA at
   `0x9365ec0`** (runtime **0x9765ec0**):
   - init: `lea rax,[rip→0x9365ec0]` + `call 0x23a5d0` guarded by the init
     flag byte at 0x9365ea0 (0x2424c7–0x2424d7);
   - FNAME_Find: `lea rax,[0x9365ec0]` + `call 0x23c0b0` (0x2424ee–0x2424fe);
   - add path: `lea rcx,[0x9365ec0]`; `shl rax,6`; `lea rdi,[rax+rcx+0x10040]`
     + `call 0x23c340` — hash probe into pool+0x10040 with 64-byte buckets
     (0x24252e–0x242550).
4. **Layout corroboration** from the callees (all take the pool in `rdi`/`rbx`):
   - init 0x23a5d0: `mov [rbx+0x10], rax` (Blocks[0] = first block),
     `lea r14,[rbx+0x10040]`, `lea r15,[rbx+0x14040]`; then intern("None",
     0x7b96c17), intern("ByteProperty", 0x7abcd83), intern("IntProperty",
     0x7bfb4f8) and stores the ids at pool+0x14040/0x14044.
     This is UE5 FNamePool::Init and matches the port's own comment
     (`port/unreal/src/Signatures.cpp:243-248`).
   - find 0x23c0b0 reads `[pool+0x10]` (Blocks) and `[pool+0x18]` (Blocks[1]);
     insert 0x23c340 reads `[pool+0xc]` (CurrentByteOffset) and
     `[pool+0x10]` (Blocks[0]).
   - ⇒ `FNameEntryAllocator`: Lock@+0x0, CurrentBlock@+0x8,
     CurrentByteOffset@+0xC, Blocks[8192]@+0x10 — consistent with the port's
     constants (`NameTypes.hpp:154-159`).
5. Pool candidate sits in LOAD7's **bss** (file part of LOAD7 ends at
   0x93085f2) → zero in file, populated at startup, and lands in the in-game
   RW span 0x953c000–0x9a34000 (prot 3) → **data-side readable in-game**.

Honest labels: every address above is an **operand-derived file-image label**
(same standard as GameEngineTick `0x2fac8c0` in the profile); nothing has been
read or called on target. The 161,503-ref global at 0x9131f08 that the FName
functions also load shows a read-compare-guard-then-abort pattern
(0x24247f/0x242489/0x242557–0x242576) — likely a TLS-generation guard; left
unresolved, not needed for this chain.

## 5. Remedy design (evidence standard = slots work)

**Option 1 — bind the engine ctor (preferred; unlocks the whole FName path):**
- Composition carries three file-image labels: `ctor=0x23d590`,
  `pool=0x9365ec0`, `base_observed=0x400000` (base re-derived per launch from
  the GUA anchor: validated runtime GUA 0x97ba110 − label 0x93ba110).
- In-game **data-side validation of the pool first** (no code reads):
  1. `CurrentBlock`(u32 @ runtime pool+0x8) ≥ 1;
  2. `Blocks[0]`(@ pool+0x10) nonzero and inside the readable heap span
     (0x8018c000–0x100118000 per igf6 vmmap);
  3. entry id at pool+0x14040 resolves via Blocks[] to the ASCII string
     "None" (and pool+0x14044 → "ByteProperty") — header u16 then chars.
  A guessed address fails one of these; the in-game anchor proves the base,
  the readable data validates the pointers.
- Bind `FName::ConstructorInternal.assign_address(base+0x23d590)` (the port's
  SysV `void(FName*, const CharType*, EFindName)` signature already matches
  the engine lowering; `NameTypes.hpp:228-235`, corroborated by
  `worker-fname-construction/REPORT.md:24-27`).
- **Behavioral validation of the call** (the call itself is the only exec-side
  step, and it is engine code, not ours): one guarded
  `FName("ByteProperty", FNAME_Find, override)` — exactly the port's existing
  probe (`port/ue4ss/src/Signatures.cpp:248`) — must return a nonzero id that
  **equals the independently pool-read id at +0x14044**. Zero-fill,
  divergence, or missing post-records ⇒ refuted, no replay.
- Then `FindFirstOf("ItemDatabaseSubsystem")` works (given a loaded save where
  the subsystem object exists), because the GUA enumeration and index-Equals
  need nothing else (`UObjectGlobals.cpp:434-478`).
- Guardrail: construct the FName with **FNAME_Find explicitly** — the
  `FindFirstOf(const CharType*)` default is FNAME_Add
  (`NameTypes.hpp:319`), which would mutate the pool on a miss; the port
  itself documents FNAME_Find for lookup-only (:315-318).

**Option 2 — pure data-side FindFirstOf (no engine call at all):**
- Resolve the id of "ItemDatabaseSubsystem" by walking the pool blocks
  (readable RW): CurrentBlock small, ≤64 KiB per block, 2-byte entry stride
  (FNameEntry 2-byte alignment, header u16 + chars).
- Then scan the validated GUA (71,959 objects, 2 chunks) comparing
  UObjectBase `NamePrivate` (offset validated by the fully-widthed UObjectBase
  contract, ClassPrivate 8 / NamePrivate 8 — profile `compiled_port_widths`)
  against that id.
- Costs: no code execution, no hook, no detour; consistent with the exec-only
  constraint. This is the fallback if the guarded ctor call is refused.

**Sequencing note:** FC-1's in-game code byte-shape check (FC1-A) is itself
exec-blocked — the validation weight must rest on the data-side pool reads
plus the behavioral ByteProperty probe. Keep the offline byte-shape checks
(FC1-CHECKS.md) as the *pre-launch* gate on the frozen capture instead.

## 6. What would falsify this verdict

- Pool validation reads fail (CurrentBlock=0 / Blocks[0]=0 / string mismatch)
  ⇒ pool label wrong or base moved; re-derive, never guess.
- ByteProperty probe returns 0 or an id ≠ pool-read id ⇒ ctor label or ABI
  wrong; stop, reconcile, distinct-id retry only.
- `FindFirstOf("ItemDatabaseSubsystem")` still null with ctor verified ⇒ the
  subsystem object is absent (save not loaded) — a game-state negative, not an
  FName failure (worker-spawner-itemdb-audit/SPAWN-READINESS.md:17,79).
