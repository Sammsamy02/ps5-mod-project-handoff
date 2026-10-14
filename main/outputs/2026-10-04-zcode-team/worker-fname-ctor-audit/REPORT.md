# REPORT.md — worker-fname-ctor-audit (2026-10-04)

Task: audit FName construction on PROSPERO for the Item Spawner's hard gate
`FindFirstOf("ItemDatabaseSubsystem")` — (1) trace the port's FName ctor
resolution, (2) check whether any in-game record pins the FName pool and derive
pool candidates offline from the retained eboot, (3) state what the scan
actually needs, (4) verdict + honest remedy design. Proposal-only. No console
contact, no nested subagents, nothing executed against the game image; offline
analysis = read-only parsing + capstone disassembly of the frozen capture.

Deliverables (in order): `CLAIM.json` (written first), `FNAME-VERDICT.md`,
this report. All paths below relative to
`.superpowers/native-dependency-spike/` unless prefixed otherwise.

## 1. Answer in four lines

- The port's FName string ctor **silently no-ops** unless
  `FName::ConstructorInternal` is bound
  (`ue4ss-unreal-native-build-1/port/unreal/include/Unreal/NameTypes.hpp:246-248`);
  FindFirstOf then returns nullptr with no diagnostic.
- On PROSPERO the only binder is a Lua AOB script or an optional ps_scan
  backend that is not wired; both are **code scanners**, and the eboot text
  maps execute-only in-game ([0x400000,0x787c000) prot 0x4, igf6 vmmap), so
  the whole route is exec-dead. Failure semantics: **silent**.
- Nothing in the in-game records pins the pool. Offline, from the frozen
  capture (no symbols — section table beyond EOF), the ctor candidate chain
  0x23d590→0x23d600→0x23e0d0→0x242470 LEAs the FNamePool singleton at
  **file-image 0x9365ec0 / runtime 0x9765ec0**, with full structural
  corroboration (Blocks@+0x10, CurrentBlock@+0x8, CurrentByteOffset@+0xC,
  hash tables @+0x10040/+0x14040, bootstrap "None"/"ByteProperty"/"IntProperty").
