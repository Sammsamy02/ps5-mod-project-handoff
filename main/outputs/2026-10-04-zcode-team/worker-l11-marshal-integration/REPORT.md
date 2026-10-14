# REPORT — worker-l11-marshal-integration (2026-10-05)

Mode: STAGED DIFF AUTHORING + HOST PROOF ONLY. No composition run (controller
owns serialized builds), no packaging, no console contact, nothing in-game, no
nested subagents. `launch10-build/`, the port tree, `worker-luamod-rebase/`
staging and every canonical receipt untouched; all deliverables under
`worker-l11-marshal-integration/`. Read-order honored: R3D in full, then
CARRIER-CANDIDATES, the DIFF-1/DIFF-2 pair + authoring REPORT, DRAIN-TRACE,
the rebase REBASE.md/REPORT.md, the dry-run VERDICT.md.

## 1. Task and delivery

Task: design + deliver the DIFF-1 drain-consumer linkage (dry-run VERDICT §6's
remaining decision) as staged, host-proven artifacts, on top of the variant-B
LuaMod.cpp, plus the launch-11 integration runbook.

| deliverable | content |
|---|---|
| `CLAIM.json` | 5 claims (L11MI-1..5) + non-claims + full pin list |
| `DESIGN.md` | who the design says the caller is, the exact seam, the decision logic, what the consumer deliberately is not, follow-ups |
| `DIFF.md` | the 3-way derivation (base `4540593a…` → diff `f595c57f…` → staged `04c9d916…`), round-trip proof, non-goals |
| `staging/luamod-l11-marshal-consumer/LuaMod.cpp` | the staged consumer source (clearly-owned copy; originals untouched) |
| `diffs/l11-marshal-consumer.diff` | the single-revision diff (2 hunks, 1 file) |
| `receipts/` | compile-gate.json, stderr parity, nm lists, undefined deltas, disassembly chain, probe + patch-roundtrip receipts, toolchain identity |
| `RUNBOOK.md` | controller compose steps, host + in-game acceptance oracles (tiered), follow-ups, rollback |
| `work/` | byte-verified base copy, control/consumer objects, patch-check scratch |

## 2. The design (one paragraph)

R3D §5.2 item 4 + the DIFF-1 author's own handoff (REPORT §4) name the missing
caller: the arming-service driver, file-static in LuaMod.cpp ("the only
intended caller of arm_funcptr_marshal_drain"). The lifecycle seam is
`LuaMod::on_program_start()` — called from `UE4SSProgram::on_program_start()`
(PROG:1504) which runs after `setup_unreal()` (PROG:628) and immediately before
`start_lua_mods()` (PROG:1506): exactly R3D §4.4a's window. The consumer is
gated on the initializer's own deferral state (`R3MarshalPumpPending`, DIFF-2's
`pump_pending`), so the W-works world performs no store at all; in the R3 world
it walks CARRIER-CANDIDATES §3's ranked survivors data-side (`StaticFindObject`
— the in-file RegisterHook route) and arms through DIFF-1's own
`arm_funcptr_marshal_drain` (precheck = sole gate, loud refusals, no store on
refusal). Call placement is BEFORE `on_program_start`'s LoadMap registration —
the function's first eager detour arm. Rank 3 (`Tick`) is excluded until the
census resolves its owner.

## 3. The proof (all green, receipts cited)

1. **Control route = the dry-run's route.** The recovered 172-token argv
   (drift = source/-o/-MF only) compiling the byte-verified base reproduces the
   dry-run's verified gate object **byte-identically**
   (`5595feb785206c0057eb29af0ccdcd53a8898d86629db0b10debc4a17aa08dea`).
2. **Consumer object.** Staged source `04c9d916…` compiles exit 0 to
   `854a476bdd4f998581f206af9faaa10b64eba8814fc590350c9565dce47f9565`
   (+130,520 B). stderr kind-parity vs control: zero new/lost diagnostic kinds
   (23 pre-existing warnings both sides, 0 errors).
3. **The dry-run's finding, inverted.** Control object: cluster ABSENT
   (reproduces `dryrunB-all-symbols.txt`). Consumer object: all four symbols
   PRESENT — `arm_funcptr_marshal_drain`, `funcptr_marshal_hook`,
   `r3_marshal_carrier_precheck`, `r3_marshal_calibration_consumer` + the
   candidate table in `.data.rel.ro`.
