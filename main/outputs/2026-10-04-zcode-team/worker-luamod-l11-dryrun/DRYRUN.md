# DRYRUN — worker-luamod-l11-dryrun (2026-10-05)

Step-by-step execution log of the worker-luamod-rebase `INTEGRATION.md` runbook
in a scratch area, toward launch-11. LOCAL ONLY, host-side. All shas are
sha256 (first 16 hex unless full). Read REBASE.md / INTEGRATION.md /
CLAIM.json of `worker-luamod-rebase` first, as instructed.

Scratch root: `outputs/2026-10-04-zcode-team/worker-luamod-l11-dryrun/`
(`MYD` below). Nothing outside `MYD` was written. The canonical launch-10
`launch10-build/` tree, `prx-candidate-*` dirs, `worker-fname-heap-runtime-bounds/`,
`worker-facts-geometry-fix/`, `worker-lease-coverage-fix/` were NOT touched.

---

## 0. Scratch setup (copies, never the originals in place)

- `MYD/scratch/build_runtime_composition6_fixed_l10lm.py` — BYTE-IDENTICAL copy
  of the L10LM E0 fixed-script edition, sha `9de2c4c8ccfbbdf8...` (matches pin;
  re-checked by the driver at run time).
- `MYD/scratch/build_runtime_composition6_l10lm.py.pinned-original` —
  BYTE-IDENTICAL copy of the driver, sha `9b332fc56ededa74...` (matches pin).
- `MYD/scratch/build_runtime_composition6_l10lm.py` — the executed scratch
  driver: differs from `.pinned-original` by EXACTLY TWO scope-isolation path
  redirects (`logs/scratch-driver-diff.txt`):
  - line 100: `MY` → `worker-luamod-l11-dryrun/scratch` (so logs, chain-symbols,
    default receipt all resolve inside `MYD`);
  - line 103: `COMPOSE6_FIXED` → the scratch copy of the fixed edition
    (driver's TEAM-based default would have executed the rebase dir's file;
    mission forbids running originals in place).
  No logic edits; `py_compile` OK.
- `MYD/scratch/receipts/composition6-replace-receipt.json` — BYTE-IDENTICAL
  copy of the regenerated 100-file receipt `27c1b3b0dc8d646a...`.
- `MYD/scratch/run_dry_l11.py` — harness that executes the scratch driver
  module's OWN `main()` sequence in runbook order and raises the mission STOP
  bound before `build_overlay`/`package_phase`.

The extra-receipt default (`worker-comp6-driver/receipts/composition6-extra-receipt.json`,
`0528d3d6...`) is TEAM-based in the delivered driver (not MY-based), read-only;
used as-is, exactly like the r3-armed run.

## 1. INTEGRATION.md §1 — sha gates: PASS (8/8)

Ran the §1 `shasum` block verbatim (`logs/sha-gates.txt`):

| target | expected | got |
|---|---|---|
| fixed_l10lm edition | 9de2c4c8ccfbbdf8... | 9de2c4c8ccfbbdf8... OK |
| l10lm driver | 9b332fc56ededa74... | 9b332fc56ededa74... OK |
| staged luamod LuaMod.cpp | 4540593ac9e66cdc... | 4540593ac9e66cdc... OK |
| initializer UnrealInitializer.cpp | 8b0c7e9905ce4a85... | 8b0c7e9905ce4a85... OK |
| initializer SeedTable.h | 59effe6e4cd34eb0... | 59effe6e4cd34eb0... OK |
| replace-receipt | 27c1b3b0dc8d646a... | 27c1b3b0dc8d646a... OK |
| package_prx.py (lineage) | e6fbe6c5011b498e... | e6fbe6c5011b498e... OK |
| frozen fixed baseline | 8dd92a6fe4b71cf1... | 8dd92a6fe4b71cf1... OK |

Lineage diffs: fixed edition vs frozen baseline = EXACTLY line 52
(`RETAINED_REPLACEABLE={'program'}` → `{'program','luamod'}` + comment)
(`logs/lineage-diff-fixed.txt`). l10lm driver vs r3-armed driver = 57 diff
lines, docstring + tagged L10LM E0/E1/E2/E3/E4/E5 sites only; the code changes
are precisely: MY path, COMPOSE6_FIXED path+sha, DEFAULT_REPLACE_RECEIPT,
initializer staging path, the new `luamod=` replace spec, vendor-origin
virtual `__file__` path (`logs/lineage-diff-driver.txt`).

## 2. INTEGRATION.md §2 — host re-verification, scratch equivalent: PASS