- **Verdict: NO as composed today; depends-on-X** where X = one validated ctor
  binding (preferred: carry `base+0x23d590`, validate data-side via the pool,
  behaviorally via the port's own ByteProperty FNAME_Find probe) or the pure
  data-side alternative (pool walk + GUA NamePrivate compare). Both remedies
  never use a guessed address: in-game anchor proves the base, readable pool
  data validates the pointers, and the call is cross-checked against
  independently read pool data.

## 2. Task 1 — resolution chain trace (file:line)

- Ctor entry: `NameTypes.hpp:319-327` (default **FNAME_Add**), delegate
  `construct_with_string` `:246-267`; **silent early return** at `:248` leaves
  `ComparisonIndex{}`=NAME_None (`:218`). PROSPERO SysV sret signature
  `void(FName*, const CharType*, EFindName)` at `:228-235`.
- Binding: `port/unreal/src/UnrealInitializer.cpp:842-845` assigns
  `results.fname_ctor_wchar` unconditionally when the config flag is set;
  `assign_address(nullptr)` → not-ready
  (`port/first/include/Function/Function.hpp:57-63`).
- Result source: `port/unreal/src/Signatures.cpp:107-111` — only via
  `ScanOverrides.fname_constructor`; installed only by
  `setup_lua_scan_overrides` when `UE4SS_Signatures/FName_Constructor.lua`
  exists (`port/ue4ss/src/UE4SSProgram.cpp:1034`;
  `port/ue4ss/src/Signatures.cpp:222-285`, incl. VerifyFNameConstructor at
  :242 and the `FName("ByteProperty", FNAME_Find, address)` probe at :248).
- ps_scan backend optional, absent: `port/ue4ss/include/Platform/
  NativeProgramContext.hpp:64-66`; `UE4SSProgram.cpp:964`.
- Scan surface: `port/unreal/src/Signatures.cpp:83,:148` —
  `ModuleSource->ReadableRanges()` (mapped in-game image).
- Verification path `VerifyFNameConstructor`
  (`UnrealInitializer.cpp:878-941`) is **detour-based** (needs a code patch at
  the ctor) — exec-blocked on this target; not usable as the validator.

## 3. Task 2 — in-game records + offline pool derivation

**In-game:** no pool anchor exists. Records are vmmap/module-list/layout
predicates/GUA header words only
(`worker-rung3-reflection-prep/REPORT.md:70-77`;
`worker-anchor-validation/REPORT.md:75`; igf6/igf3 records under
`worker-parser-validation/run2/`). GUA facts: layout A, runtime 0x97ba110,
71,959 objects, 2 chunks (`worker-checkpoint-reconcile/CURRENT-STATE.md:7`).

**Offline (frozen capture, sha256 3b6cfad4…3622fc re-verified):**
- No symbols: section-table header offset 0x6f48a0a8 > EOF; program headers
  only. LOAD0 exec-only (flags=1) [0x0,0x7479b1c); LOAD1 R; LOAD3 RW covered
  by GNU_RELRO (identical span); LOAD7 RW [0x913c000,0x9633418 file,
  bss tail to 0x9633418). Under base 0x400000 these reproduce the igf vmmap
  spans exactly (prot 4/1/1/3).
- Ctor 0x23d590: `55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74 33 …`
  (byte-exact vs `worker-fname-fc1-materialize/REPORT.md:46-47`); rdi=sret,
  rsi=wchar_t*, edx=mode; tail `call 0x23d600; mov [rbx],rax`.
- Chain: 0x23d590 → 0x23d600 (underscore-suffix number split at 0x23d6c0
  `cmp word ptr […], 0x5f`; wide→narrow SIMD) → 0x23e0d0 → **0x242470**
  (ANSI variants call 0x242470 directly at 0x23d94f).
- **0x242470 LEAs pool `0x9365ec0`** three ways (init via 0x23a5d0 guarded by
  flag byte 0x9365ea0; FNAME_Find via 0x23c0b0; insert probe
  pool+0x10040 with 64-byte buckets via 0x23c340).
- Corroboration: init stores first block at pool+0x10
  (`mov [rbx+0x10],rax` @0x23a625), builds tables at +0x10040/+0x14040
  (0x23a618/0x23a656), interns "None"(0x7b96c17)/"ByteProperty"(0x7abcd83)/
  "IntProperty"(0x7bfb4f8) and stores ids at pool+0x14040/0x14044
  (0x23a727/0x23a732); find reads [pool+0x10]/[pool+0x18] (0x23c1b2/0x23c1ea);
  insert reads [pool+0xc]/[pool+0x10] (0x23c359/0x23c361). Matches the port's
  constants (`NameTypes.hpp:154-159`) and its ByteProperty comment
  (`port/unreal/src/Signatures.cpp:243-248`).
- Runtime candidate **0x9765ec0** sits in the readable RW span
  [0x953c000,0x9a34000); it is bss (zero in file — file bytes at that offset
  belong to a later LOAD and were not interpreted).

## 4. Task 3 — what the scan actually needs

- If it scans **code** in-game: dead (exec-only). This includes patternsleuth,
  Lua AOB, and detour verification — cite CURRENT-STATE.md:9 code-anchor block.
- If it derives from **data**: the port never needs the pool for FindFirstOf —
  it needs a **bound engine ctor**. The engine ctor itself consults the pool
  internally; the port only supplies the address. The derived pool label makes
  the ctor *validatable* data-side (pool reads) and behaviorally (ByteProperty
  probe), and enables the no-call Option 2.
- Exact resolution chain for the fix:
  composition carries (ctor 0x23d590, pool 0x9365ec0, base per-launch from the
  validated GUA pair 0x97ba110−0x93ba110) → data-side pool validation reads →
  `FName::ConstructorInternal.assign_address(base+0x23d590)` →
  `FName("ByteProperty", FNAME_Find, override)` id must equal the pool-read id
  at pool+0x14044 → `FindFirstOf("ItemDatabaseSubsystem")` meaningful at last.
  FNAME_Add must be avoided on this path (`NameTypes.hpp:315-318,319` — pool
  mutation on miss).

## 5. Verdict + remedy

Full statement in `FNAME-VERDICT.md` §1, §5. Summary:

- **Can FindFirstOf("ItemDatabaseSubsystem") work after composition-6 as
  composed today?** No — ConstructorInternal is never bound, silently.
- **Depends-on-X:** a single validated ctor binding (Option 1, preferred) or
  the data-side pool-walk alternative (Option 2). Option 1 needs exactly one
  engine call for validation (the ByteProperty FNAME_Find probe the port
  already uses); Option 2 needs none but costs a GUA scan (71,959 objects,
  2 chunks — cheap readable data).
- Both options follow the slots evidence standard: operand-derived file-image
  labels (never guessed), an in-game anchor proving the base (the already
  validated GUA pair), data-side validation reading the readable pool, and a
  behavioral cross-check — with explicit refutation conditions and no replay
  on mismatch.

## 6. Honest limitations

- Every derived address is an operand-derived **file-image label** from the
  frozen capture (same class as GameEngineTick 0x2fac8c0 in
  `worker-2-layout-facts/profiles/townfall-ps5-ue56.profile.json:365-388`);
  none has been read, called, or certified on target. The pool label has no
  in-game record yet; the validation reads are specified, not performed.
- The 161,503-ref global 0x9131f08 (also loaded by the FName functions) shows
  a read-compare-guard-then-abort pattern (0x24247f/0x242489/0x242557-76);
  likely a TLS-generation guard; **left unresolved**, not part of the chain.
- The "None"-entry arithmetic (entry = Blocks[Block] + 2*Offset, header u16)
  uses the port's constants + engine alignment; the first validation read will
  confirm or refute it directly.
- tool-call count ~55 of the 40-70 bound; no quality scores issued (no
  multiview/temporal evidence domain applies).
