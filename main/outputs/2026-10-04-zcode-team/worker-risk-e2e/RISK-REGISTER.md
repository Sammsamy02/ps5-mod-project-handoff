# RISK-REGISTER.md — worker-risk-e2e (team 2026-10-04)

Open-risks register compiled from every worker `REPORT.md` (gaps/limits
sections of all 59 delivered reports) plus the checkpoint unresolved lists
(worker-mutex-fix-review, worker-v3-startup-diag outcome matrix,
worker-e2e-console-staging GO-NOGO G0–G6, worker-e2e-rehearsal F-R1..F-R3).
This register is the refill-backlog source: each row is independently
actionable worker-sized work unless noted controller-owned.

Evidence fields cite the owning worker's REPORT plus, where I re-verified
(sha256 recomputation / anchor-occurrence counting this session), I say so.
Likelihood/impact are qualitative engineering judgments, not measurements.

## A. Mandated risks (task-named)

### R-RW-01 — Rosetta unkillable-wedge amplifier
- **area**: host tooling / Rosetta x86_64 emulation (macOS arm64)
- **risk**: any fail-stop `abort()` in this composed flow wedges as an
  uninterruptible `UE` state: survives `kill -9` for many minutes, freezes
  concurrently running threads mid-`malloc`/`fprintf`, blocks SIGALRM
  delivery (alarm guards never fire), hangs `lldb` attach. It amplifies any
  bug that ends in abort into an operator-visible machine wedge.
- **evidence**: worker-mutex-deadlock REPORT §2 ("Amplifier" paragraph);
  restated as still-open in worker-mutex-fix-review §4 item 2 and
  worker-mutex-fix-2 §5 residual risk. Trigger chain observed:
  unbounded cleanup retry -> harness sleep-budget abort -> wedge.
- **likelihood**: medium (every debug iteration that hits a fail-stop path)
- **impact**: high (machine-level; kills unrelated automation; loses evidence)
- **mitigation**: convert host-only abort() paths to fail-code returns;
  keep every reproduction watchdog-bounded (shell watchdog + alarm, as
  mutex-fix-2 did); never run unbounded instrumented abort loops; keep
  heavy processes serialized (AGENTS.md condition).
- **blocking_what**: any future hang/fault debugging of the Windows-candidate
  host drivers; operator machine availability during batch runs.

### R-DRV-01 — driver unbounded cleanup retry (NOT fixed — verified)
- **area**: sdk_driver / owned-runtime teardown (`sdk_driver.c:45`
  `while(mb_owned_ps5_cleanup()) sleep(1);`)
- **risk**: any permanently failing cleanup stage (e.g. stage-40 mutex BUSY)
  spins the driver forever -> 15-minute watchdog kill, no phase-5 RETIRED,
  no honest terminal receipt. The retry loop itself is unchanged in the real
  tree; only the fixture causes that fed it have proposed (unapplied) fixes.
- **evidence**: worker-mutex-deadlock §2/§4; worker-mutex-fix-2 §2 cascade
  (stage-40 BUSY from unretired tokens; -68 fixture bug at
  worker-mutex.h:65; MFR-2 early-return leak) and §5: "sdk_driver.c's
  unbounded cleanup retry remains a liveness hazard for any NEW permanently
  failing stage". Verify result this session: the two fix diffs
  (`worker-mutex.h.diff`, `worker-fixture.c.diff`) remain proposals under
  worker-mutex-fix-2/; real files untouched; real `build.py` rebuild is
  still the controller's acceptance gate.
- **likelihood**: medium (any new permanently failing stage reintroduces it)
- **impact**: high for automation throughput (every hit costs a 15-min hang
  + wedge risk via R-RW-01); medium on target (console-side driver has no
  watchdog bound documented).
- **mitigation**: (1) apply mutex-fix-2's diffs and rerun
  `build.py --host-only --entry --clock --workers --sync --init-once
  --mutex`, assert phases [1,2,3,4,5] + all-zero first_failure;
  (2) independently bound the retry loop (max attempts -> loud fail);
  (3) keep the fixture token-retirement invariant asserted in-suite.
- **blocking_what**: windows-candidate profile acceptance; any further
  mutex/workers integration debugging; worker-fileio-prep's pending joint
  full-green receipt (R-SER-01).

