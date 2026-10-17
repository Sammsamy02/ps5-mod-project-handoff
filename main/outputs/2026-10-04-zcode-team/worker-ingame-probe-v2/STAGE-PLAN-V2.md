# STAGE-PLAN-V2 — modbridge_ingame_facts PRX (build igf2-764f957dc0a6)

Exact controller staging steps for the v2 ISOLATION in-game probe. Everything
below is staging ONLY. The Townfall GAME LAUNCH itself is USER-COORDINATED —
never fake a launch, and never claim in-game results until
`/data/modbridge-ingame-facts.jsonl` exists on the console.

WHY v2 EXISTS: v1 (igf-f4a91d8123b0, sha db8ec550...) was silently
load-rejected while the sibling aligned-entry module loaded and logged in the
same launch (checkpoint `zcode_worker_ingame_probe.second_launch_diagnosis`).
HYPOTHESIS, UNPROVEN: the 5 DERIVED import identities were not accepted by
the console loader. v2 carries ONLY the 7 console-proven verbatim identities
and writes an ENTRY MARKER as its first channel action, so the file's very
existence separates load failure from experiment failure (see section 4).

## Artifact (frozen for this plan)

- PRX: `outputs/2026-10-04-zcode-team/worker-ingame-probe-v2/build-764f957dc0a6/modbridge_ingame_facts.prx`
- SHA-256: `94c10408055c6e6f4d403d1c66f4a8da399fdbae0ef0eb945e1faad16c23b09d`
- Bytes: 30,701 (bare-sdk ELF 86,048, sha `dc867858b1b14f9a5d5e47e0507b523eead3f6a16c4195f62a202621a1af4138`)
- Local checks: `build-764f957dc0a6/artifact-checks.json` = LOCAL_PACKAGE_CHECKS_PASSED
  (exactly 7 NID-encoded strong dynsym imports — all 7 NID bases are a subset
  of the console-proven lineage PRX's 191; ZERO weak imports; forbidden-symbol
  gate passed: no dlopen/dlclose/dlsym/module-query name, no _DYNAMIC, in the
  link; DT_INIT/DT_FINI explicit; init/fini arrays loader-unowned, 1 sentinel
  slot each; SDK 0x11000000; fself segment equality verified).
  Full receipts: `build-764f957dc0a6/build-receipt.json`. Two clean rebuilds
  reproduced the identical final hash (determinism/determinism.json.txt).
- Module name/soname/log path are UNCHANGED from v1 on purpose
  (`modbridge_ingame_facts`, `/data/modbridge-ingame-facts.jsonl`): this file
  REPLACES v1's, so `ploader.ini` needs NO edit.

## 1. Upload (replace v1's file)

Stage with the game CLOSED. Upload the PRX to exactly:

```
/data/plugins/modbridge_ingame_facts.prx
```

This REPLACES the v1 file (the ploader.ini line already points here; the
module name inside the artifact is identical, so nothing else changes).

Upload verification — ftpsrv decryption-oracle method (documented in
checkpoint `zcode_worker_ingame_probe.staging`): ftpsrv auto-decrypts FSELF
on RETR, so the readback is the DECRYPTED ELF, not the stored 30,701-byte
fself. Acceptance checks on the readback:

