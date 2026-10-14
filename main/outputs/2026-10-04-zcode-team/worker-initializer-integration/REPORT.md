# REPORT — worker-initializer-integration (2026-10-04)

Merged the three converging change-sets into the single staged
`UnrealInitializer.cpp` unit, host-tested what is testable, compile-gated with
the composition's exact unit argv, and delivered the shipping argv + contracts.
No console contact, no nested subagents, no frozen/wiring file modified.

## 0. Verdict up front

The merged unit is DELIVERED and COMPILE-GATED (exit 0, zero new warnings,
exact run-3 composition argv). With the landed {0,2,4} seed table (gmalloc
unseeded) the compiled `ScanGame` IS the Shape-A named refusal — **the shipped
object contains no post-arming code path at all**: no seed binding, no scan,
no vtable walk, no detour. Launch 9's init thread stops loud at I3
(LAUNCH9-PREDICTION §2/§4-#2); module survives; `failed(1,<msg>)` ack.

## 1. What was merged (all in `staging/UnrealInitializer.cpp`, PROSPERO-gated; non-Prospéro behavior byte-preserving)

1. **Seed-table consumption (SLOTS §4b/§5 + landed table)** — `SeedTable.h`
   (generated) + `SeedArming::consume_required_slot_seeds`: per-launch base
   check via `ModuleSource->Views().array[MainExe].image_base`; slot 0 GUA
   probe+bind; slot 3 gmalloc probe+bind (when seeded); slot 4 SCO bind at the
   first-pass consumer; slot 2 ctor bind + pool validation chain +
   behavioral ByteProperty probe. Seed values enter ONLY through the generated
   header — zero inline addresses.
2. **FNAME-VERDICT remedy (Option 1, exactly)** — data-side pool chain
   (CurrentBlock≥1, Blocks[0] in igf6 heap span, "None"/"ByteProperty" entry
   resolution with header/bounds checks), bind via
   `assign_address(base+label)`, behavioral probe `FName(STR("ByteProperty"),
   FNAME_Find)` (never FNAME_Add) must equal the pool-read `+0x14044` id,
   divergence ⇒ `reset_address()` + throw ("refuted, no replay"). Pool
   not-yet-initialized shapes retry within the existing
   `SecondsToScanBeforeGivingUp` budget; once the shape holds, mismatches are
   terminal.
3. **DoScan/ps_scan gating** — armed seeds clear the four required config
   flags (+ the three never-resolvable secondary slots), skip the ps_scan
   retry loop, and guard BOTH Lua DoScan arms (`&& !bSeedsArmed`). FLAG-4
   closed on Prospero: the never-assigned `ftext_fstring` field is removed
   from the second-pass gate (its presence made that arm unconditionally
   live — the latent AOB re-arm trap); non-Prospéro keeps the original
   expression.
4. **A-PI honoring (FLAG-3)** — the Zydis derivation block is honored via the
   retained parse path, cited in-code: `[Hooks] HookProcessInternal/
   HookProcessLocalScriptFunction` ini → `SettingsManager.cpp:279-280` →
   `SettingsManager.hpp:116-117` (default true) → `UE4SSProgram.cpp:1222-1223`
   → the block's condition. Zero initializer changes needed; settings-v3's
   keys decision drives it (INTEGRATION-CHECKLIST item 6).
5. **FLAG-1 answer** — the seed-bound ctor skips `VerifyFNameConstructor`
   UNCONDITIONALLY (not gated behind the writability verdict): the behavioral
   probe established identity-equivalence the detour never could, and no
   detour is ever armed for FName. The tick path keeps the existing
   scan→vtable fallback (comment records the corrected tick address:
   runtime 0x33ac8c0 = base + linked 0x2fac8c0; 0x37AC8C0 was a
   runtime-mislabeled-as-linked convention error) and remains verdict-
   dependent, failing LOUD if the detour is refused.
6. **SHAPE A safe default + optional verdict gating** — arming requires ALL
   FOUR required slots; the compiled refusal names present/missing
   (LAUNCH9-PREDICTION: a partial-table continuation would hit the LoadMap
   vtable-walk text READ — igf6 crash class — before the honest PostInitialize
   throw). `SeedArming::kExecWritabilityVerdictGating=false` documents the
   designed-but-unarmed IGW verdict extension ({X->RX ok} gates vtable walks;
   {R->RW ok} gates a vtable-swap tick backend; absent → stays Shape A);
   the ini-ordering contract belongs to ridealongs. PostInitialize's
   "GMalloc is uninitialized." throw is untouched (defense-in-depth).

## 2. SEED-TABLE.json format spec (authoritative) + consumption

`SEED-TABLE-FORMAT.md` v2: canonical schema + the derivation dialect the
landed table uses (normalized by `gen_seed_table.py`; both verified to produce
identical slot values), evidence-standard enforcement (present slots need
anchor_bytes/rip_arithmetic/uniqueness; absent slots need the reason; seeded
slot 6/10 refuse), and the Shape-A arming table. `SEED-TABLE.conformed.json`
is the canonical snapshot of the landed values (0x93ba110 / 0x23d590 /
0x490aa0 labels; gmalloc under-determined per the derivation worker's
rejection evidence; tick correction recorded). `SEED-TABLE.example.json` shows
the armed shape (clearly marked; the generator refuses its placeholder).

## 3. Host tests (the testable part)

`staging/SeedValidation.hpp` — pure, engine-independent validation chain
(pool shape, entry bounds, entry ASCII match incl. wide-flag/length/character
refutations, pointer probes, probe-id guard). `host-tests/
host_seed_validation_tests.cpp` drives every check through pass AND refute
paths with fake-pool fixtures (including a full read-chain test and a garbage-
pool simulation): **ALL PASS** (`receipts/host-tests.final.stdout`). The same
header compiles into the merged unit.

## 4. Compile gate + composition-gate proof

- Exact run-3 unit argv (190 args, prospero-clang++): **exit 0**, zero errors,
  zero warnings from the merged files (`receipts/compile-gate.stderr`).
- Control experiment: the unmodified s2-arm source reproduces run-3's object
  BIT-EXACTLY — the tree is reproducible; all deltas are attributable.
- Shipped (Shape A, partial table) object: ScanGame folds to the refusal
  (0x4a4 bytes, no lambdas, no `ps_scan` import; 296 unreachable-tail externs
  drop out). **Composed undefined-delta simulated exactly against run-3's
  exports: identical to run-3's own delta (`added: [sceKernelGetModuleInfo]`,
  nothing removed, no binding changes)** — every dropped import is defined by
  another composed unit (`ps_scan` by native-psscanbinding) and the single
  added import (`fmt::vformat<char>`) resolves against the composed fmt unit.
  The S:167-179 gate passes with the existing baseline-extended argv.
