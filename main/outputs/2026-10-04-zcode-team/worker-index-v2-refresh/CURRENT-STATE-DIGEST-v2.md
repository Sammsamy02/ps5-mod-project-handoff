# CURRENT-STATE-DIGEST-v2 — the 3-minute read (refresh 2)

Compiled by `worker-index-v2-refresh` (read-only synthesis; no console contact; nothing launched).
Baseline: v1 digest absorbed ~20 deliveries since; ledger now 234 `zcode_*` entries (last: `zcode_worker_fc1_validator_probe` 03:52:45Z).

---

## THE headline: what launch 9 will actually test, and its honest expected outcome

**Launch 9 = composition-6.** The full piece set is on disk and audited: bridge r12 (configured roots `/app0` + launch-8's page-quantized lease fix + identity gate) + adapter Resolution-B opt-ins + D2 text-base guards + entry r4 (timestamped failure prefixes) + the merged staged initializer with **Shape-A seed arming** + the final seed table (3 of 4 required slots seeded) + settings v3 + four ridealongs (observer, qdrv1, fc1 validator, IGW-1 exec-write probe).

**Honest prediction (worker-launch9-prediction, the project's own most-likely case):**

1. **WILL be tested for the first time on console:** the r11 page-quantized read lease, the D2 text-base guards, and configured roots — launch 8 proved conversion+context once; these three have never run in a real launch. Expected: they pass (dry build passed the offline gate 8/8; preflight GO_WITH_FIXES all applied).
2. **Then launch 9 STOPS — loudly, before ANY engine-ready path and before any exec-page touch.** The compiled seed gate (Shape A: all-4-or-refuse) fires the named refusal at ScanGame entry:
   `[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc]`
   — a `failed(1,<msg>)` ack, module survives, game safe, NOTHING bound, no scan, no vtable walk, no detour (the compiled object contains no post-arming path — verified by compile-time folding). Do not confuse it with `[Seeds] REFUTED:` (a different, consume-path message).
3. **Engine-ready, mods-started, and the Item Spawner menu are NOT expected in launch 9.** Three things block past the gate, in init order: (a) GMalloc unseeded — the only REQUIRED slot without a derivable constant (0/14 patterns in the pinned image; PostInitialize would throw `GMalloc is uninitialized.` even in a world where exec pages are fully readable/writable); (b) the vtable-walk exec READ (ASMHelper jmp-probe, no PROSPERO branch — the igf6 crash class); (c) the tick-detour exec WRITE (PLH x64Detour RWX+patch). Shape A was chosen precisely so a crash-shaped death cannot masquerade as progress.
4. **Launch 9's guaranteed products are independent of the stop:** first console evidence for r11+D2+configured roots, plus the ridealong records — the **IGW-1 verdict** {W_WORKS / W_FAILS_R_WORKS / BOTH_FAIL} that decides the exec approach, fc1's 11 data-side FName-pool checks, and observer/qdrv1 session data. Those records + the GMalloc derivation (in flight) are the inputs for launch 10.

**User effort for launch 9:** launch Townfall with a loaded save; wait ~1 min for records. No OPTIONS/F7 this launch — the menu path stays unproven until a launch gets past the gate.

## The arming-rule decision (made, not open)

**Shape A adopted as the compiled default** — all four REQUIRED slots must be seeded or ScanGame refuses by name before anything exec-side happens. Shape B (bind {0,2,4} and continue) was built and then WITHDRAWN after the launch-9 safety analysis, because it would walk straight into the exec READ/WRITE before the honest GMalloc throw could fire. A same-launch **verdict-gating extension is designed but NOT armed** (`kExecWritabilityVerdictGating=false`): once the ridealongs owner pins the IGW verdict-file path + schema and the probe's ini line is ordered first, a bounded read at ScanGame entry would let {X→RX ok} open the vtable walks and {R→RW ok} open a vtable-swap tick backend; absent/unreadable stays Shape A. Landing a derived GMalloc constant later is a pure table update (regenerate + re-hash + recompose; no merged-unit change).

## The GMalloc launch-10 item

Slot 3 is the structural blocker for ANY launch to reach engine-ready. worker-gmalloc-derivation is in flight (stage-1 artifacts on disk; no report yet). If it derives a validator-backed constant: regenerate SeedTable.h, re-pin the receipt, recompose — that launch then inherits FLAG-2 (vtable READ) and FLAG-1 (tick WRITE) as its open verdicts, ideally with the IGW verdict already in hand. If it cannot derive one, the honest answer is that PostInitialize's GMalloc gate needs a different remedy (data-side derivation from a live session, or an initializer change) — the seed table fails loudly rather than guessing.

## The probe-verdict gating design (one probe run, two decisions)

IGW-1 rides launch 9 as a `=2` ploader.ini plugin: 12 write-through records per attempt, exactly one `sceKernelMprotect` syscall site, restore-always, budget 8 (6 used: X→RX far page → restore → X→RWX + 1-byte write/readback → restore, plus the vtable-route's D-1 pair R→RW on a RELRO data page → restore). Armed PRX `48949316…` (recomputed on disk), determinism 4/4. Its verdict selects: exec-route port as designed / vtable-swap architecture (hedge fully pre-designed — with the corrected premise that vtables live in READ-ONLY RELRO, so swaps need their own R→RW data-page mprotect) / data-side-only tier (R3 zero-syscall UFunction::SetFuncPtr, build-out in flight). Honest risk recorded: a ring-0-fatal syscall cannot be excluded (igf5 history).

## What else landed since the last digest (~28 ledger entries, ~35 worker dirs)

- **E2E chain closed out host-side**: promotion 51→54 APPLIED (3-line diff verified, G1 replay byte-exact, EX_CONFIG 6 amended); composer splice DONE (caught a receipt-format drift that would have silently re-diverged; rebuild byte-identical); D1 canonical merged chain rebuilt; RUN-CONFIG B fully scripted + host-executed (console run still pending, test #4).
- **Bridge r12 final review GO — nothing blocks** (anchor table independently re-verified against the retained image; P2-1 + 5 P3 diffs). r13 polish applied as a file but its receipts dir is still EMPTY — comp6 ships r12.
- **Required-slots question CLOSED**: ps_scan is structurally dead on this target (5 stacked gates, one measured) — this is WHY the seed route exists.
- **Seed table final**: GUA `0x97ba110` (live-validated), FName ctor `0x63d590` (3-way consensus, in-game unverified), SCO `0x890aa0` (in-game unverified), GMalloc honestly unseeded. Tick corrected `0x37ac8c0`→`0x33ac8c0` (93 stale occurrences swept; live tools fixed — verified zero wrong values remain in the batch tool).
- **Settings v3 is canonical**: 1448 B `d8faa4ec` + loadlist 447 B `4d98ecd7` (both recomputed on disk); both `[Hooks]` keys REQUIRED (OR-gated derivation); supersedes v2 (834 B) — the master plan still cites v2 in its restage step, use v3.
- **Evidence machinery hardened**: two false-PASS gates PROVEN and closed (stale rung evidence now exits 5 INCONCLUSIVE-STALE; pad verdict can't green-light after a no-run launch); console_batch.py at selftest 271/0 with staging for all ridealongs built in; rehearsal 5/5 distinguishes all five failure shapes.
- **Comp6 preflight GO_WITH_FIXES, all applied** (entry r4 adopted, r12 packaging-compile PASS, 1328/1328 pins 0 drift); manifest-finalizer fail-closed tool ready for post-build sha fills; ODR enforced mechanically (16-check emitter gate, non-vacuous).
- **App track**: download-runtime gap closed host-side (live-smoked, 42/42); UI flows host-tested; still user-gated: real client_id, live-cookie session, console consumption.
- **Fixes that caught real mistakes**: cycle-runner no-args guard (a `--help` had run the pipeline), dry1 mixed-vintage PRXs quarantined DO-NOT-LAUNCH, 8fam memory oracle corrected (114688→163840), launch-6 ledger gap closed retroactively.

## Pending console tests (all still pending; nothing was launched)

1. **Launch 9** (comp6 + ridealongs) — expected stop at the seed gate, per above.
2. dll8 Static-TLS payload run (game closed; batch tool, order dll8→e2e→stage-ridealongs→game-evidence).
3. E2E MODE B console run (scripted; promotion applied; possibly sign-in prereqs).
4. Pad hardware probe (standalone isolation).
5. Rung-3 igf7 combined probe (optional). FName exec-side probe HELD (and record-corrected: it was never wired; data-side fc1 validator rides launch 9).
6. **Launch 10** (contingent on IGW verdict + GMalloc derivation; launch10-matrix CLAIM delivered but the matrix itself is not yet written).

## Open questions (short list)

- GMalloc derivability (worker in flight) — the only REQUIRED unseeded slot.
- Exec-writability X→any transitions: NOTHING proven (evidence reset); launch 9's IGW records are the first real answer.
- Verdict-gating activation: needs the ridealongs-pinned verdict file path + schema (designed, constant false).
- Launch-10 matrix: PENDING (owner delivered CLAIM only); r3 zero-syscall tier build-out in flight.
- In-flight, not delivered: gmalloc REPORT, launch10-MATRIX.md, r13 receipts, desk-rehearsal runs (empty), r3 REPORT.

## Standing honesty notes

No >8 quality claims anywhere; Item Spawner activation has never been observed in-game; the launch-9 statement above is a prediction with named failure signatures, not a promise; all PASS numbers are quoted from worker receipts — this refresh re-verified only on-disk hashes (13 artifacts) and file presence, never the console.