1. Readback begins with `\x7fELF`.
2. Readback size ≈ 86,048 minus small SHT truncation (v1 pattern: stored
   32,077 → readback 96,680 from a 98,240 bare-sdk; the readback for v2 must
   NOT be 96,680 — a different size confirms v1's file was actually replaced).
3. Payload beyond the header page byte-identical to
   `build-764f957dc0a6/bare-sdk/modbridge_ingame_facts.prx` except ≤4 sparse
   header/SHT-rebuild bytes + trailing-section truncation (v1 tolerance,
   same channel).
4. Full-hash equality is IMPOSSIBLE through this FTP channel for fself
   files; the final oracle is the loader itself (a corrupt file fails to
   load → no records: safe failure mode).

## 2. Configure ploader — NO INI CHANGE NEEDED

`/data/plugins/ploader.ini` already contains (from v1 staging, preserved):

```ini
[PPSA33286]
/data/plugins/modbridge_ingame_facts.prx = 2
```

Verify the line is still present and ALL other pre-existing entries are
untouched (including the proven aligned_entry = 2). Mode 2 = LATE load,
re-fires per game launch. Do not add or reorder anything.

Rollback: delete `/data/plugins/modbridge_ingame_facts.prx` and remove/restore
the ini line (v1's file is recoverable from
`worker-ingame-probe/build-f4a91d8123b0/` if ever needed, though v1 never ran).

## 3. Expected record file

`/data/modbridge-ingame-facts.jsonl` — created by the module via the
lineage-proven sceKernelOpen/sceKernelWrite/sceKernelClose trio
(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600). Same envelope as v1, NEW
build_id prefix:

```
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"igf2-764f957dc0a6",
 "seq":N,"exp":"entry"|"startup"|"v1_vmmap_full"|"v1_vmmap_page"|
 "v2_elf_base_scan"|"v2_scan_page"|"v3_dtdebug"|"v4_layout_predicates"|
 "v5_fw_summary","rc":R,"errno":E,"fields":{...}}
```

Record order: `entry`(seq1, written BEFORE any scan/pointer read),
`startup`(seq2, ctors_run + channel_open_rc), `v1_vmmap_full` +
0..48 `v1_vmmap_page` parts (20 entries each, complete bounded list),
`v2_elf_base_scan` + 0..10 `v2_scan_page` parts (7 candidates each),
`v3_dtdebug`, `v4_layout_predicates`, `v5_fw_summary`. Each record ≤2 KiB;
truncation marked `"__trunc":1`.

## 4. Launch + collection WITH ENTRY-MARKER DISCRIMINATION (controller addition)

1. Ask the USER to launch Townfall normally (ploader picks the module up
   late). Give it a minute, then retrieve
   `/data/modbridge-ingame-facts.jsonl` and store a copy + SHA-256 next to
   the staging receipt.
2. DISCRIMINATING LOGIC (the aligned-entry sibling proved a /data write
   works from inside the game in the same launch that rejected v1, so the
   channel itself is not in question):
   - File ABSENT → v2 failed BEFORE the first write: load-time rejection OR
     sceKernelOpen failure (module_start returns -22 in that case; watch for
     a ploader FAIL notification and record its rc verbatim). With all 7
     imports console-proven, THIS OUTCOME REFUTES/WEAKENS the
     derived-import hypothesis and moves suspicion elsewhere (see 5).
   - File = `entry` line ONLY → load + channel-open SUCCEEDED; the failure
     happened after the marker (ctor walk / ig_collect_vmmap / V1) —
     localize with the `startup` line's absence/presence.
   - `entry` + `startup` present, no V-records → failure inside the VMMAP
     sysctl walk or its record emission.
   - Full record set → complete run; interpret per section 6.
   Absence is a RESULT, never a retry trigger; report it as-is.
3. IMPORTANT: `module_start` also returns -22 when the log cannot be opened;
   if a ploader FAIL popup appears, capture its text/word verbatim — it is
   the only loader-side signal for the absent-file case.

## 5. Hypothesis discipline (controller addition 3)

"v1 was rejected at load BECAUSE of the 5 derived import identities" is a
HYPOTHESIS, not an established fact. v2's evidence structure decides:

- v2 loads and writes records (entry marker present) → the only material
  import-table difference was the 5 derived identities → strong support for
  the hypothesis; the loader facts + layout verdicts also become available.
- v2 is ALSO silently rejected (absent file, ploader OK) → hypothesis
  REFUTED for practical purposes: something else distinguishes these builds
  from the aligned-entry lineage (candidates: our smaller import table
  itself, DynamicWriter output details at 7 rows, module size/segments,
  soname collision, environment state). The record then feeds the next
  isolation step (e.g., 7-import no-experiment shell that only writes the
  entry marker).

## 6. Interpretation map (honest outcomes)

- `v1_vmmap_full` `entries`/`spans_kept`/`prot_histogram` + page parts → the
  game's complete bounded mapping set (list bound: min(spans, 20*48=960),
  `list_truncated` flags overflow; histogram covers every kept span).
- `v2_elf_base_scan` `chosen_base`/`chosen_extent`/`provenance`=
  "v2_elf_base_scan" → main eboot candidate = LARGEST executable image whose
  dynamic section parses; every candidate carries full evidence
  (magic/class/e_type/phnum/phdr_ok/exec/extent/dyn_mode/dynok/dynn/dtnull)
  so the controller can overrule the choice. `dyn_mode`: 1 = dynamic at
  base+p_vaddr (ET_DYN bias), 2 = absolute p_vaddr (ET_EXEC). Absent chosen
  candidate → `needs-base`.
- `v3_dtdebug` `dt_debug_present` + `dt_debug_value` + `entries` → game rtld
  populates DT_DEBUG in-process at the scanned base; `validation_blocked_
  derefs` counts refused reads (containment worked, never faulted).
- `v4_layout_predicates` `gua_verdict`/`gua_layout_matched` (BOTH hypotheses
  + classifiers)/`tick_code0`/`sco_code0` → vendored worker-5 predicate
  verdicts at the scanned base with `base_provenance`; needs-base skip ONLY
  if no candidate parsed. NEVER guessed.
- `v5_fw_summary` `kern_version` (sysctl KERN_VERSION; crt1
  kernel_get_fw_version is NOT linkable in a PRX — v1 G7 note stands),
  `records_written`, `write_failures`, `ctor_events`,
  `quiesce`=`inert_no_threads_module_stays_loaded`,
  `unload_attempted`=`no`.

## 7. Known risks surfaced before staging (see REPORT.md for detail)

- Base scan heuristics: RX-span starts + readable spans adjacent to exec
  spans; a game image whose header page is NOT at a span start (or split
  differently) would not be enumerated; all candidates + evidence are
  recorded so the controller can re-derive the choice offline.
- Dynamic address mode is a 2-hypothesis test chosen by DT_NULL/sanity
  score (tie → base+vaddr, the v1 spelling); `dyn_mode` is recorded.
- Candidate records cap at 64 (`cands_truncated` flags); vmmap list caps at
  960 entries in page records (`list_truncated`).
- If v1's file were NOT actually replaced, the ini line would load the OLD
  module (96,680-byte readback betrays it — check section 1.2).
