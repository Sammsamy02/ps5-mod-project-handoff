# agent-EQ — Phase 5/6 silent-throw armor + L27/L26 pin-registry identity fix

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (consumption law) + profile/copyin-probe/driver delivery.
Console evidence: `/tmp/fixrun.log`, 2026-10-09 11:36 run (package-A rebuilt 11:32, variant A = `MODBRIDGE_L25_ENGINE_ADD_LISTENER`).

## 1. The silent throw: exact site found

The 11:36 log order was: slot 2 bound -> L27 refusal -> slot 4 bound -> `[L15] total elapsed ... outcome=verified-and-bound` -> `Stopping mod` with NO `[Seeds][L25]` line. Decisive detail: **the L15 line is the `L15BindingLog` DESTRUCTOR printing during unwind** (agent-EM's fixture proved this class at 10:37), so the throw happened between the slot-4 bind statement and the L25 line — i.e. inside Phase 6 — and:

- `seed_refused` (UnrealInitializer.cpp :807) **throws without logging** — it is only `throw std::runtime_error{fmt::format("[Seeds] REFUTED: ...")}`, and no upstream catch prints `what()` readably (nothing between the L15 line and teardown in the log).
- The Phase 6 `code_identity(engine_add_runtime=0x8841A0, 16)` failed exactly like the L27 check (same pin-registry cause, below) -> `seed_refused("engine add delete-listener op 0x4841A0: authenticated cached image/signature admission failed (variant C fail-closed)")` -> silent terminal throw -> RAII unwind (L15 dtor line; `CompletionCleanup` dtor completes the copyin cleanup silently) -> teardown.
- Not `SetupEngineDeleteListenerOps` and not the `:1491` `ensure_str` send (never reached; both would either log or garble-log).

**Armor applied** (candidate/src/UnrealInitializer.cpp, survived the parallel agent-ER merge, now sha `6701d75e...`): Phase 5, Phase 6 and the completion statements (`copyin_cleanup.complete()` + the L25 send) are each wrapped in `try/catch(const std::exception&)` + `catch(...)` that log `[Seeds] Phase N admission exception: <what>` via `send_narrow_diag` (narrow-first, literal fallback, cannot itself throw) BEFORE rethrowing — terminal semantics unchanged. The Phase 6 success line also drops `ensure_str(kImageSha256)` (halved-sha garble class) for the narrow-first sender.

## 2. L27 identity refusal: root cause + fix

The refusal string `constructor startup image identity/base unavailable or mismatched` is `NativeFNameChecks.hpp:18` — the `ops.code_identity` precheck inside `rc::fnameabi::signature`, NOT the :1101 "fresh startup whole-image" Phase-1 check and NOT the 10:37 copyin-fault reason.

`code_identity` -> `bridge_code_cache_context` -> `rc::codecache::named_entry` (**CodeImageCache.hpp:25-33**), which iterates ONLY the cache's registered pins and matches on **exact `e.runtime == address && e.size == n`**. The cache is prepared once at startup (bridge `bridge_code_cache_prepare`) from `rc::codeprofile::kProfile` — which registered only THREE pins: fname_ctor (slot 2, 0x63D590/32), SCO (slot 4, 0x890AA0/25) and the thread-name accessor. That is why slot 2 and slot 4 passed while the ToString label 0x63FA00/32 and BOTH listener ops 0x8841A0/16, 0x884240/16 failed: **they were never registered** — the seed-table-slot hypothesis in the task, confirmed verbatim (it is the profile PIN registry, not segment bounds: 0x63FA20 lies inside the same exec PT_LOAD that admitted 0x63D590).

**Fix (G1-compliant delivery):**
- `profile/units/CodeSignatureProfile.hpp` (`a4cd6c04...`): six pins — added `{4,"engine_fname_tostring_internal",0x23FA00,kToStringInternalPrefix,32}`, `{5,"engine_add_uobject_delete_listener",0x4841A0,kAddPrefix,16}`, `{6,"engine_remove_uobject_delete_listener",0x484240,kRemovePrefix,16}`, bytes single-sourced from the rc::engtostring/rc::englistener profile headers (same sha-pinned image), count 3->6.
- `profile/units/CodeImageCache.hpp` (`22803051...`): `kMaxPins` 4->8 (Metadata layout must be identical in every TU calling named_entry).
- Delivered as l26 profile candidates over **byte-untouched base originals** via two new `profile/CHANGES.json` rows (runtime side: vfs aliases; the first attempt edited base files in place and was reverted — `build_variants` G1 "BASE selection pin drift" is execute-fatal).
- `l26_packaging_driver.py` (`ee3aa230...`): new overlay destinations stage the candidates into the packaging lineage (`lifecycle/CodeSignatureProfile.hpp` shadows the outer-headers 3-pin copy; root `CodeImageCache.hpp`), plus the two profile headers as new pin-verified outer_headers rows so the bridge TU can include them.
- All six pin byte ranges re-verified against the pinned eboot (`outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`, sha `3b6cfad4...` MATCH) at file offsets label+0x4000: tostring 0x243A00, add 0x4881A0, remove 0x488240 (+ existing 0x241590/0x494AA0/0xA2A2E0 re-checked).

**Parallel reconciliation (mid-flight):** at ~12:18 agent-ER (Claude directive 2) deleted the L27 Phase 4c engine bind — FName::ToString is now the pure in-port pool decoder in NameTypes.cpp — while KEEPING the EQ armor intact (verified). Consequence: the L27 refusal can no longer occur (no bind attempt); pin 4 stays registered as dormant provenance. The pin registration remains **load-bearing for Phase 6**: the variant-C listener-op admission still uses the same code_identity/cached_code_signature pair and was the silent-teardown blocker.

## 3. Proofs

- **Fixture** `tests/actual_eq_pinadmission.cpp` + `tests/test_eq_pins.py`: REAL `Cache::prepare` (streaming sha256 + phdr parse + prefix extraction) over the REAL pinned eboot authenticates all SIX pins; `named_entry`/`prefix` admit 0x63FA00/32, 0x8841A0/16, 0x884240/16 AND the three pre-existing pins; RED control (fabricated 3-pin registry) reproduces both 11:36 refusals; armor order proof (logs full reason before rethrowing the ORIGINAL exception). **19 checks, plain + ASan/UBSan, exit 0** — receipt `results/HOST-TESTS-L26-EQ-PINADMISSION.json`.
- **EI stringfix fixture regression**: re-ran `tests/actual_ei_stringfix.cpp` plain + ASan/UBSan — PASS (armor is additive).
- **Prospero (Z13 recipe, real toolchain)**: `unreal-unrealinitializer` on the ER-merged base + re-anchored copyin-probe overlay + vfs-redirected l26 candidates — **exit 0, 6 known inherited warnings**, dep file proves both headers resolve exclusively to the l26 candidates. `runtime_start_bridge` (gated 94f81a17) against a faithfully reconstructed stage (outer headers + EQ overlay destinations) — **exit 0 under -Werror**, resolves `lifecycle/CodeSignatureProfile.hpp` (6-pin) + root `CodeImageCache.hpp` (kMaxPins 8). `native-thread-name-threadnameruntime` (second CodeImageCache consumer) with the redirect overlay — **exit 0**. Receipts in `results/eq-pinverify/`.
- **Copyin-probe patch**: hunk 3 re-anchored onto the armor completion wrap (same immediately-after-complete() position); applies cleanly (`/usr/bin/patch --forward`, no fuzz/rejects) to the current merged base; MANIFEST re-pinned (patch `c5c081fe...`; base expectation deliberately left at Q-era so composition always takes the patch-rebased path).
- **Gates**: `build_variants.py --plan` — 66 gates, exit 0, only false gate is `G2q-...-unrealinitializer.base` (documented expectation-vs-invariant, same as every run since the stringfix). `l26_packaging_driver.py --plan` — 9/9 ok. Compose mode `patch-rebased-onto-revised-base` x3.

## 4. Not mine / for the controller

- `tests/run.py`'s inherited head (`test_scan.py`) fails at generation — pre-existing (TypeChecker.cpp evolved in L26 while the fixture scaffolding didn't; none of EQ's files are its inputs; last full-suite pass was Oct 8 19:52 hardening). The EQ suite is standalone (`tests/test_eq_pins.py`) and also hooked at the tail of run.py.
- `NameTypes.cpp`/`NameTypes.hpp` CHANGES rows were observed drifting from live parallel edits at report time — their owner must refresh before `--execute` (G2).
- No console contact, no uploads, no build/packaging execution beyond --plan; next cycle: `build_variants.py --execute` then `l26_packaging_driver.py --prepare/--execute` per NEXT-BUILD.