§2 as written would rewrite `LM/receipts/` (another worker's dir) — not run in
place. Equivalent executed in scratch: recompiled the staged variant-B source
under the EXACT pinned 172-token luamod argv + LLVM_CONFIG (extracted from the
base receipt `bcd23a2a...`, saved at `scratch/staging/luamod-argv.json`):

- exit 0 in 4.3 s; dependency walk = **1306 files** (matches gate 1306/1306).
- object `47fcda30d0db4b34...` (6,904,032 B) — differs from the gate's
  `5595feb7...` object ONLY because my scratch copy's filename
  (`luamod-l10rebase-LuaMod.cpp`) is embedded in the static-initializer symbol
  (`_GLOBAL__sub_I_luamod_l10rebase_LuaMod.cpp` vs `_GLOBAL__sub_I_LuaMod.cpp`):
  +16 B text, +45 B strtab, +48 B rela (section-level diff, nothing else).
  Confirmed by symbol sets: defined 7475 == gate 7475 (0 diff), undefined
  565 == gate 565 (0 diff).
- llvm-nm census (`-a`, all 25,373 symbols incl. file-static locals):
  `arm_funcptr_marshal_drain`, `funcptr_marshal_hook`,
  `r3_marshal_carrier_precheck`, `R3MarshalPublishGameThreadIdentity`,
  `R3MarshalQueryHarnessDispatch`, `R3MarshalPumpPending` — **0 present,
  0 undefined refs**; `m_funcptr_marshal_armed` (2), `is_funcptr_marshal_armed`
  (2), `FuncPtrMarshal` string (1) — the live DIFF-1 surface. The
  link-inertness finding is CONFIRMED in the stronger "eliminated" form.

## 3. INTEGRATION.md §3 — the build (dry-run edition: compose + chain gate, STOP before overlay/package)

Harness `scratch/run_dry_l11.py` executed the scratch driver's own main()
sequence in runbook order. Console: `logs/dryrun-harness.console.log` (run-a),
`logs/dryrun-harness-run-b.console.log` (run-b).

| step | result |
|---|---|
| `check_sha(COMPOSE6_FIXED)` | OK 9de2c4c8... |
| `odr_gate` | OK, port == staged header `0a068bb9d9b25f9b...` |
| `receipt_preflight` | OK: 100 replace-receipt files, 6 extra-receipt files |
| `edited_landed_gate` | OK (no unedited stand-ins) |
| `entry_preflight` | OK: entry `c97055f40f5a6dd9...`, both module entries, ≥2 alignment annotations |
| bridge pin | OK: `10385e2265af0a12...` |
| `compose_phase` (fixed edition, 172-token luamod argv redirected source/-o/-MF only) | OK run-a +75.3 s, run-b +30.4 s; **52/52 subcommands exit 0** incl. `retained-luamod`, `retained-program`, `link`, `undefined`, `exports` |
| `chain_symbols_gate` | OK: 14/14 startup-chain symbols defined (`scratch/staging/chain-symbols.json`) |
| STOP bound | build_overlay + package_phase NOT run (see §6) |

**FINDING L11D-F1 (runbook bug, worked around):** the delivered driver's
DEFAULT `--program-source` (`worker-luamod-rebase/staging/comp6-program/
UE4SSProgram.cpp`) does NOT exist — the rebase staging never re-staged the
program copy, and the regenerated receipt covers the canonical
`worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp`
(`75abf2ab77791f4e...`, sha-verified). The r3-armed run-a passed the canonical
path explicitly (its receipt proves it). The dry run passed the canonical
rr-covered path the same way. INTEGRATION.md §3's "all other arguments have
pinned defaults" is wrong for `--program-source`; a controller default-run
would fail in receipt_preflight.

### Run results (both passes)

| field | run-a | run-b |
|---|---|---|
| status | NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED | same |
| composed object sha | `633a8c82d71b6233989bc5ada83bc06ac6b65e1cf11ef769e00601b6ea2ccda4` | **identical** |
| composed object bytes | 117,742,096 | **identical** |
| units / retained / fresh | 368 / 353 / 17 | same |
| retained_units_replaced | `["luamod","program"]` | same |
| luamod.o sha | `5595feb785206c0057eb29af0ccdcd53a8898d86629db0b10debc4a17aa08dea` (6,903,984 B) | **identical** |
| undefined_delta | added `["sceKernelGetModuleInfo"]`, removed 0, changed 0, current 198, SDK-baseline extended | same |
| linked_prx / target_executed / console_contact | False / False / False | same |

(`logs/run-receipts-summary.json`; per-pass receipt.json shas differ by
embedded run-dir paths — expected; the object shas are the durable comparators.)

### Admission evidence (SEC32-3), from the run receipts

- `retained-luamod` compile exit 0; unit entry carries
  `retained_replaced: true, runtime_replaced: true`.
- argv drift vs the pinned base-receipt argv: indices EXACTLY `{165, 168, 171}`
  = source → `.../worker-luamod-rebase/staging/luamod-l10rebase/LuaMod.cpp`
  (`4540593ac9e66cdc...`), `-o` → run dir `luamod.o`, `-MF` → run dir
  `luamod.d`. 172 tokens preserved. (The SEC32-3 assertion
  `set(drifted)!={si,oi,mi}` would have refused anything else.)
- Source covered by the replace-receipt; fresh `luamod.d` walk 1306/1306
  pre-covered — no `Unpinned retained-replacement dependency` raised.
- `Affected header inventory changed` NOT raised (affected six untouched).

### Determinism (runbook §3 second pass)

run-a vs run-b: composed objects byte-identical; 368/368 unit objects
identical (incl. luamod.o and program.o); identical undefined_delta.
`input-hashes.json` maps differ ONLY in the 17 freshly-compiled `.o` path keys
(`run-a` vs `run-b` in the dir name); all 368 common paths have identical
shas, and the 17 fresh objects' shas are themselves identical across passes.

### Unit delta vs r3-armed run-a (INTEGRATION.md §4 check)

- 368/368 unit names equal.
- EXACTLY ONE unit object differs: `luamod`
  (`2371839b3d6fe0b4...` pinned → `5595feb785206c00...` fresh).
- `program` (`135392038b21a9d8...`) and `unreal-unrealinitializer`
  (`f5aa9efae0fdbbf2...`) byte-identical to r3-armed.
- Composed object +1,936 B vs r3-armed (117,742,096 vs 117,740,160);
  luamod.o itself +1,808 B (matches the gate's measured r3 additions).
- Import contract: luamod.o UND 565 == pinned 565 (identical name sets);
  composed-object undefined_delta IDENTICAL to r3-armed's (same single SDK
  baseline extension). I1/I1+ will still measure the real candidate.

## 4. Link-inertness on the REAL composed objects (llvm-nm)

- Composed run `luamod.o` (`5595feb7...`), `llvm-nm -a`, 25,373 symbols:
  drain cluster (`arm_funcptr_marshal_drain`, `funcptr_marshal_hook`,
  `r3_marshal_carrier_precheck`) **absent**; the three `R3Marshal*` bridge
  names **absent from the luamod unit**; live surface present
  (`m_funcptr_marshal_armed` local `b`, `is_funcptr_marshal_armed` local `t`,
  one `FuncPtrMarshal` key string). Zero undefined refs to any of the absent
  names.
- Big composed object (`633a8c82...`, 117.7 MB, 287,573 symbols): drain
  cluster names **0 hits**. `R3MarshalPublishGameThreadIdentity` /
  `R3MarshalQueryHarnessDispatch` / `R3MarshalPumpPending` appear as **defined
  (T)** symbols of the initializer unit with byte-same sizes in the r3-armed
  big object — lineage-inherited DIFF-2/R3 surface, NOT introduced by DIFF-1;
  UND sets for those names: empty in BOTH lineages
  (`logs/run-a-bigobj-R3Marshal-und.txt` vs `logs/r3-bigobj-R3Marshal-und.txt`).
- **Verdict on the finding:** the mission phrasing "drain cluster symbols
  present but unreferenced" is NOT what the objects show — the cluster is
  compile-time dead-code-ELIMINATED (file-static, no TU-internal caller), so
  there is no definition anywhere in the candidate to link against. The
  link-inertness FINDING HOLDS, stronger: the initializer's deferred pump has
  no consumer and nothing arms the marshal; a future caller requires first an
  emitted definition (non-static linkage or an in-TU caller). This is
  INTEGRATION.md §6's controller decision, unchanged by this dry run.

## 5. Artifacts in this dir

- `scratch/ue4ss-native-supplier-composition-6-l10lm/run-a`, `run-b` — the two
  full compose runs (587 MB total): composed objects, per-unit `.o/.d`,
  `commands.json`, `receipt.json`, `input-hashes.json`, `undefined-delta.json`,
  `runtime.map`. THESE ARE SCRATCH RECEIPTS, not canonical.
- `scratch/staging/` — dryrun-variantB object + `.d`, pinned argv extract,
  chain-symbols.json (driver-written), gate logs from the harness.
- `scratch/*.py` — fixed-edition copy (byte-identical), driver copy
  (2 path redirects), pinned-original copy, harness.
- `logs/` — console logs (both passes), sha-gates.txt, lineage diffs,
  scratch-driver diff, symbol lists, section-size diffs, R3Marshal UND
  comparisons, run-receipts-summary.json.

## 6. Deliberate stop (serialization requirement)

The runbook's remaining phases — `build_overlay` and `package_phase` — execute
the frozen shared `package_prx.py` (`e6fbe6c5...`, sha-checked) with in-memory
PIN injection. That is the same packaging pipeline launch-10 uses for its
controller-owned serialized builds. Per mission limits the dry run STOPPED
before it; **no prx-candidate dir was created** and no overlay dir was built.
Launch-11 packaging must be serialized against launch-10 by the controller
(one packaging process at a time on this 16 GB Mac).