### R-E2E-FR3 — F-R3: console link is not bit-reproducible
- **area**: E2E composition / console payload provenance
- **risk**: rebuilding the console payload from identical inputs yields a
  different sha256 (host binary rebuilds are bit-identical). Any sha-pinned
  staging manifest can therefore mismatch a legitimate rebuild, inviting
  either false NO-GO or (worse) staging a stale payload that "verifies"
  against a re-pinned-but-stale sha.
- **evidence**: worker-e2e-rehearsal REPORT §3 F-R3 (rebuild 664400 B /
  116 imports, sha `fb59d0c9…` vs pinned `3860cd56…`). This session:
  `worker-e2e-final/composition/build/console/modbridge-installer.payload.elf`
  currently hashes `3860cd561645cb55…` — the original pin, size 664400.
- **likelihood**: certain (observed both ways)
- **impact**: medium (provenance ambiguity on every console staging)
- **mitigation**: pin size + import count + import-name set as the primary
  identity (664400 B / 116 / missing=[]), treat sha as secondary; after any
  relink, re-pin deliberately with a receipt; never "re-pin to match" a
  stale artifact. Same rule applies to the current bridge payload
  (`cfa96ab5…`, 681632 B, 116 imports) when it is relinked for D1 (R-D1-01).
- **blocking_what**: console E2E staging integrity checks
  (worker-e2e-console-staging STAGING-MANIFEST / GO-NOGO G0).

### R-PAD-01 — pad provider never ran on hardware
- **area**: input family (`native_ps5_pad_provider`)
- **risk**: first real ScePad open/read happens during the VISIBILITY-TEST
  console attempt; any hardware-level mismatch (open flags, read loop,
  button semantics) surfaces there unmediated.
- **evidence**: worker-real-input REPORT "Gaps / blockers":
  `hardware_runtime_validated=false`; PadEdgeSource is host-validated only,
  integration is a proposed diff (`vendored/PROPOSED-BACKEND-DIFF.md`),
  not applied.
- **likelihood**: n/a (unvalidated by construction — open question, not a
  predicted failure)
- **impact**: medium (gates the input rung; downstream C1/C2/D1–D5 wait on it)
- **mitigation**: run the VISIBILITY-TEST as designed (bounded, records on
  failure, no silent fallback); do not compose the pad integration diff
  until the first hardware read proves the provider.
- **blocking_what**: rung-5 input acceptance; VISIBILITY-TEST console attempt.

### R-ARGV-01 — argv probe crashed the console (root cause unknown)
- **area**: console dispatch route (pldmgr `/loadpayload`) / G1 gate
- **risk**: a console dispatch of the argv probe ended in a console crash;
  no workspace receipt or log records the event, so route-vs-payload-vs-
  environment attribution is unresolved. G1 (argv forwarding) is therefore
  both unproven AND suspected of destabilizing the console.
- **evidence**: controller-reported (task brief); workspace artifacts:
  worker-argv-probe built ARGV_ECHO.elf sha256 `53075bd7cadbebbd…`
  (111,016 B, deterministic, forbidden loader imports absent) with
  PROBE-PLAN ≤3 dispatches — no dispatch receipt exists under
  `outputs/2026-10-04-zcode-controller/console/` (latest is the 17:05Z
  INITONCE_DLL_TEST pass `1a6b37b7…`). The crash is real per the
  controller; its mechanism is unrecorded here.
- **likelihood**: occurred once; recurrence unknown
- **impact**: high (console availability; all subsequent probes staged
  behind it; G1 verdict blocked)
- **mitigation**: do not re-dispatch ARGV_ECHO until the controller
  reconciles the crash (which payload sha, which route, console log tail);
  if dispatch is retried, use the PROBE-PLAN interpretation matrix and the
  designed fallback (config-file-driven entry via FTP-STOR'd
  `/data/modbridge/intake/e2e-cmd.json`, bare dispatch) so G1 failure no
  longer blocks the E2E even if forwarding is never proven.
- **blocking_what**: GO-NOGO gate G1 (argv dispatch); E2E entry-mode choice.

### R-V3-01 — v3 in-game probe stopped after VMMAP (cause unknown)
- **area**: in-game facts probe lineage (igf3 / igd ladder)
- **risk**: the v3 probe ran to `v1_vmmap_raw` chunk 7/8 then stopped;
  no records from later experiments; the three surviving hypotheses
  (module_start never ran for later stages / `sceKernelOpen` failed -22 /
  first write failed silently) are unconfirmed. The layout-promotion ladder
  (L7 base, GUA validation) is blocked with no diagnosed cause.
