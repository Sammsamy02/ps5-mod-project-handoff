# STAGE-TABLE — r13/r14 composition-startup marker → stage classification

Worker: worker-launch10-evidence-upgrade · 2026-10-04 (host clock 2026-10-05) ·
host-side source reading ONLY; no console contact; nothing executed against the
console. This table is the provenance backing for the stage classification in
`evidence.sh` (this worker directory). Every marker below was read back from
the pinned sources listed in the Source column; nothing is invented.

## 0. Scope and config truth

Launch-10 runs the SHORTEST 2-plugin config (`ploader.ini` sha
`1216aab6…eda0`): ONLY `modbridge_supplier_composition.prx = 2` and
`modbridge_path_test.prx = 2`; `modbridge_runtime_aligned_entry.prx = 0` and
all probes (incl. xwp) `= 0` (RETURN-SESSION-COMMANDS rev 2026-10-05a). The
staged fself is the rebuilt supplier_composition: r14 lease-coverage fix
(worker-lease-coverage-fix/runtime_start_bridge_r14.cpp) + armed SeedTable +
GMalloc V1/V2 validator diff. Therefore:

- Every `modbridge module_start failed:` line in the UE4SS mirror now belongs
  to supplier_composition (the [t=…pid=…] r4/r7 prefix exists in BOTH
  lineages: worker-comp6-driver lineage-overlay-2 lifecycle/runtime_module_entry.c
  :184-232 and worker-module-start-wiring/runtime_module_entry_r4.c :183-232 —
  the prefix does NOT disambiguate writers; the CONFIG does).
- The aligned-entry log and the xwp jsonl must NOT grow; growth = CONFIG-DRIFT.
- The r13 code linked in the launch-9 entry chain and the r14 candidate share
  the marker vocabulary except where noted (r14 adds the L3/L4 gate tags).

## 1. Channels

| channel | path | grammar | role in launch-10 |
|---|---|---|---|
| UE4SS mirror | `/data/plugins/UE4SS.log` | text lines; PATH_TEST/ACTIVATION_TEST stamps; r4-prefixed failure lines; xwp JSON mirror records | PRIMARY (append-only) |
| runtime-status | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | `modforge-runtime-status\t1`, `session\t<pid>\t<ms>`, `state\t<seq>\t<state>\t<code>\t<detail>`; O_TRUNC per session (last-writer-wins) | PRIMARY (session truth) |
| aligned-entry log | `/data/townfall_runtime_aligned_entry.log` | `[PPSA33286] modbridge-runtime-aligned-entry/v1 build=<hex> <tag> value=0x<hex>` | HISTORY only (entry disabled; growth = drift) |
| xwp jsonl | `/data/modbridge-exec-write-probe.jsonl` | `{"marker":"modbridge.execwriteprobe",…}` | drift guard only (xwp disabled) |
| supplier-diag jsonl | `/data/modbridge-supplier-diag.jsonl` | `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,"tag":"…","value":D,"value_hex":"0x…","t_ms":MS}` (worker-diagnostic-sink staging/diagnostic.c) | OPTIONAL: full tag stream + timing if the sink landed |

## 2. The stage table

Stages in program order (runtime_start_bridge_r13/r14.cpp
`modbridge_runtime_start_with_activation`; comp6 lineage entry
`module_start`/`start_composed_runtime`). "Success markers" = durable evidence
the stage completed; "failure markers" = the loud terminal evidence if it
failed there. `S11` repeats across the process lifetime.

