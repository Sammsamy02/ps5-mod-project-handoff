# MD-HEDGE-PROPOSAL — the M-D data-side dispatch backend under the 934 NID-routing law

Worker: worker-md-hedge · 2026-10-05 · staged-unit PROPOSAL only. LOCAL ONLY: no console
contact, no composition, no shared-lineage edits, no engine/provider execution, no game
claims. Wrote ONLY under `outputs/2026-10-04-zcode-team/worker-md-hedge/`. This document
extends, and is bounded by, `worker-walkguard-gate` REPORT §2 mechanism M-D; every prior
finding it relies on is cited, and every unproven kernel fact is named as open.

---

## 0. Basis (read and verified on disk this session)

| alias | file | anchor |
|---|---|---|
| WG | `TEAM:worker-walkguard-gate/REPORT.md` | M-D row (§2), GATE-1 trace (§1) |
| SWAP | `TEAM:worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md` | §1 perms, §2 slots, §3 D-1, §4 thunk, §5 R1-R4 |
| R3T | `TEAM:worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md` | §1 SetFuncPtr, §4 marshal, §6 verdict |
| MAP | `TEAM:worker-postadmission-map/ADMITTED-CHAIN-MAP.md` | steps 14/15/18/22, §3 dependency table |
| LAD | `TEAM:worker-r3-ladder/REPORT.md` | five availability-first sites, staged `6f97f8a1` host 40/40 |
| 934 | `CON:launch10-evidence/pid934-crashboot/EVIDENCE.md` | THE raw-syscall fatality proof |
| EW | `TEAM:worker-exec-writability/CLAIM.json` | EW-2 (igf5 crash-correlated), EW-4 (mprotect import-unavailable) |
| XWP | `TEAM:worker-igw-row0-rebuild/determinism/run-armed-1/import-contract.json` | the probe's OWN 8-import contract |
| CT1/CT2 | `CON:launch11-combined/prx-candidate-l11-{1,2}/supplier-contract.json` | 20:03 candidates, read by python over `diagnostics` |

`TEAM=` `outputs/2026-10-04-zcode-team/`; `CON=` `outputs/2026-10-04-zcode-controller/`.
NEW since WG was written: the 934 record — kernel dialog `PPSA33286 crashed 0xa002030a
SYSTEM_ILLEGAL_FUNCTION_CALL`, diagnosis "the raw `syscall` instruction is fatal inside the
sandboxed game process … the instruction itself" (standalone-payload successes ran in a
homebrew context); the mincore raw-syscall path was REMOVED and replaced by NID-routed
open/write/close/unlink "all in the 196-import contract". EW-2's igf5 raw-syscall-74
crash-correlation (previously "leading hypothesis") is thereby mechanistically confirmed
as a class: **any raw `syscall` instruction executed in-game is process-fatal.**

---

## 1. D-1 CONTRACT VERDICT — mprotect in the composition's imports

