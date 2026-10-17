# REPORT — worker-layout-drift-audit (2026-10-05)

LOCAL ONLY. Object-level audit; no edits to any input; no composition run; no
console contact; no nested agents. Every tool call read receipts/.d files/sources
or ran llvm-nm/llvm-objdump read-only disassembly of existing objects. Deliverables
in this dir: CLAIM.json (first), DRIFT-AUDIT.md, this REPORT last.

## 1. Adjudication result (short)

The luamod-rebase §3 finding is CONFIRMED and SHARPENED on three points:

1. **The brief's retained/fresh split is mechanically wrong.** The named "RETAINED"
   units (lmf-sce-ops, lmf-base-scan, native-query-harness, module-runtime-ack-
   supplier, runtime_owner, ue4ss_backend) are all compiled FRESH at launch-10
   compose time (`build_runtime_composition6_fixed.py:185-201,203-228`), and the
   packaging BOOTSTRAP_UNITS are compiled fresh at packaging time
   (`package_prx.py:645-665`). The true retained set is the ~241 base-receipt
   pinned objects (compiled 2026-10-03, all pre-boundary) + activated_mod_source.
2. **The exposure is composition-wide, not per-TU.** `ld -r` collapses the weak
   inline definitions to one survivor per symbol, and the survivors are the STALE
   retained copies because retained definers precede program.o in link order
   (cppmod 29, luamod 30, settings 33; program 40). All six offset-bearing
   survivors in run-1 carry stale immediates (0x471 / 0x3c0 ×2 / 0x473 / 0x3c8 ×2)
   — measured by disassembly, not inferred. Fresh program.o's own calls
   (queue_event, the is_event_loop_thread guard sites) bind to these stale bodies.
3. **Only luamod has a direct execution-path exposure** (`get_program().m_mods`
   at −0x30 inside fire_on_lua_start/stop_for_cpp_mods, called from start_mod and
   uninstall). The other 11 header-closing retained units touch only statics,
   out-of-line calls, or pre-shift members; native-ownedmappedresolvercollector and
   activated_mod_source have zero closure. Every NativeProgramContext by-value
   flow (bridge → ack supplier → MakeNativeProgramContext → factory → program ctor)
   is fresh-fresh, and launch-9's configured-roots PASS proves the executed
   validator copy reads the NEW fields.

Minimal remediation: freshen cppmod, luamod, settings (the three retained definers
preceding program.o) via the luamod one-line-driver-edition precedent. The staged
L10LM `{'program','luamod'}` admission alone does NOT repair the four inline event
functions — cppmod.o still wins them.

## 2. Honest limits

- **Object-level evidence only.** No disassembly claims beyond llvm-nm/llvm-objdump
  symbol and immediate reads. I did not recompile any retained source myself; the
  fresh-vs-pinned 6-byte luamod delta is worker-luamod-rebase's receipt (control
  vs 2371839b...), which I verified is internally consistent with my independent
  derivation of the +0x30 class from the pre-edit/edit header diff.
- **No runtime claim of any kind.** The fault characterizations (silent
  queue_event drops, wrong thread-guard branches, garbage-vector iteration in
  fire_*) are static offset arithmetic on the measured shift. None of the six
  functions has ever executed in-game (launch-9 refused upstream of UE4SSProgram
  construction). "Near-certain crash" for the fire_* path is an inference from
  interpreting pre-shift bytes as a vector, not an observation.
- **Weak-resolution semantics**: "first weak definition in link order wins" is
  backed empirically (the run-1 survivor is stale while fresh program.o
  participates in the same link) and is standard lld behavior for -r dedup, but I
  did not read the LLD implementation; the winner is at least deterministic
  run-to-run (the composition's determinism twin passed).
- **I did not verify the remaining ~346 retained units by recompilation** — their
  safety is by construction from the .d screen (no header in the transitive include
  set), which is sound for direct layout closure but would not catch indirection
  through a data-driven offset table (none is known; the composition has no such
  mechanism in these units).
- **Launch-10 does not exist yet** (worker-launch10-build-witness REPORT: the
  controller one-pass has NOT run; its input verifications all PASS). Everything
  here is about the staged composition; the first real compose could still hit the
  fail-fast cases the L10LM worker documented in INTEGRATION.md §3.
- The same drift class exists in run-1 (launch-9 payload) and in any future
  composition with the same retained set and link order — including the L10LM
  edition unless cppmod/settings are also admitted.
- No completion, quality, or launch-safety claims. The two open launch-9 blockers
  (lease coverage, facts-adapter geometry) are unaffected by, and do not mask,
  this drift: both refuse before the drifted code paths.

## 3. Key file:line anchors

| fact | anchor |
|---|---|
| RETAINED_REPLACEABLE={'program'} | build_runtime_composition6_fixed.py:52 |
| affected-set assertion (six) | build_runtime_composition6_fixed.py:125 |
| extra units compiled fresh | build_runtime_composition6_fixed.py:185-201 |
| bootstrap replacements compiled fresh | build_runtime_composition6_fixed.py:203-228 |
| fresh-.d fail-closed gate (remediation path) | build_runtime_composition6_fixed.py:173-178 |
| launch-10 compose_argv (3 tagged edits) | build_runtime_composition6_l10.py:324-347 |
| ODR gate (port==staged header) | build_runtime_composition6_l10.py:229-251 |
| diagnostic sink substitution | build_runtime_composition6_l10.py:574-579 |
| BOOTSTRAP_UNITS + packaging compile | package_prx.py:128-140, 645-665 |
| the two inserted path fields | NativeProgramContext.hpp:77-78 (edited 0a068bb9..., mtime 2026-10-04 21:41:25) |
| context embed by value | UE4SSProgram.hpp:127 (9529fe76...) |
| four inline event-query bodies | UE4SSProgram.hpp:349-365 |
| native_mod_source (unshifted read) | UE4SSProgram.hpp:279-281 |
| luamod direct m_mods reads + call sites | notify-cancel-1/src/LuaMod.cpp:6348, 6368, 6463, 6571 |
| fresh callers of stale bodies | UE4SSProgram.cpp:2855, 2861, 2267-2516 |
| 6-byte +0x30 witness | worker-luamod-rebase/receipts/control-vs-pinned.disasm-diff (37e91c61...) |
| run-1 composed object (launch-9 payload) | ...composition-6/run-1 (4dd4f309...), packaged as prx-candidate-composition6-1 → fself 76ee6af3 |
| launch-9 stage reach | launch9-evidence/LAUNCH9-RESULT.md (refusals upstream of the initializer) |

## 4. Tool budget

~25 tool calls (find/ls/read/grep, python for receipt enumeration, llvm-nm /
llvm-objdump on 20 objects, shasum on 17 files). Zero writes outside this dir.