| stage | entered after | success markers (channel) | failure markers (channel) | expected terminal state string (status) |
|---|---|---|---|---|
| **S0_DIAGCHANNEL** diag channel open | module_start entry guard | (silent on success; subsequent tags prove it) | failure line `modbridge module_start failed: diag_channel_open_failed code=-22 detail=…` (UE4SS) — SILENT unless the channel-postmortem patch is resident; absence never excludes -22 | `failed -22 diag_channel_open_failed …` only with the patch resident |
| **S1_ENTRY_CTORS** module_start → constructors | channel open OK | `module_start_entered`, `entry_incoming_sp_mod16`, `entry_call_sp_mod16`, `constructor_count`, `constructor_begin`/`constructor_return` per ctor, `probe_lifecycle_result` (jsonl / entry-text) | `runtime_start_skipped` (ctored unit refused; no wiring) | `failed <ctor-rc>` |
| **S2_ACTIVATION** mfa_activate | lifecycle clean | `runtime_wiring_begin`, `mfa_activated_low` + `mfa_activated_high` (preloaded bytes) | `mfa_activate_failed` + `mfa_activate_os_error`; failure line `… mfa_activate_failed code=<mfa_rc> detail=<MFA_* name …>` | `failed <mfa_rc> mfa_activate_failed …` |
| **S3_VMMAP_WALK** sysctl KERN_PROC_VMMAP + per-record walk | activation adopted | `bridge_vmentry_entries` (count), `bridge_vm_walk_stop` (0 clean / 2 truncated-OK / 1 malformed=FAIL), `bridge_kve_distinct0..3`, `bridge_kve_eboot`, `bridge_exepath_valid`, `bridge_exepath_sig` (jsonl / entry-text; r13 :278-350) | query rc: -2 pid, -3 sysctl, -4 short, -5 zero entries, -6 MALFORMED (loud, r6 contract) → anchor route dies here | `failed 1 runtime_start_failed <no diag if query threw>` |
| **S4_LEASE_REG** readable-range registration | first scan op | r13: `bridge_lease_reg_spans` (readable spans in extent; expected 6 on this target) + `bridge_lease_reg` (registered; 6; 0xffffffff on query fail) — r13 :483-523. r14: same tags with FROZEN-snapshot basis, plus `bridge_lease_cap` (128 saturation), `bridge_lease_gap`, `bridge_lease_gap_req`, `bridge_lease_ref_lo/hi/len`, `bridge_lease_near_lo/hi/len` (r14 L1/L2/L3/L5) | r13: none loud (silent drift class — launch-8 root cause). r14: loud gap report → start refuses BEFORE conversion | `failed 1 runtime_start_failed lease coverage gap: requested [0x… +0x…) outside registered readable spans` (r14 only) |
| **S5_IDENTITY_ANCHORS** window-anchor identity gate | scan wrapper runs | `anchor_cov` per window, `anchor_match` (0x0001_0001-style w<<16|matched), `anchor_mm` (mismatch offset), `anchor_c_either` (1 = B&&D&&E&&(C‖C2) satisfiable). r14 adds `anchor_owned_ref` (L4 ownership gate per window) and `anchor_refuse` (r13 :355-397; r14 :428-436, :705+) | `anchor_c_either=0` or uncovered required windows → scan returns -1 → NoImageAnchor conversion refusal | `failed 1 … no image anchor (status NoImageAnchor); refusing conversion; dlopen(NULL) is never a fallback` |
| **S6_CONVERSION** TryMakeNativeLoaderFactsFromSupplier | capture done | NO positive tag on success in r13 — success is visible only via S7 tags. Refusal diagnostics: `lmf_vmentry_cap`, `lmf_vm_scan_cap`, `lmf_linkmap_cap`, `lmf_provider_code`, `lmf_identity_source` (r13 :653-657; launch-9 observed vmentry=2 vm_scan=2 linkmap=1 provider_code=0xfffff05f identity_source=2) | `derive_readable_ranges` zero-intersection refusal (comp6-integration/lmf_to_native_loader_facts.cpp :77-109, :228-231); supplier status refusals; Resolution-B path missing | `failed 1 … no observed mapping interval intersects the module image extent with valid geometry` ← **the launch-9 terminal string**; in launch-10 config it would belong to the COMPOSITION |
| **S7_CFG_ATTACH** configured roots attach | conversion OK | `bridge_cfg_gamedir=1`, `bridge_cfg_workdir=1` (+ `bridge_exepath_source` 0/1/2 label; r13 :668-671, :412-467) | (attachment cannot fail; absence after successful conversion = pre-r12 binary) | n/a |
| **S8_CONTEXT** MakeNativeProgramContext | cfg attach | (no positive tag; success visible at S9) | path validation `Native module and executable paths must name existing regular files`; **deep PLH lease throw `Owned read range includes unregistered bytes` (code=2) — DORMANT in r13 (launch-8's r10 threw it, pid 234); r14's pre-lease gate exists precisely to keep this unreachable** | `failed 2 … <validation text / throw text>` |
| **S8B_INITIALIZER** seeds → armed scan → PostInitialize | context built; runtime init thread | armed-path `[Seeds]` progress (wording not verbatim-pinned), `[TownfallItemSpawner]` prints later | `[Seeds] Required-slot seed table incomplete … missing [gmalloc]` (Shape-A; armed table NOT shipped = launch-10 regression), `[Seeds] REFUTED: …` (terminal per launch), `GMalloc is uninitialized.` (PostInitialize gate), `Native game-thread initializer callback registration failed` (I7) | `failed 1 <the refusal line>`; Shape-A: nothing binds, game UNMODDED |
| **S9_START_ACK** StartActivatedRuntimeWithAck → owner start | prepare OK | `runtime_start_ok` tag (jsonl/entry); status `state … activated` ack | code=3 MRS_ERR_START: `runtime_start_failed` tag + failure line (ack sink may already hold the failed session — entry logs loudly WITHOUT truncating status) | ladder `activated`; else `failed 3 …` |
| **S10_LADDER** engine ack chain | runtime init thread | status states `engine-ready` → `mods-started`; `[TownfallItemSpawner]` prints (UE4SS) | frozen-at-activated = hang/crash check (F7/F8) | `mods-started` = ge.5 gate opens (user tests OPTIONS only after this) |
| **S11_STOP** module_stop / unload | any time | `module_start_return` (entry-level return; jsonl/entry), `runtime_stop_result` (stop ack) | `MRS_ERR_STOP_EXN` (5, post-r13 renumber) | `stopped` |

## 3. Known launch-9 terminal (the anchor case)

- Terminal chain (worker-launch9-diagnostics-mine F3, entry-log offsets
  @2274097-2276081): 5 data windows verified+matched at candidate base
  0x400000, window 5 uncovered, `anchor_c_either=1`, then `lmf_*` caps
  vmentry=2 vm_scan=2 linkmap=1, `lmf_provider_code=0xfffff05f` (-4001
  kRefusalExecHandleRawZeroIngame), `lmf_identity_source=2` (VmScan),
  `runtime_start_failed=0x1`; mirror line
  `[t=44372082ms pid=273] modbridge module_start failed: runtime_start_failed code=1 detail=no observed mapping interval intersects the module image extent with valid geometry`;
  status session 273 `failed 1`. Contradiction at the heart of the bug: the
  bridge's own registration query found 6 readable spans (`bridge_lease_reg_spans=0x6`,
  `bridge_lease_reg=0x6` @2270807/@2270933) minutes earlier in the same
  attempt, while the conversion snapshot derived ZERO intersecting intervals.