- **evidence**: `outputs/2026-10-04-zcode-controller/console/
  ingame-v3-full-results.jsonl` ends at v1_vmmap_raw chunk 7/8 followed by
  the igd1 minimal diagnostic's `seq=1 entry` record (outcome pending);
  worker-v3-startup-diag §3 (v2↔v3 diff: loader-visible structure
  identical, only size-shaped deltas: .text 22→40 KB, .bss 4.2→5.6 MB,
  +1 relative reloc) and §4 outcome matrix; worker-ingame-probe-v3
  freeze facts (PRX `2555b946…`, inner `c52e582d…`).
- **likelihood**: occurred; recurrence unknown pending DIAG matrix
- **impact**: high (blocks rung-2→5 staging: no promoted layout, no
  validated GUA, no predicates on real data)
- **mitigation**: run STAGE-PLAN-DIAG to decision (record
  appears/absent/malformed/crash); build igd2–igd4 bisection ladder ONLY on
  matrix outcome 1; keep one frozen variant per launch (no unchanged-
  diagnostic repeats).
- **blocking_what**: layout promotion (worker-2/worker-5/worker-layout-*),
  worker-real-input C5/C1/C2, FName P2 — the whole in-game ladder.

### R-FTP-01 — FTP readback artifact caused diagnosis delay
- **area**: console staging channel (ftpsrv readback / payload freshness)
- **risk**: the FTP readback of a staged fself is the segment-covered
  prefix of the inner ELF (observed size law: readback = bare-sdk − 1560
  bytes across v1/v3/igd1; SHT tail stripped, e_shnum zeroed), so
  exact-size/exact-hash oracle expectations are wrong by construction and
  any mismatch invites a wrong "staging failed / stale file" diagnosis —
  which is exactly what delayed the v3 zero-records diagnosis.
