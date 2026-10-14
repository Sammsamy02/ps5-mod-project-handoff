# REPORT — worker-composition-4-fix

**Verdict: COMPOSITION-4 COMPLETED AND PACKAGED.** The composition-4 build script
was fixed in a copy under this worker's output dir, the full recomposition ran
clean into `…ue4ss-native-supplier-composition-4/run-3` (all gates passed), and
the PRX `--link` packaging succeeded with the same static-check result as the
accepted composition-3 precedent. No in-game claim; `target_executed=false`,
`console_contact=false`.

## 1. Root cause found (different from the task brief, documented honestly)

The task brief described the failure as "`--replace-runtime-unit` rejects
bootstrap unit names because they are not in the affected set". Readback of the
actual on-disk state showed a different, two-layer situation:

1. The **delivered** script (attested `7a83cb38…`, which produced the run-1
   `Unpinned fresh dependency` failure documented in worker-composition-4's
   REPORT) had **no bootstrap-replacement path at all** and no fallback in the
   fresh-unit dependency walk.
2. The **on-disk** script at `outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/build/build_runtime_composition4.py`
   was hash `b77fd70a…` — i.e. someone edited it in place AFTER the attested
   delivery, adding exactly the intended `--replace-bootstrap-unit` section and
   an `rr` fallback in the fresh-unit loop — but with a **fatal ordering
   defect**: `rr` (the replace-receipt pin map) was referenced in the
   fresh-unit loop (line 80) before its first assignment (line 86), so any
   unpinned dependency would raise `UnboundLocalError`; the extra-unit loop had
   the same latent bug. The bootstrap-replacement design itself was already
   present and correct.

**The minimal fix was therefore not "add bootstrap replacement" (already there)
but "make the existing attestation fallback actually defined before use".**

## 2. The fix (copy only; delivered file untouched)

Fixed copy: `outputs/2026-10-04-zcode-team/worker-composition-4-fix/build_runtime_composition4_fixed.py`
(sha256 `36cc8199b7e5a2528badd4da53ff3750075cbbe4d76ca115490b50a008ed8ff3`),
baseline `b77fd70a…` + exactly three surgical changes (full diff in the copy's
docstring; verified via `diff`):

1. **Hoist**: the `rr={}` + `idmap={}` replace-receipt construction (file-hash
   verification of every receipt entry + `include_dir_map` parse) moved to
   BEFORE the fresh affected-unit loop; the duplicate later construction
   (inside the bootstrap section) removed — one source of truth.
2. **Pins-on-fallback**: the fresh-unit loop's fallback now mirrors the
   bootstrap/extra loops: `if rp in rr: pins[rp]=rr[rp]; check(p,pins[rp])`
   instead of `check(p,rr[rp])` (attested replacement sources enter the
   receipt's `source_hashes` like every other pin).
3. Provenance docstring note pinning the baseline hash and the reason.

All frozen gates, replacement semantics and receipts are unchanged:
- `--replace-bootstrap-unit name=source` validates against
  `base['bootstrap_units']` (`activated_mod_source`, `runtime_owner`,
  `ue4ss_backend`), recompiles with the unit's ORIGINAL argv (with
  `include_dir_map` remapping `-I` dirs to the hash-attested bootstrap-stage
  copies — the exact mechanism composition-3 already proved: its receipt shows
  both units `bootstrap_replaced: true`), substitutes the new object into the
  link inputs via the `replacements` map, and pins the replacement sources
  through `--replace-receipt`.
- New units (`native-query-harness`, `module-runtime-ack-supplier`) go through
  `--extra-unit` + `--extra-receipt` as designed (WIRING.md §4: the ack
  supplier is NOT in the composition — it is a new unit).
- `--replace-runtime-unit unreal-unrealinitializer=…/staging/UnrealInitializer.cpp`
  now passes the dependency walk because the staging path is attested in
  `composition4-replace-receipt.json` (`933d08a2…`), which the hoisted `rr`
  provides.

## 3. Run — composition-4 into run-3 (full APPLY-ORDER Step 2 argv)

- `run-1` = preserved failure artifact (prior worker); `run-2` = empty
  pre-existing leftover of the interrupted in-place-fix attempt (16:26, before
  this worker; the script's `exist_ok=False` refuses it — not deleted, out of
  scope). This run used **run-3**.
- Log: `compose-log.txt` (exit 0). Receipt verified:

