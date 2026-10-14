# STAGE-PLAN — modbridge_ingame_facts PRX (build igf-f4a91d8123b0)

Exact controller staging steps for the in-game loader-facts experiment.
Everything below is staging ONLY. The Townfall GAME LAUNCH itself is
USER-COORDINATED — never fake a launch, and never claim in-game results
until the record file below actually exists on the console.

## Artifact (frozen for this plan)

- PRX: `outputs/2026-10-04-zcode-team/worker-ingame-probe/build-f4a91d8123b0/modbridge_ingame_facts.prx`
- SHA-256: `db8ec5505f6eb58fc26b8f6473ffbcc9ed03e86167e8aef36712acd83fdf5991`
- Bytes: 32,077
- Local checks: `build-f4a91d8123b0/artifact-checks.json` = LOCAL_PACKAGE_CHECKS_PASSED
  (12 NID-encoded strong imports incl. dlopen/dlclose; DT_INIT/DT_FINI explicit;
  init/fini arrays loader-unowned, 1 sentinel slot each; SDK 0x11000000;
  fself wrap segment-equality verified). Full command receipts:
  `build-f4a91d8123b0/build-receipt.json`.

## 1. Upload

Place the PRX at exactly:

```
/data/plugins/modbridge_ingame_facts.prx
```

Verify the readback hash equals the SHA-256 above before configuring
anything. Do not rename the file: the module name/soname inside the artifact
is `modbridge_ingame_facts`.

## 2. Configure ploader (mode 2, late)

Edit `/data/plugins/ploader.ini` and add under the Townfall section:

```ini
[PPSA33286]
/data/plugins/modbridge_ingame_facts.prx = 2
```

Rules from CONSOLE_RUNBOOK.txt section PLOADER: entry syntax
`/data/plugins/your_module.prx = 2`; mode 2 is LATE load; re-send/re-stage
per game launch lifecycle. If other entries exist for this title, keep them
unless the controller explicitly reorders — but note THIS module is designed
to run BEFORE any UE4SS module initializes; if a UE4SS entry is present at
the same mode, coordinate ordering with the controller (safest first run:
this module alone, so its records are unambiguous).

Rollback: remove the line (or restore the prior ini) and delete the PRX.

## 3. Expected record file

`/data/modbridge-ingame-facts.jsonl` — created by the module itself via the
lineage-proven sceKernelOpen/sceKernelWrite/sceClose trio
(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600). One JSONL line per experiment,
envelope:

```
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"igf-f4a91d8123b0",
 "seq":N,"exp":"g1_exec_handle"|"g2_dtdebug"|"g3_noload_eboot"|"g4_module_list"|
 "g5_vmmap"|"g6_layout_predicates"|"g7_fw_summary","rc":R,"errno":E,"fields":{...}}
```

8 records total (G1..G7 + summary inside G7). Each record <= 2 KiB;
truncation marked with `"__trunc":1`.

## 4. Launch + collection

1. Ask the USER to launch Townfall normally (ploader picks the module up late).
2. After the game is up (give it a minute), retrieve
   `/data/modbridge-ingame-facts.jsonl` over the normal console channel and
   store a copy with its SHA-256 next to the staging receipt.
3. If the file is ABSENT, the module either failed to load or failed to open
   the log (module_start returns -22 in that case; ploader's FAIL
   notification is the signal — capture its rc/notice verbatim). Absence is a
   result, not a retry trigger; report it as-is.

## 5. Interpretation map (honest outcomes)

- G1 `plausible_handle` + `size_echo_ok` -> the GAME eboot exec-handle case is
  resolved (standalone payload returned 0; controller verdict called this
  inconclusive pending exactly this probe).
- G2 `dt_debug_present` + non-zero `dt_debug_value` + `entries` -> game rtld
  populates DT_DEBUG in-process; link-map route becomes usable (payload env
  refuted it). `validation_blocked_derefs` counts refused dereferences.
- G3 `handle_nonnull` -> eboot is dlopen-visible with RTLD_NOLOAD (ownership
  usable); `dl_route` records which import route served the call.
- G4 `discriminated_unit` -> the multi-module game process discriminates the
  3-arg size unit (`bytes_4N` vs `elements_N`); `eboot_index`/`eboot_seg0`
  locate the eboot entry.
- G5 `entries`/`prot_values` -> the game's mapping set for ownership/ranges.
- G6 `gua_verdict`/`tick_code0`/`sco_code0` -> vendored predicate verdicts
  (Validated/Inconclusive/Refuted + rcode) at the resolved base with explicit
  `base_provenance`; `needs-base` if G1/G4/G2 established none. NEVER guessed.
- G7 summary: `records_written`, `write_failures`, `ctor_events` (sentinel
  proof), `quiesce`=`inert_no_threads_module_stays_loaded`,
  `unload_attempted`=`no`.

## 6. Known risks surfaced before staging (see REPORT.md for detail)

- dlopen/dlclose are STRONG derived libkernel identities (SharpProspero
  resolved them locally to NIDs, but load-time acceptance is only provable on
  console). If the module FAILS to load with an import-related rc, fall back
  to the weak-dl variant: delete the two rows from STRONG_DERIVED in
  build_ingame.py, restore the weak declarations, rebuild (fresh dir) — G3
  then records `dl_route=none` honestly instead of risking the load.
- The three sceKernel module-query identities are similarly derived (same
  libkernel pattern the frozen sceKernelOpen/sysctl rows use); same fallback
  logic applies (they are G1/G4, so losing them degrades the experiment but
  not the load if dropped).