4. **Reachability, relocation-level.** `on_program_start` emits
   `R_X86_64_PLT32 → .text.r3_marshal_calibration_consumer`; the consumer emits
   the `R3MarshalPumpPending` gate, the `StaticFindObject<UFunction>` call, and
   `R_X86_64_PLT32 → .text.arm_funcptr_marshal_drain`; the arm emits the
   precheck call, the `&funcptr_marshal_hook` argument, and the
   `UFunction::RegisterPostHook` call.
5. **Link contract.** Undefined delta vs control = exactly three additions,
   all declared-contract symbols: the two DIFF-2 bridges (defined ONLY in the
   armed fold — initializer rev-B pairing law; the launch-11 staging is the
   armed fold, in-file `UE4SS_R3_MARSHAL_MODE=1` at line 179) and
   `UFunction::GetFuncPtr` (untouched unreal-ufunction unit, UFunction.cpp:28).
6. **Round-trip.** `patch -p1` on a fresh byte-verified base copy reproduces
   the staged consumer byte-identically (no fuzz, no rejects).

## 4. Key findings worth the controller's attention

1. **The elimination mechanism is unused-static DCE even at `-O0`** on this
   toolchain (verified `__PROSPERO__` is defined implicitly by the target —
   probe receipt — so the ifdef convention is not the cause). Any future
   "link-inert" audit can rely on: no caller ⇒ no symbol, at ANY opt level.
2. **The consumer inherits the rev-B fold pairing law for free:** a
   mode-0/fallback initializer link fails loudly on the first bridge symbol.
   That is the correct refusal, now linker-enforced.
3. **DIFF-1 ladder trap (BLOCKING for the spawner's default path; owned by the
   DIFF-1 author/controller, recorded not patched):** the ladders fall to the
   marshal only when the EngineTick ADDRESS is unavailable; in W_FAILS_R_WORKS
   the address IS assigned (vtable fallback), so the first default-method
   `ExecuteInGameThread` attempts the lazy detour and throws
   `EngineTick callback registration failed` — the armed marshal is bypassed.
   R3D §4.5 specified the opposite ordering. A distinct-ID revision must
   re-order the PROSPERO ladders.
4. **The R3 world still dies after arming:** `on_program_start`'s own LoadMap
   registration (and other unguarded eager registrations) still throw under
   BOTH_FAIL after the consumer runs. The consumer must run first — and does —
   but boot-to-Lua in the R3 world needs the follow-up guard (RUNBOOK §5.2).
5. **In-game acceptance for the marshal path must be tiered** (RUNBOOK §3.3):
   arm + pump lines are keypress-independent (engine-driven carrier); closure
   drain via the spawner's default path is blocked by finding 3 until that
   revision lands; explicit `EGameThreadMethod.FuncPtrMarshal` probes work
   today.

## 5. Honest limits

- No composition run: the consumer-integrated composed-object sha is unknown
  by design; the RUNBOOK's comparators are unit shas + structural checks.
- No in-game claim of any kind: arm success depends on live object state
  (interning, FUNC_Native, non-null FuncPtr, game-thread cadence) — F-1 stays
  OPEN; every consumer failure path is loud by construction.
- The candidate table is the offline-closed ranked stand-in, not the rate
  calibration; `ReceiveTick`'s real rate/thread behavior is unmeasured.
- Tool calls this session: well within budget; no web calls; no provider
  concurrency consumed beyond this worker.

## 6. File inventory (this worker dir)

```
CLAIM.json  DESIGN.md  DIFF.md  RUNBOOK.md  REPORT.md
diffs/l11-marshal-consumer.diff
staging/luamod-l11-marshal-consumer/LuaMod.cpp
receipts/{compile-gate.json, gate-control-base.stderr, gate-consumer.stderr,
  gate-*.warn-kinds.txt, stderr-parity.diff, nm-control-all.txt,
  nm-consumer-all.txt, undefined-{control,consumer}.txt,
  disasm-{on-program-start,consumer,arm}.txt, prospero-macro-probe.*,
  patch-roundtrip.txt, toolchain-version.txt}
work/{base-LuaMod.cpp, control-src/LuaMod.cpp, control-luamod.o,
  consumer-luamod.o, patch-check/}
```