| gate | expectation (APPLY-ORDER) | measured |
|---|---|---|
| status | `NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED` | same |
| runtime_compilation_units | 368 | 368 |
| freshly_compiled_units | 15 | 15 (7 affected+elfsource, 6 extras, 2 bootstrap) |
| runtime_sha256 / bytes | from receipt | `45dd9901db7d626e63b8ac813bd8bf09f00938b1c5ecfa5f726ff7c9ff075af5` / 117,779,552 |
| undefined delta removed/changed | `[]` / `{}` | same |
| delta added | "clock_gettime, getpid" (table) | **`[sceKernelGetModuleInfo]` only** — see §4 |

Replacement landing (all verified in the receipt):
- `runtime_owner` / `ue4ss_backend`: `bootstrap_replaced: true`, fresh objects
  `bootstrap-runtime_owner.o` (`9677a7a0…`), `bootstrap-ue4ss_backend.o`
  (`e3406706…`), both substituted in the 368-entry `runtime_object_order`.
- `unreal-unrealinitializer`: `runtime_replaced: true`, consumed from the
  attested staging copy (`outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp`),
  pinned initializer `66691a88…` untouched on disk throughout.
- Extras `native-query-harness` + `module-runtime-ack-supplier` compiled clean
  and appended; `baseline_extended: true`.

## 4. Honest note on the delta expectation

Measured `added = ['sceKernelGetModuleInfo']`, not `[clock_gettime, getpid]`.
This is **correct, not a miss**: the delta is computed against the notify-cancel-1
base baseline (197 symbols), which ALREADY imports `clock_gettime`/`getpid` —
they are not "added" by the supplier. The only genuinely new import is the same
`sceKernelGetModuleInfo` composition-3 added, with the identical four-provider
family (`libkernel.so`, `libkernel_stub_weak.so`, `libkernel_sys.so`,
`libkernel_web.so` — receipt `providers`). Equivalently: the composition-4
delta vs composition-3's composed object is EMPTY — the S2-arm wiring, harness
and ack supplier contributed zero new undefined imports, and every harness
symbol (`Global()`, `ArmLayoutGate`, `DisarmLayoutGate`, `OnEngineTickDispatch`,
`InstallDefaultEngineBindings`, `PrepareActivatedUE4SSRuntime`, …) is DEFINED
by the linked composition. APPLY-ORDER's table was imprecise; the script's own
provider gate is the authority and passed.

## 5. Packaging (APPLY-ORDER Step 3)

`package_composition4.py` (this dir) = composition-3's documented
pinned-values-adjustment driver pattern applied to the UNMODIFIED fixed
`worker-prx-link-gaps/package_prx.py` (sha `e6fbe6c5…` verified before load):
composition_dir → `…composition-4/run-3`, object sha/bytes from the run
receipt, checkpoint `zcode_supplier_composition_4`, provisional ledger mirror
(`transport-ledger.composition4-mirror.json`; real ledger remains
controller-owned).

Result: exit 0, `prx-candidate-composition4-1/modbridge_supplier_composition.prx`
(30,778,567 bytes, sha256 `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f`;
bare-sdk `c8cb94ae…`), **17/17 static checks PASS, gaps `[]`** — byte-for-byte
the same check set as composition-3's accepted packaging (compared directly
from both build-receipts: both `mode=link`, 17 PASS, no non-PASS lines).
APPLY-ORDER's "20/20" was an inaccurate recollection; 17 is the complete set
and matches the precedent exactly.

## 6. Scope / non-interference

- Wrote ONLY under `outputs/2026-10-04-zcode-team/worker-composition-4-fix/`
  plus the script-owned run-3 build dir. The delivered integration-patch
  script, `build_runtime_composition4.py` (b77fd70a, pre-existing drift),
  run-1, run-2, all pinned files, frozen receipts and the composition-3
  artifacts are untouched.
- No console contact, no target execution, no nested subagents, no network.
- Call budget: well under the 60-call bound.

## 7. Artifacts (this dir)

- `CLAIM.json` (written first), `REPORT.md` (this file, last)
- `build_runtime_composition4_fixed.py` — the fix (`36cc8199…`)
- `compose-log.txt` — full composition stdout/stderr (exit 0)
- `package_composition4.py`, `package-log.txt` — packaging driver + log
- `transport-ledger.composition4-mirror.json` — provisional ledger mirror
- `prx-candidate-composition4-1/` — PRX candidate + build-receipt
- Run artifacts: `…/ue4ss-native-supplier-composition-4/run-3/` (receipt.json,
  undefined-delta.json, commands.json, input-hashes.json, all unit objects)