- Armed-shape interim build (for the future complete table): full ScanGame
  (0xad7, all 17 lambdas), 0 lost externs, +75 additions, composed delta
  still identical to run-3.

## 5. Findings the controller should know

- **The landed SEED-INTEGRATION diff set had two gaps this merge closes:**
  its consumer blocks were gated on config flags it set false (seeds would
  never reach the consumers — fixed by explicit validated binding), and its
  "scan loop must be skipped" requirement had no diff (the loop would still
  time out — fixed by the armed skip).
- **Compile-time arming is a feature with a sharp edge:** the constexpr
  constants fold even at -O0, so the shipped object literally contains the
  refusal and nothing else (verified down to symbol level). A table update
  regenerates a different compile — never edit the object by hand.
- **The vtable-walk FLAG-2 read sits BEFORE PostInitialize** in init order —
  any future partial-arming temptation re-introduces the igf6 crash class;
  Shape A exists precisely to keep that unreachable.

## 6. Non-claims

- No seed constant is claimed identity-proven: slot 0 live-validated class;
  slots 2/4 derived-static-strong pending their in-game validators (FC-1 /
  sco_anchor); gmalloc unseeded, honestly.
- No console contact; the compile gate and host tests are local.
- Non-Prospéro compilation was not exercised (no such toolchain run here);
  those regions are #if-preserved textually.
- No completion, engine-readiness, or quality claim of any kind; launch 9 is
  predicted to stop at the Shape-A refusal, loudly.

## 7. Deliverables (this directory)

- `staging/UnrealInitializer.cpp` (merged unit, sha256 `c85c42f7…`)
- `staging/SeedTable.h` (generated, `2cab0c52…`), `staging/SeedValidation.hpp`
  (`b8d851e9…`)
- `SEED-TABLE-FORMAT.md` (authoritative spec, v2), `SEED-TABLE.conformed.json`,
  `SEED-TABLE.example.json`, `gen_seed_table.py`
- `host-tests/` (+ receipts), `receipts/` (compile-gate object/stderr,
  replace-receipt.json, host-test outputs)
- `REPLACE-ARGV.md` (the two-flag argv delta + gate proof),
  `INTEGRATION-CHECKLIST.md` (driver's companion steps),
  `CLAIM.json` (first), this `REPORT.md` (last).