- `bridge_cfg_gamedir`/`bridge_cfg_workdir` are ABSENT from the whole launch-9
  delta (expected: conversion refused before the r13 :668-671 attach point).
- The lease throw is DORMANT in r13: the code=2
  `Owned read range includes unregistered bytes` wiring sequence in the shared
  log is launch-8's r10-lineage binary (no `bridge_lease_reg_spans` tag, no
  `bridge_cfg_*` tags — pre-r12; launch attribution of that mirror line is
  ambiguous, diagnostics-mine F2b). r14 replaces the deep throw with the loud
  pre-lease gate (S4/S6). Seeing the throw text in launch-10 = the rebuild did
  not carry the r14 gate → verify the readback sha first.

## 4. Timing markers (per-writer rule)

| writer | stamp | unit/domain | extract rule |
|---|---|---|---|
| path_test plugin | `PATH_TEST: /data/plugins/UE4SS.log writable at <ns> monotonic-ns` (path_test_entry.c :141-145); `… OPEN_FAILED errno=<e> at <ns> monotonic-ns` (:165-170) | CLOCK_MONOTONIC ns | deltas only vs previous same-writer stamp |
| composition/entry failure lines | `[t=<ms>ms pid=<pid>] ` (r7 prefix, both lineages) | CLOCK_MONOTONIC ms, per-pid | sequence per (writer, pid); never across pids/writers |
| supplier-diag jsonl | `"t_ms":<ms>` per event (worker-diagnostic-sink; same domain as runtime-status `t=`) | CLOCK_MONOTONIC ms | per-stage elapsed = delta vs previous event, same writer |
| runtime-status | `session\t<pid>\t<ms>` single point; state lines carry NO per-state timestamps | CLOCK_MONOTONIC ms | file sequence order is the only intra-status ordering |

GUARD (hard rule in the tool output): stamps are per-writer sequences; NEVER
subtract across writers. File-level sequence order (task 3c) is the only
cross-line ordering that is trusted, and only within one append-only file.

## 5. Sources (all read back verbatim; no console contact)

- `worker-module-start-wiring/runtime_start_bridge_r13.cpp` (markers :278-350,
  :380-396, :412-467, :483-523; stage/catch structure :547-734)
- `worker-lease-coverage-fix/runtime_start_bridge_r14.cpp` (L1-L5, new tags
  `anchor_owned_ref`, `bridge_lease_cap`, `bridge_lease_gap*`,
  `bridge_lease_ref_*`, `bridge_lease_near_*`; gap detail text "lease coverage
  gap: requested [… +0x…) outside registered readable spans"; launch-9
  attribution note in header)
- `outputs/2026-10-04-zcode-controller/comp6-integration/lmf_to_native_loader_facts.cpp`
  (derive_readable_ranges :77-109; refusal string :228-231; status_error
  strings :36-67; extent resolution :174-225)
- `worker-comp6-driver/staging/lineage-overlay-2/lifecycle/runtime_module_entry.c`
  (entry stage sequence; write_ue4ss_log_line r7 prefix :184-232;
  write_status_failed; mfa_activate path; diag -22 line :344)
- `worker-module-start-wiring/runtime_module_entry_r4.c` (r4 entry lineage,
  same failure grammar + prefix)
- `worker-minimal-path-test/src/path_test_entry.c` + `build_path_test.py`
  (PATH_TEST/ACTIVATION_TEST emission, LOG_PATH=/data/plugins/UE4SS.log)
- `worker-diagnostic-sink/staging/diagnostic.c` (supplier-diag jsonl grammar,
  SUPPLIER_DIAG_PATH=/data/modbridge-supplier-diag.jsonl)
- `worker-startup-checkpoint/diag/src/module_entry.c` (DIAG line grammar)
- `worker-launch9-diagnostics-mine/CLAIM.json` (launch-9 terminal chain,
  offsets, lease-spans=6 evidence, attribution caveats)
- `outputs/2026-10-04-zcode-controller/RETURN-SESSION-COMMANDS.md` (rev
  2026-10-05a: launch-9 result, SHORTEST config 1216aab6…, launch-10 plan)
- `worker-launch10-matrix-prep/LAUNCH10-MATRIX.md` (launch-10 candidate
  terminals: seeds refuted / GMalloc gate / D-1 / SCO hazards)