- **evidence**: worker-v3-startup-diag §2 (108216 vs 109776 oracle
  contradiction; "STAGE-PLAN-V3's exact-size/exact-hash oracle … was
  wrong"; staging exonerated); exact ftpsrv reassembly transform not
  derivable from the workspace — size law + ELF magic is the sufficient
  check. Launch-ritual freshness oracle (runtime aligned-log mtime) per
  ZCODE-FORK-HANDOFF LAUNCH RITUAL.
- **likelihood**: certain-by-construction on every fself staging
- **impact**: low-medium (wasted dispatches/misdirected debugging; bounded
  now that the law is documented)
- **mitigation**: always verify staged artifacts with the size law + magic,
  never full-hash; use log mtime (not file readback) as the freshness
  oracle; treat "readback mismatch" as a channel property, not a staging
  failure, unless size-law fails too.
- **blocking_what**: interpretation of every console staging/probe receipt
  (incl. the igd ladder and the D1-relinked payload in R-D1-01).

### R-PLD-01 — ploader cannot see module_start's return value
- **area**: injection/launch chain (ploader -> sceKernelLoadStartModule)
- **risk**: ploader's only success signal is LoadStartModule returning a
  positive module id (`int ok = (rc > 0)`, ploader main.c:810);
  module_start's own return never reaches ploader, and whether the kernel
  folds a nonzero entry return into LoadStartModule's result is not
  determinable from the retained source. "OK (~1207ms)" is therefore NOT
  evidence that a payload's entry ran — a silent entry failure looks
  identical to success.
- **evidence**: worker-v3-startup-diag §1 (retained ploader source read in
  full context); consistent with R-V3-01's "OK + untouched log" evidence.
- **likelihood**: structural (always true on this route)
- **impact**: medium (masks entry-level failures; inflates false GO
  verdicts at launch time)
- **mitigation**: treat the payload's own /data record stream as the only
  acceptance (entry record first, flushed pre-records); the igd1
  always-return-0 diagnostic removes the return-propagation variable;
  never claim "started" from ploader OK alone.
- **blocking_what**: honest verdicts for every console launch (v3/igd,
  INITONCE lineage already follows the record-first discipline).

### R-D1-01 — D1 journal-order fix still needs E2E/console propagation
- **area**: installer core commit sequence (modinst_core.c; install +
  uninstall + recovery)
- **risk**: baseline deletes the journal BEFORE committing records at three
  commit sites; a death in that window leaves journal durably gone + records
  stale — an unrecoverable "records lie" state (records claim installed
  while staged files are gone; next transaction rebuilds from the lie).
  The fix exists ONLY in worker-uninstall-fault-expansion's lineage; the
  currently staged console payload and every other chain's binaries are
  pre-D1. A console crash mid-commit during the E2E run would poison
  `/data/modbridge` state irrecoverably (short of manual records surgery).
- **evidence**: worker-uninstall-fault-expansion REPORT §2 (demonstrated on
  baseline `receipts/d1-window-demo.log`; fixed by tuples `uf-order-commit-a`,
  `uf-order-commit-b`, `uf-recover-forward`, `uf-recover-commit`; patched
  binary `46d87345…`, baseline byte-identical to the zip-to-mfb pin
  `947affc8…` — provenance verified). This session's verification: current
  sha256 of all chain artifacts and anchor occurrence counts per chain
  (0 conflicts except the r2 interplay) — full per-chain map and exact
  application steps in **D1-PROPAGATION.md** (same directory).
- **likelihood**: certain (fix exists, not yet propagated); crash-window
  hit probability during E2E is low but consequence is unrecoverable
- **impact**: high (state store integrity; E2E stages 3/6 trustworthiness)
- **mitigation**: before the console E2E link, append the 4 uf tuples to
  the chosen chain (recommended: the zip-bridge-console-link chain), relink
  the console payload, rerun the 186-case fault suite + bridge regression,
  re-pin the new payload sha (with R-E2E-FR3's size/import identity rule).
- **blocking_what**: console E2E run (install/uninstall/clean-state stages);
  any console-state longevity beyond a single session.

## B. Supplementary open risks (refill backlog)

### R-R2C-01 — F-R2 fix exists only in the f-r2-fix host lineage; conflicts with the uf chain order
- **area**: E2E transcript honesty (e2e-status.jsonl begin/end pairing)
- **risk**: the staged console payload still dangles a `begin` line on
  install refusal (F-R2). The fix (2 r2 tuples) lives only in the 47-tuple
  f-r2-fix chain, which has DIVERGED from the 49-tuple uf chain. Naively
  appending uf tuples to the r2 core fails: `uf-order-commit-b`'s anchor has
  0 occurrences there (verified this session) because `r2-install-status-ok`
  inserts a line after the shared `say("installed …")`. Verified fix: apply
  uf BEFORE r2 (then the r2 anchor still matches exactly once).
- **evidence**: worker-f-r2-fix REPORT §1/§3; my anchor-count verification
  (details + merge order in D1-PROPAGATION.md §4).
- **likelihood**: certain without the merge; low after
- **impact**: medium (transcript misread as dead commands during E2E)
- **mitigation**: adopt the canonical 51-tuple merge order; keep the
  fr2-a/fr2-b suite checks in every relink regression.
- **blocking_what**: E2E transcript pairing gates; D1 propagation relink.

### R-G1-02 — argv forwarding unproven (independent of the crash)
- **area**: E2E entry mode
- **risk**: no workspace record evidences pldmgr forwarding arguments; the
  installer is argv-driven (modinst.c:1480-1522). Until G1 resolves, the
  console E2E has no proven way to pass per-stage verbs.
- **evidence**: worker-e2e-console-staging REPORT §4 finding 3 (hard gate
  G1 + bounded probe spec); worker-argv-probe PROBE-PLAN matrix.
- **likelihood**: open (50/50 until the probe runs)
- **impact**: high (blocks stage commands) — mitigated by the designed
  config-file fallback
- **mitigation**: config-file-driven entry (STOR + sha-verify + bare
  dispatch); keep ARGV_ECHO for later attribution only (see R-ARGV-01).
- **blocking_what**: E2E stages 1–6 command entry.

### R-G3-01 — FTP STOR into `/data/modbridge/intake/` never evidenced
- **area**: console E2E stage 0/2a intake placement
- **risk**: all prior console traffic is payload-dir HTTP upload + FTP
  reads; intake-dir write permission/semantics are unproven.
- **evidence**: worker-e2e-console-staging REPORT §4 finding 4 (gate G3,
  bounded probe specified).
- **likelihood**: open
- **impact**: medium (stage 2a blocked; owner-placed-zip path also needs it)
- **mitigation**: run the specified bounded STOR probe first; no silent
  fallback routes.
- **blocking_what**: E2E stage 2a; download-intake path on console.

### R-DL-01 — console download stage remains host-linked only
- **area**: E2E stage 1 (download)
- **risk**: the console payload has no download command (import set
  identical to baseline proves it); stage 1 needs the ps5io-rdwr native
  download closure merged OR archives staged via the owner-placed-zip path.
- **evidence**: worker-zip-bridge-console-link REPORT §4 item 2;
  worker-e2e-console-staging (stage-1 DEFERRED; probe A shows
  register-mod accepts a raw zip without a sidecar).
- **likelihood**: certain until merged
- **impact**: medium (stage 1 unavailable; mitigated path exists)
- **mitigation**: use the owner-placed-zip probe-A path for the first E2E;
  merge the ps5io-rdwr closure as a separate queued integration.
- **blocking_what**: E2E stage 1.

### R-ACK-01 — real runtime ack absent on console (running=1 stays SIMULATED)
- **area**: activation/ack (mfa_status_* emissions)
- **risk**: the injected runtime emits no ack lines; stage 4-5 can never
  honestly report running=1; all current running=1 evidence is the host
  simulate-runtime file-format proof.
- **evidence**: worker-installer-composition REPORT §6 item 2;
  worker-zip-bridge-console-link §4 item 3; worker-e2e-console-staging §4
  item 5; worker-ack-integration-deep honest limits (composition-2 object
  remains pre-ack by design; build_runtime.py mechanism is a proposal).
- **likelihood**: certain until implemented
- **impact**: medium (E2E stage 5 verdict is honestly SIMULATED, not real)
- **mitigation**: implement the five module_start emissions in the runtime
  composition (controller-owned step); label running=1 SIMULATED until then.
- **blocking_what**: E2E stage 5 real acceptance; worker-7 caps honesty.

### R-GT-01 — game-thread delivery gate observed failing in-game
- **area**: UE4SS mod runtime (EngineTick seam)
- **risk**: `ExecuteInGameThread` nil at main.lua:695 — the single hard
  gate for any in-game mod behavior; bridge preserves refusal semantics,
  does not implement delivery.
- **evidence**: worker-real-input REPORT "Gaps / blockers" (A9);
  worker-itemspawner-dryrun BLOCKERS.md B0.
- **likelihood**: certain until delivered
- **impact**: high for the mod-visible goal (one representative in-game
  success per family)
- **mitigation**: land worker-3's EngineTick seam with validated layout
  facts (depends on R-V3-01 ladder).
- **blocking_what**: rung-5; all Lua-mod in-game behavior claims.

### R-IDENT-01 — identity/retention facts unresolved in-game
- **area**: live module facts (worker-1 experiments 1–7)
- **risk**: DT_DEBUG population, RTLD_NOLOAD retainability, exec-module-
  handle signature, module-list size unit, VMMAP permission, loader-vs-
  kernel handle domain equality, fw 13.60 profile values — all unproven;
  `KERN_PROC_PATHNAME` unobserved in-game; `text_base` unresolved end-to-end;
  module info stays `NotBound` (`allow_loader_handle_as_module_handle`
  false); scan-fed conversions stay provenance-tagged, never full facts.
  Note: liveprobe E1 intentionally dereferences DT_DEBUG-derived pointers
  and may fault mid-run (bounded JSONL before the fault stays valid).
- **evidence**: worker-1-live-supplier unresolved list; worker-s1-adapter
  target-only facts; worker-supplier-ingame-update unresolved list;
  worker-composition-prep limits; worker-liveprobe honest boundary.
- **likelihood**: open (experiments specified, not run)
- **impact**: high for identity-gated consumers (adapter refuses empty
  executable_path on target today)
- **mitigation**: run the 7 experiments as controller-owned standalone
  probes, one JSONL record each; keep refusal semantics until each is green.
- **blocking_what**: s1 adapter target conversion; staged-layout identity
  gates; UE4SS activation views.

### R-LAYOUT-01 — layout predicates remain synthetic-host evidence only
- **area**: layout validation/promotion
- **risk**: predicates + width gates prove logic on synthetic images;
  ACCEPTED.md stays empty until they run in-process on the real mapped
  snapshot; 8 width-blocked members wait on a widths-3 campaign; UFunction
  has no validated predicate by discipline; FName width (P2) unresolved.
- **evidence**: worker-layout-batch3 §5; worker-5-layout-validator honest
  status; worker-2-layout-facts unresolved list; worker-fname-construction
  §2.
- **likelihood**: certain until the ladder runs (gated by R-V3-01)
- **impact**: high for rung-3+ (callable ABI claims stay absent)
- **mitigation**: keep the RUNBOOK path; never promote from synthetic
  evidence; keep candidate != validated separation.
- **blocking_what**: rung-3→5 staging; FName-dependent marshalling.

### R-LIBC-01 — console libc dirent route + fsync durability unproven on target
- **area**: installer composition (rm_rf / journal enumeration / rmdir)
- **risk**: the chosen opendir/readdir/rmdir binding is nm-verified at link
  time only; whether the console runtime's libc serves it usefully, and
  fsync/fsync_dir/O_DIRECTORY semantics on the console FS, are answered
  only by the target run (E2E stage-3 rollback probes exercise it).
- **evidence**: worker-installer-composition §3/§6.5; worker-installer-ps5io
  honest limits (fsync/O_DIRECTORY/SceKernelStat/pread-equivalence
  unproven on target).
- **likelihood**: open
- **impact**: high if broken (uninstall/recovery enumeration is D1's
  repair path — a broken enumeration compounds R-D1-01)
- **mitigation**: E2E stage-3 rollback probes run before any long-lived
  console state is trusted; keep recovery fail-loud (`unknown journal
  phase` behavior pinned).
- **blocking_what**: console uninstall/recovery stages.

### R-PRX-01 — PRX candidate is static-verification only
- **area**: PRX packaging/link
- **risk**: composition-2 candidate passes all static checks (NID-decoded
  196-import contract, alignment prologues) but `promotion_allowed:false`;
  no in-game load/promotion evidence; worker-prx-packaging's own `--link`
  step was budget-cut (superseded by prx-link-gaps' composition-2 build).
- **evidence**: worker-prx-link-gaps §5 limits; worker-prx-packaging honest
  status.
- **likelihood**: certain (by scope)
- **impact**: medium (the PRX route stays unproven vs the ELF /loadpayload
  route)
- **mitigation**: keep static checks as gates, never as load claims; the
  argv-probe decision (ELF route) already de-risks the E2E path.
- **blocking_what**: any future PRX-route dispatch decision.

### R-CRT-01 — CRT census gap blocks startup-parity wiring
- **area**: CRT startup parity (worker-crt-startup-design)
- **risk**: `worker-dll-mod-retention/baseline-dll-census.json` absent from
  this workspace; DESIGN.md 2.2 is a PENDING table; xp→xt→_cexit ordering
  flagged for re-verification against vendored CRT sources; owners named
  for composition not found as source (marked not-integrated).
- **evidence**: worker-crt-startup-design honest boundaries + "Unblocked
  by" list.
- **likelihood**: certain until delivered
- **impact**: medium (blocks DLL-startup parity claims; no current consumer
  on the critical E2E path)
- **mitigation**: controller delivers census + entry-adapter interface;
  then wiring-list items 1–10 in order.
- **blocking_what**: CRT parity acceptance (queued, not E2E-critical).

### R-LOG-01 — in-game log parser rules unvalidated against real bytes
- **area**: evidence collection tooling
- **risk**: repair rules derived from the documented malformation class,
  not real log bytes (the two real console logs were absent when built);
  extraction heuristics are schema guesses; v1 facts JSONL comma bug is
  real (observed lineage) but rule fidelity is unproven.
- **evidence**: worker-ingame-log-parser honest limitation (cross-cited by
  e2e-console-staging/f-r2-fix as a controller delivery gap); launch-
  automator CLAIM basis.
- **likelihood**: unknown until run on real logs
- **impact**: low-medium (mis-parsed evidence could mislead verdicts)
- **mitigation**: `python3 -m unittest discover -s tests` against the real
  logs before adopting as the standard collector; built-in comma-tolerant
  fallback in ritual.py as second channel.
- **blocking_what**: launch-automator verdict parsing.

### R-SER-01 — joint fileio+mutex composition full-green receipt pending (environment serialization)
- **area**: windows-candidate host verification
- **risk**: the joint compose run compiled+linked and reached PE execution
  but stalled while another worker's identical driver ran concurrently;
  worker stopped rather than kill anything. Rerun needed when no other host
  driver is active (command line preserved in the report).
- **evidence**: worker-fileio-prep selftest status (prior fileio-only pass
  `a17f8325…` stands; joint run `63a5e1fa…` incomplete).
- **likelihood**: certain until rerun
- **impact**: low-medium (single profile's full-green receipt)
- **mitigation**: rerun the preserved command on an idle Mac; keep the
  serialize-heavy-processes rule.
- **blocking_what**: fileio+mutex joint acceptance.

### R-BOUND-01 — bridge v1 bounds refuse large archives
- **area**: zip→mfb bridge
- **risk**: ≤256 members / ≤64 MiB member bytes / ≤1 MiB manifest — larger
  real corpora (e.g. leon-2k 100 MB texture paks) are refused honestly; a
  streaming variant is future work. Identity defaults are conventions, not
  extracted metadata (supplier .mfb path remains preferable).
- **evidence**: worker-zip-to-mfb §4; worker-zip-bridge-console-link §4.4.
- **likelihood**: certain for large mods
- **impact**: low-medium (honest refusal, no corruption)
- **mitigation**: document bounds at refusal; commission the streaming
  variant only if the corpus demands it.
- **blocking_what**: large-mod support (post-E2E).

### R-CONTC-01 — container format coverage gaps
- **area**: container intake (worker-6)
- **risk**: Zen TOC field semantics/signing unknown; 4699-vs-4544 delta
  unresolved; Oodle presence in the two complete pairs unchecked; encrypted
  ZIP/PAK fixtures absent; RAR one-off; bsa/ba2 and 7z pure gaps.
- **evidence**: worker-6-containers open questions; worker-container-headers
  classification specs.
- **likelihood**: certain (unexplored)
- **impact**: low for the current E2E path (zip corpus proven)
- **mitigation**: backlog; do not guess byte semantics.
- **blocking_what**: non-zip archive intake (queued).

### R-ACT-01 — actions layer is a host transport; payload binding open
- **area**: app backend (worker-8)
- **risk**: action driver uses POSIX fork/exec — impossible in the console
  payload process model; binding `mfw8_actions` to the payload launch path
  is the named controller integration step; `installation_supported:false`
  remains true until a console E2E proves otherwise; jobs synchronous;
  cancel is a documented no-op for terminal jobs.
- **evidence**: worker-8-app-backend §5.
- **likelihood**: certain (by design)
- **impact**: low for E2E; medium for product claims
- **mitigation**: keep caps default false; queue the payload-binding task.
- **blocking_what**: app-level install actions on console.

### R-MUT-01 — CreateMutexA census slot unbound (policy decision owed)
- **area**: windows-services mutex family
- **risk**: the observed UE4SS import is `CreateMutexA`; the delivered
  family binds only the W variants; an A→W shim needs a CP_ACP decision
  (controller decision required).
- **evidence**: worker-mutex census honesty section.
- **likelihood**: certain until decided
- **impact**: low-medium (an A-name caller would hit an unbound slot)
- **mitigation**: controller policy decision (CP_ACP mapping) then shim
  with its own evidence.
- **blocking_what**: full mutex-family parity.

## Reading order for refill dispatch
1. R-D1-01 + R-R2C-01 (one merged relink task; steps in D1-PROPAGATION.md)
2. R-V3-01 (STAGE-PLAN-DIAG decision) — unblocks the whole in-game ladder
3. R-ARGV-01/R-G1-02 + R-G3-01 (E2E entry + intake gates)
4. R-DRV-01 + R-SER-01 (host acceptance reruns)
5. R-ACK-01, R-DL-01 (E2E stage completion)
6. R-IDENT-01 experiments, R-LAYOUT-01 ladder (post-DIAG)
7. R-LOG-01, R-PAD-01 (evidence/input tooling)
8. R-CRT-01, R-PRX-01, R-MUT-01, R-BOUND-01, R-CONTC-01, R-ACT-01 (queued)

Standing channel risks that bound ALL console work: R-E2E-FR3 (sha
identity), R-FTP-01 (readback law), R-PLD-01 (record-stream truth),
R-RW-01 (host-side wedge amplifier).
