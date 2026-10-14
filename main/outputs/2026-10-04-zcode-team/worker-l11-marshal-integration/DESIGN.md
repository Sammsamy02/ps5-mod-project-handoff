# DESIGN — the launch-11 marshal consumer (the DIFF-1 drain-consumer linkage)

Worker: worker-l11-marshal-integration, 2026-10-05. STAGED + HOST-PROVEN ONLY:
no composition run, no packaging, no console contact, nothing in-game.
Originals untouched; everything under `worker-l11-marshal-integration/`.

Abbreviations: `R3D` = `TEAM:worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md`;
`CC` = `TEAM:worker-r3-carrier-prep/CARRIER-CANDIDATES.md`;
`LM` = the staged LuaMod.cpp (this revision's staging copy);
`PROG` = `TEAM:worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp`.

---

## 1. The problem (what the dry run left)

The launch-11 admission path (DIFF-1 marshalled via the variant-B rebase) is
link-INERT: `arm_funcptr_marshal_drain`, `funcptr_marshal_hook`,
`r3_marshal_carrier_precheck` have no caller, and this toolchain eliminates
unused file-static functions even at `-O0` — they are absent from the unit
object AND the composed object (dry-run VERDICT §6, the stronger form). This
session reproduced the control byte-identically and re-measured the same shape
(`receipts/nm-control-all.txt`): only `m_funcptr_marshal_armed` +
`is_funcptr_marshal_armed` survive, and the latter only because DIFF-1's
non-ifdef'd `FuncPtrMarshal` ladder branch calls it.

## 2. Who the design says the caller is

Three sources converge; no new design was invented:

1. **R3D §5.2 item 4**: DIFF 1's fourth element is a "Calibration service:
   bounded candidate enumeration + trial hooks + counters (§4.2); runs
   post-`setup_unreal()`, before/parallel-with `mods-started`".
2. **worker-r3-diffs-authoring REPORT §4** (the DIFF-1 author's own handoff):
   "the enumeration/trial/selection protocol's driver remains future work and
   is the only intended caller of `arm_funcptr_marshal_drain` (file-static by
   design — the service lives in LuaMod.cpp per R3D)."
3. **R3D §4.4a**: arming is DEFERRED until the carrier is found; "first
   possible point: after `setup_unreal()`'s object world exists"; Lua can load
   and enqueue before arming ("Enqueueing before arming is safe and lazy").

Therefore: the consumer is the arming-service driver, it lives in **LuaMod.cpp**
(the arm function is file-static there by contract), and its lifecycle window is
post-`setup_unreal()`, pre-`mods-started`.

## 3. The exact lifecycle seam

PROG call chain (pins in CLAIM.json L11MI-2):

```
UE4SSProgram::init()                    PROG:610
  setup_unreal();                       PROG:628   <- object world exists from here on
  ...
  m_is_program_started = true;          PROG:664
  m_native_context.on_engine_initialized();  PROG:668-674
...
UE4SSProgram::update() (event loop)     PROG:1518
  on_program_start();                   PROG:1532
    install_lua_mods();                 PROG:1503
    LuaMod::on_program_start();         PROG:1504  <- THE SEAM (inside LuaMod.cpp)
    fire_program_start_for_cpp_mods();  PROG:1505
    start_lua_mods();                   PROG:1506  <- first Lua script runs after this
```

`LuaMod::on_program_start()` (LM:7255 pre-patch) is inside the patched unit,
runs exactly in R3D §4.4a's window, and — decisive ordering fact — its FIRST
eager detour registration is the `RegisterLoadMapPreCallback` a few lines into
its body (an `AddCallback` → `EnsureHooked` arm of the same class that throws
under BOTH_FAIL; Hooks.cpp:195-211 + DetourInstance AddCallback/EnsureHooked,
the DRAIN-TRACE hop-B pattern). The consumer call is therefore inserted at the
TOP of that body, BEFORE the LoadMap registration: the deferral window is used
before anything in the function that can throw. (Guarded `#ifdef __PROSPERO__`,
matching DIFF-1's convention.)

## 4. The consumer's decision logic (bounded, no new imports)

```
r3_marshal_calibration_consumer():                    # file-static, LM, under __PROSPERO__
  if is_funcptr_marshal_armed(): return               # idempotent (belt-and-braces)
  if !Unreal::R3MarshalPumpPending(): log; return     # W-works world: no store at all
  for name in {"Actor:ReceiveTick",                   # CC §3 rank 1
               "AnimInstance:BlueprintUpdateAnimation"}:  # CC §3 rank 2 (thread risk recorded)
    carrier = StaticFindObject<UFunction*>(null, null, StringType{name})   # data-side only
    if !carrier: log skip; continue                   # not interned / class not constructed
    if arm_funcptr_marshal_drain(carrier, carrier->GetFName()): return   # ARMED
    # else: precheck refused loudly (null FuncPtr / script-only / non-native) -> next rank
  log warning "no ranked candidate passed"; return    # closures queue; F-1 leans FALSE loudly
```

Design decisions, each traceable:

| decision | source |
|---|---|
| Arm gate = `R3MarshalPumpPending()` (DIFF-2's `pump_pending`), NOT unconditional | DIFF-2 sets `pump_pending` exactly when it compiled out the EngineTick registrations; arming in the W-works world would add a second, unnecessary pump. This reuses DIFF-1's own declared bridge (LM:93) so no new cross-unit contract is invented. Fold note: the bridges are defined by the initializer ONLY in the armed fold (`#if defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE`, initializer rev-B pairing law); the launch-11 staging is the armed fold (in-file flip =1 at line 179), and a mode-0 link fails loudly on the first bridge symbol — the correct refusal, inherited deliberately. |
| Candidate set = CC §3's ranked survivors, rank order = selection order | CC closed the candidate SPACE offline (`PostRender` etc. NAME-REFUTED); `ReceiveTick` outranks `BlueprintUpdateAnimation` (ctor cluster + 4 independent game-region refs; world-frame independence). |
| Rank 3 (`"Tick"`, owner AMBIGUOUS) EXCLUDED | CC §3: owner unresolved offline; arming an unknown-owner function is worse than a loud failure. Joins only after the cc1/in-runtime census resolves its owning class. |
| Resolution = `StaticFindObject<UFunction*>(nullptr, nullptr, "Class:Function")` | The same route the Lua `RegisterHook` native branch uses in this TU (LM:4829 pre-patch); pure data-side GUObjectArray-class walk; in-file precedent, zero new imports. |
| Sole arm gate = DIFF-1's `arm_funcptr_marshal_drain` (+ its `r3_marshal_carrier_precheck`) | The DIFF-1 author declared it "the only intended caller" contract; precheck refuses non-null-FuncPtr-less / script-only / non-native carriers LOUDLY with no store (R3D §4.3). |
| Carrier id = `carrier->GetFName()` | DIFF-1's idempotence key; matches its "idempotent re-arm of the same carrier" semantics. |
| Thread context = the event-loop thread (UE4SS-UpdateThread) | Same exposure class as Lua `RegisterHook` at script load (already runs there in-tree): data-side lookups + the one heap store via `RegisterPostHook` → `SetFuncPtr`. No exec page is touched anywhere in the chain (R3D §1.2: single aligned store into already-RW heap memory, zero syscalls). |

### What this consumer deliberately is NOT

It is NOT the full R3D §4.2 rate calibration: no trial-hook counters, no
rate/thread measurement windows, no demotion pass. Those steps are the F-1
falsifier and need in-game time to pass. The staged consumer is the
**deterministic ranked stand-in**: it converts the offline-closed candidate
space into an arm attempt at the designed lifecycle point, fails loudly when
nothing qualifies, and leaves the calibration session (cc1 census + rate
windows) fully able to revise the candidate table in a later revision with
distinct IDs. "Tick" stays out until the census resolves its owner.

## 5. Why the linkage is now real (the host proof)

Under the frozen 172-token argv (drift = source/-o/-MF only, per the dry-run
GO):

- **Control** (byte-verified base `4540593a…`): exit 0, object
  `5595feb7…` — byte-identical to the dry-run's verified gate object. The
  route is the dry-run's own; nothing else changed.
- **Consumer** (`04c9d916…` source): exit 0, object `854a476b…`
  (+130,520 B — the cluster now emitted). `llvm-nm`: all four symbols
  PRESENT (`arm_funcptr_marshal_drain`, `funcptr_marshal_hook`,
  `r3_marshal_carrier_precheck`, `r3_marshal_calibration_consumer` + the
  candidate table in `.data.rel.ro`). Relocations close the chain end-to-end:
  `on_program_start` → consumer → arm → {precheck, `&funcptr_marshal_hook`,
  `UFunction::RegisterPostHook`}, plus the `R3MarshalPumpPending` gate and the
  `StaticFindObject` call. stderr kind-parity: zero new/lost diagnostic kinds.

## 6. Known follow-ups (owned elsewhere, recorded NOT patched)

1. **The R3 world's remaining eager registrations**: after the consumer arms,
   `LuaMod::on_program_start`'s own `RegisterLoadMapPreCallback` still attempts
   a detour and will throw under BOTH_FAIL — killing `start_lua_mods()` (the
   `TRY` in PROG catches; mods never start; the marshal pumps nothing). DIFF-2
   compiled out the INITIALIZER's EngineTick + FName registrations only; R3D
   §5.3 item 3's "Leave LoadMap … dead under both-fail" obligation for the
   LuaMod-side registration is UNOWNED. A future revision must guard it with
   distinct IDs. Until then the R3-world boot still dies there — loudly.
2. **In-game calibration**: CC's cc1 census + R3D §4.2 rate windows decide F-1;
   the consumer's table may then be revised (e.g. add the census-resolved
   "Tick" owner, or demote rank 2 for thread instability).
3. **`StaticFindObject` name form**: the consumer uses the short
   `"Class:Function"` form (in-file RegisterHook precedent). If the live
   resolution misses, the skip line names the candidate — the fix is a table
   revision (full-path form), never a silent retry.