Method (the controller's): python over `supplier-contract.json` → `diagnostics` rows,
per-row `name/binding/symbol_type/status`, plus the top-level `counts` and
`weak_unresolved_names`. Read of BOTH current candidates (CT1 `prx-candidate-l11-1`,
CT2 `prx-candidate-l11-2`); they agree exactly.

1. `counts`: `required_strong: 196, covered_strong: 196, missing_strong: 0,
   required_weak: 6, conflicts: 0`.
2. `diagnostics` (198 rows): 187 `unique`, 5 `supplier_extension_added`, 6 `absent`.
3. **THE VERDICT: no `mprotect` / `sceKernelMprotect` strong row exists.** The only
   mprotect-bearing row in either candidate is:

   ```
   kernel_mprotect | binding: WEAK | symbol_type: NOTYPE | status: absent | evidence: []
   ```

   and it sits in `weak_unresolved_names` next to the five `__dl*` names. This is the
   libc-internal mprotect() weak dependency (EW-4): in the composed PRX toolchain it
   resolves to **0**, so calling libc mprotect() in-game jumps to address 0. The Packager
   rejects weak imports outright (`XWP mprotect_verification.packager_rejects_weak_imports:
   true`), so it cannot even be declared as weak for the composition.
4. The only typed `sceKernelMprotect` anywhere in-tree is the **xwp probe's own contract**
   (`XWP`, 8 strong imports): provenance string verbatim —
   `"sceKernelMprotect": "derived:SharpProspero-StubCatalog (NOT console-proven; LOUD FLAG)"`.
   That is the probe PRX's import surface, NOT the composition's. The same probe contract
   records `"raw_syscall_route": "FORBIDDEN (igf5 crash class; inverted gate)"` and drops
   `igcar_sys_mprotect` from its identity set.
5. **The extension precedent is real but weaker than it looks for mprotect.** The 5
   `supplier_extension_added` rows (sceKernelDlsym, sceKernelGetModuleInfo,
   sceKernelLoadStartModule, sceKernelStopUnloadModule ← `sdknm-libkernel.so.stdout`;
   qsort ← `sdknm-libSceLibcInternal.so.stdout`) each carry REAL supplier export evidence:
   the symbol was OBSERVED in the supplier's export dump. An `sceKernelMprotect` row would
   be a different class of extension: **catalog-derived NID, zero export evidence** —
   EW-4's bases stand on the current contract generation ("zero mprotect strings in
   observed libkernel exports; no mprotect NID in any SDK NID table"), and CT1/CT2's
   `kernel_mprotect` row with `evidence: []` is the direct signature of that absence in
   the 196-import world.
6. **No substitute capability exists among the 196.** Enumerated memory-adjacent names:
   `munmap`, `sysctl`, `getpagesize`, `shm_open`, `shm_unlink`, `memcpy` (all
   strong/unique) — none grants write access to a read-only page (`write()` targets file
   descriptors; `memcpy` obeys page protection; `sysctl` only reports the map). No
   `mmap`/`mlock`/jit/`mincore` (mincore is gone per the 934 correction).
7. **Avoid-mprotect sub-verdict (task question): NO.** The vtable span L3 RELRO
   `[0x82bc000,0x953c000)` is kernel-observed prot=1 — READ-ONLY at runtime (SWAP §1.1);
   the W bit was stripped by the loader's own RELRO processing and no in-contract symbol
   re-issues a protection change. A store from module context faults. Therefore **D-1 (a
   prot change) is mandatory for the vtable tier**, and under the 934 law that prot change
   must be an NID-routed import. **M-D viability hinges entirely on the mprotect import
   question.**

---

## 2. Design consequences of the 934 law (what changes in M-D)

1. **SWAP §3's mprotect step is dead as written.** It specified "raw syscall 74 …
   constants build-parsed from the pinned SDK". Under 934 that instruction is
   process-fatal in-game. Both transitions of a swap session (R→RW before the store,
   RW→R after) must route through the **same in-contract import** — which §1 shows does
   not exist today. D-1 is therefore not just an open kernel fact; it is an open
   **contract** fact with a kernel fact behind it.
2. **The composition stays 934-clean on this path.** The pinned walkguard unit stays
   fold-0 (preprocessor-dead, zero syscall text — walkguard-prep probe 0, object form
   composed into l11-1). U-MD-2 below never enters exec text, so the guard never runs;
   no syscall instruction exists on the M-D path at any fold.
3. **CROSS-CUTTING FLAG (controller-owned, outside my edit scope, recorded because it is
   load-bearing for the primary route):** the measured fold-flip object (`f5627427…`,
   walkguard-prep §4 probe 2) carries exactly **1 syscall instruction** (inline 74). Under
   the 934 law, even a T2 `rc==0` verdict does NOT make that object safe to boot: the
   guard's first X→RX transition would execute a raw syscall instruction. The WST-5
   admissibility law needs a 934 amendment — fold=1 admissible only together with an
   NID-routed guard revision (which itself needs the same import extension as M-D), or a
   controller-level re-scope that re-proves raw syscall-74 as an exception (currently
   against the 934 "the instruction itself" attribution and EW-2's igf5 correlation).
   M-A and M-D thus converge on the SAME gate: one in-contract mprotect import.
4. **Sequencing law unchanged:** SWAP §3 — D-1 is meaningful only after IGW-1/T2's outcome
   is reconciled; never replay a failed or fatal attempt (934 is reconciled-only).

---

## 3. The bounded staged-unit design (PROPOSAL — nothing built or staged)

### U-MD-0 — the deliberate import extension (the gate M-D lives behind)

Only after T2's record chain exists, and never silently:

- **Preconditions (all required, all console-evidence-bearing):**
  1. T2 probe LOADS in the game process — its 8-strong-import contract passes the
     loader's import validation with the `sceKernelMprotect` row present. If the loader
     refuses the catalog-derived NID, the probe is ABSENT-class and the extension is dead
     (a NID the system will not resolve cannot be imported by the composition either).
  2. The T2 `attempt mprotect_route=sceKernelMprotect_import rc` line exists with a REAL
     kernel rc. Any rc proves NID resolution + call reachability; `rc==0` additionally
     proves transition permission on that page class.
  3. Controller/user acceptance of a deliberate contract change on the exact probe
     declaration (same StubCatalog NID, same module/library metadata shape, loud
     provenance flag carried into the composition contract + manifest), landing as a
     distinct-ID composition revision with a 196→197 counts-delta receipt.
- **Honest counter-evidence carried on the unit:** the same `sdknm-libkernel.so.stdout`
   dump that evidenced all four sceKernel* extensions shows NO mprotect export — the
   extension asks libkernel to service a NID it does not publish. T2 is the falsifier;
   this unit must not be armed before T2 speaks.

### U-MD-1 — DispatchDataDetour backend (SWAP R1, re-based to the import route)

- Surface: the exact consumed-PLH interface — ctor `(uint64_t fnAddress, uint64_t
  fnCallback, uint64_t* userTrampVar)`, `hook()`, `unHook()`, `isHooked()` — selected by
  the PROSPERO typedef at the single construction site (SWAP §5 R1). Invoke, pre/post
  vectors, GC, invocation scopes byte-unchanged.
- `hook()` sequence (all gates fail-closed-loud, ERROR_ID → the existing loud-throw path):
  1. Resolve `fnAddress` in the slot registry (U-MD-2); miss ⇒ refuse.
  2. **Mapscan gate** (sysctl 202 — in-contract, proven class): fresh span walk must cover
     the slot; prot must be 1 (vtable page) or 3 (heap page); prot=4 ⇒ refuse (never exec).
  3. **D-1 gate — in-contract import call** (`sceKernelMprotect(page, 0x4000, R|W)` via
     U-MD-0), never an inline syscall. Record rc write-through BEFORE proceeding;
     rc!=0 ⇒ this hook refuses loudly; other targets may still try their own pages.
  4. **Identity pre-check** (SWAP §2.1): read the slot; require it equals the
     independently resolved original entry. Wrong offset/class ⇒ loud refusal, no write.
  5. **Swap value:** the per-target `StaticHookFn` thunk address — an ordinary static
     function in the PRX's own .text (module code proven executable in-game); SysV AMD64,
     signature = the original's; no allocation, no mmap, no RWX page, no instruction byte
     written anywhere. The ctor's `Trampoline` out-word receives the **saved original
     entry** (never a re-encoded trampoline — the original's bytes are untouched, so
     "the trampoline" is simply the original function; prologue replication/disassembly
     are never used).
  6. Single aligned 8-byte store; readback-verify; **re-protect R via the same import** +
     fresh mapscan; verify prot back to 1. Restore anomaly ⇒ fail-closed latch (stop the
     ladder; no further installs from an unrestorable page) — the igf restore discipline.
- **Restore discipline:** write-window restore is mandatory and fail-closed as above;
   teardown restore does not exist and must not be promised (PROSPERO `DeactivateHook`
   refuses to unhook and keeps forwarding — swap hooks are process-lifetime, matching
   that precedent; `unHook()` returns false outside the same external-quiescence
   conditions PLH's would need).
- **Crash-safety vs racing game threads (the bounded case):**
  - No instruction byte ever changes ⇒ the classic PLH prologue-patch race (a thread
    executing a half-patched prologue) **cannot exist** in this architecture.
  - Readers load the slot unlocked; an aligned 8-byte load never tears — a racing call
    lands wholly on the original or wholly on the thunk, never a mix. A thread that
    loaded the original just before the store completes its call into unmodified code.
  - Commit order (strict): state initialized → `OriginalEntry` stored → `Armed.
    release(original)` → slot store → readback → re-protect + mapscan. The thunk reads
    `Armed` (acquire) before first use of `OriginalEntry`.
  - Re-entrancy recurses through `StaticHookFn` exactly as with PLH; existing PROSPERO
    recursion scopes apply. All swaps on the one init thread behind `InstallMutex` —
    single writer, no second writer exists.
- **Batching:** per 16-KiB page, one protect/swap-all-slots-on-page/verify/re-protect
  cycle, never two pages in flight (ProcessEvent family, R4).

### U-MD-2 — data-only slot capture at the resolve sites (SWAP R2; the M-C shape, safely consumed)

- PROSPERO-only recorder variant of `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL`: performs
  READ-A (the CDO's vptr) and READ-B (the slot word) — the two data reads the walks
  already do, both proven working today (WG §1 items 5, READ-A/READ-B) — and pushes
  `{slot_addr, entry_value, class_instance}` into the registry. It does **NOT** call
  `resolve_function_address_from_potential_jmp`: zero exec reads, zero Zydis, zero guard
  involvement, walkguard stays fold-0/dead.
- Why the WG M-C rejection does not apply here: M-C was rejected because a raw entry
  would later feed a PLH detour's own naked disasm+write (the unguarded GATE-2 class).
  Inside M-D the consumed backend is DispatchDataDetour — no PLH disasm/write exists on
  this path, and under the 934 law no exec-write backend can exist at all. The capture
  is a registry fill, not a hook arm; nothing executes through `entry_value` itself.
- Slot/target selection (task question "LoadMap? Tick?"): **first swap target =
  GameEngine::Tick (C4)** — one class, one live instance, every virtual call site routes
  through the one slot, and the query-harness two-consecutive-reads counter is the
  in-game falsifier (SWAP §2 row 1, §5 sequencing "EngineTick remains the first real
  hook under all branches"). **LoadMap (C3) is capture-only initially**: the gate5 fold
  in facf993a already defers LuaMod's eager LoadMap arm, and the only LoadMap consumer
  is the spawner's optional, pcall-guarded A10 — swap it only if that rung is promoted.
  ProcessEvent (C8) is the R4 family (batched per page, registration-time coverage
  check mandatory — a lone UObject-CDO swap must never be counted as coverage). C5-C7/C9
  follow the same mechanics, single-class swaps only where a live instance is found.
- The 16-entry vtable DISPLAY walk stays dead (gated on non-null scan_address under
  armed seeds — MAP step 15).

### U-MD-3 — standing companion requirements (unchanged by M-D, restated because they gate the same boot)

- Settings-v3 `[Hooks] HookProcessInternal=0` + `HookProcessLocalScriptFunction=0` remain
  REQUIRED in the staged root: step 18's ProcessInternal derivation is an unguarded Zydis
  read of exec text, independent of hook backend; swaps do not remove that dependency
  (MAP step 18, FLAG-3). Without the keys the boot dies mid-`Finding ProcessInternal`
  with no failed line — in EVERY world, including a fully armed M-D.
- FName ctor verification stays FC-1-or-nothing: not dispatch-reachable (direct `E8`
  sites); under T2-refuses its reads have no RX, so the seed-bound slot-2 behavioral
  probe (data-side, MAP step 5) is what carries verification.

---

## 4. Composition with the R3 heap tier and the merged-luamod `facf993a` ladder

- **R3 SetFuncPtr heap tier (zero-syscall, zero-mprotect, zero-contract-delta):**
  `UFunction::SetFuncPtr` is one pointer store into heap memory (Func offset 0xD8 for
  5.06+; igf4-proven write class; per-target mapscan gate retained because of the L7
  R-only hole). It carries the pump (FuncPtr-marshal) and native `RegisterHook` in ALL
  worlds — it is the part of M-D that survives every verdict below (R3T §1, §6).
- **facf993a** (r3-ladder `6f97f8a1` + gate5-loadmap `9e4616d1` on base `c92c92ff`)
  supplies, in the successor admission: the marshal arm + calibration consumer
  (carriers Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation, F-1 open); the
  marshal-first-when-armed enqueue gate for the default ExecuteInGameThread
  (LM:5041-5056 + ProcessEvent mirror); the repaired five availability-first sites
  (WithDelay top-gate; AfterFrames loud marshal-world refusal; host 40/40); and the
  LoadMap defer + one-shot re-arm that keeps LuaMod's eager registration from fighting
  the swap backend.
- **Interaction matrix:**
  - **Swap-armed world** (D-1 rc==0): U-MD-2 assigns the Tick address ⇒
    `is_engine_tick_hook_available()` reads TRUE; the Tick swap arms through
    DispatchDataDetour (no ensure-throw). Lua closures still route marshal-first while
    the marshal is usable — exactly the r3-ladder host P1 world ("both alive": marshal
    wins deterministically, zero detour attempts). Division of labor: the swap backend
    carries initializer-side virtual hooks (Tick dispatch now, ProcessEvent family at
    R4); the marshal carries the Lua pump and native RegisterHook.
  - **Swap-refused world** (D-1 rc!=0 per page): `hook()` refuses loudly; composition
    shape is a controller decision between the two in-tree precedents — the fatal
    ERROR_ID path (current EnsureHooked behavior) or the R3-deferral shape
    (`pump_pending=true` + the `[R3] … deferring` line, UI:2069-2085). Honest default
    proposed: deferral, so a D-1 refusal on ONE page does not lobotomize the boot.
  - **Marshal-only world** (M-D dead, §5 V3): facf993a's marshal-first ladder IS the
    surviving pump; all vtable-tier hooks die; A10 LoadMap lifecycle degrades to the
    known pcall-tolerated no-op (stale-menu hazard recorded, R3T §2 step 2); global
    visibility rungs are unratable (missing-evidence rule).

---

## 5. Honest verdict table

| # | world | gate facts | M-D verdict | evidence each step needs |
|---|---|---|---|---|
| V1 | **T2 works** (`attempt … rc==0` on the exec tail page; probe loaded) | Route proven: the NID resolves and the kernel accepted a prot change on prot=4. Page class proven: exec tail only. | M-A remains primary per WG §3 — **but the 934 flag (§2.3) adds a precondition the WG report could not see: the measured fold-flip object carries 1 raw syscall instruction and is expected process-fatal at its first transition.** The flip is composable only with an NID-routed guard revision (same import as M-D) or a controller re-scope re-proving raw syscall-74. M-D's swap backend becomes optional (and arguably preferred for Tick: identity pre-check beats PLH's unvalidated patch) — still via the U-MD-0 extension. | T2 record chain (vmmap_persist span_prot=0x4 pre-proof; attempt route+rc; rw_test read_done=1; restore outcome=10; post_restore match) + deliberate contract extension receipt (196→197, loud provenance) + the fold-activation law amendment. |
| V2 | **T2 refuses with route alive** (real kernel `rc<0`; import resolved, transition denied on the exec page) | Resolution proven; exec-page transition denied. RELRO data (prot=1) is a DIFFERENT page class — D-1 stays open. | **M-D ALIVE conditionally**: U-MD-0 extension (route already proven by the attempt line), then a NEW D-1 import-routed same-value-writeback probe on a far page of `[0x82bc000,0x953c000)` (SWAP §3 design, re-based off the raw syscall). Swap backend proceeds only on D-1 `rc==0`; D-1 `rc<0` collapses to V3. | T2 attempt line with real rc + extension receipt + the D-1 probe's own record chain (same five-line grammar, data page, zero semantic delta even in crash). |
| V3 | **T2 absent or fatal** (probe fails to load / import unresolved / process-fatal outcome) | No mprotect mechanism exists in-game: raw `syscall` = process-fatal (934, proven class); libc mprotect() = jump to 0 (EW-4, kernel_mprotect weak-absent with evidence [] on BOTH current candidates); no export to dlsym-route (sdknm dump; EW-4); no substitute among the 196. | **M-D DEAD.** Vtable-swap backend dies with detours. Left: R3 heap tier (SetFuncPtr marshal) + port-owned polling only (SWAP §5.1 both-fail row; R3T §6 conditions: F-1 carrier fact + the two diffs + worker-1/2 facts — spawner A9 completion remains possible; global-visibility rungs unratable). M-C raw-entry skip is worthless here: its only consumer would be a hook backend that cannot arm, and unguarded exec reads remain the FLAG-3 crash class the settings keys must skip. M-E settings avoidance relocates GATE-1 to the next enabled flag and lobotomizes features — honest but insufficient alone. The facf993a marshal-first ladder is the surviving pump; the boot still needs GATE-1 relocated or the chain stops at step 14 in every composition that keeps the resolve sites enabled. | The T2 ABSENT/fatal record itself, reconciled only — never replayed (no-replay law; 934 is the precedent: diagnosis, removal, NID-routed replacement). |

---

## 6. Explicit non-claims

- No claim any swap, mprotect call, import extension, or D-1 probe has run or will run;
  T2 is unexecuted; D-1 is unproven; the extension's resolvability is unproven and
  evidence-negative in-tree (sdknm shows no mprotect export).
- No claim about UE 5.6 vtable indexes or any vtable's exact host span — runtime vptr +
  fresh mapscan remain the only authorities (SWAP §7).
- No claim the current composition is 934-clean end-to-end: this proposal only asserts
  the M-D path adds no raw syscall instruction (fold-0 walkguard, import-only D-1,
  data-only capture); the copyin/mincore correction and any other paths are their
  owners'.
- Nothing staged, composed, flipped, or launched; no shared-lineage file touched; no
  console contact; no outcome claims for any future boot.
- Contract reads are of the 20:03 l11-1/l11-2 candidates; any successor re-composition
  (e.g., the next-candidate manifest's copyin-wired trio) must re-run the §1 python
  check on the new contract before relying on this verdict.
