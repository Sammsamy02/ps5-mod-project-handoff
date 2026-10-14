# STAGE-PLAN-COMBINED — composition-5 + qdrv1 + observer in one ploader.ini

Worker: worker-game-session-ridealongs, 2026-10-04. Proposal-only staging
plan. NO console contact was made; every console step requires the standing
ritual (game fully closed, Townfall closed on console, FTP up, ploader armed,
USER launches manually — never automated). Feeds worker-console-batch-runner's
existing `stage-qdrv1` step (section 8); that worker's files were NOT edited.

## 0. Frozen artifacts (verify hashes before ANY upload; refuse on mismatch)

| Module | Console path | Bytes | sha256 |
|---|---|---|---|
| qdrv1 (worker-record-sink-addon) | `/data/plugins/modbridge_qh_driver.prx` | 26157 | `52c3221665a423ddd2b22faddd4ac59ea8fd0ab8c1880c61e225d32a43df70a4` |
| observer (worker-observer-packaging) | `/data/plugins/modbridge_runtime_aligned_observer.prx` | 30152151 | `1afa0997682cc6ecc14fd9f82961d68755edfacb36b36a4135203e37b1011e30` |

Local verification done this session (both PASSED, see
VERIFICATION-RECEIPTS.json): re-hash matched receipts; the workers' own
checkers were re-run — `check_qdrv_artifact.py` on determinism/run1 AND run2
(LOCAL_PACKAGE_CHECKS_PASSED, fself law, init/fini 1/1, SDK 0x11000000,
11-row decoded import equality) and `check_observer_artifact.py` on build-1
AND build-2 (LOCAL_PACKAGE_CHECKS_PASSED, init/fini 232/2 == frozen lineage,
191-row frozen-contract exact equality, build2 byte-identical). Top-level
delivered copies are cmp-equal to the verified build-dir copies.

Neither PRX replaces anything: both stage under their OWN names; distinct
module/export identities (`modbridge_qh_driver`, `modbridge_runtime_aligned_
observer`) — safe alongside the composition (`modbridge_supplier_composition`),
the frozen aligned-entry module, and path_test.

## 1. Combined ploader.ini — reconciled 6-entry layout

Parser facts (pinned source
`.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`):

- `MAX_PLUGINS = 10` per title, `MAX_TITLES = 10`, path <= 255 chars, line
  buffer 1024 (L26-29, L658). Six entries are inside every limit.
- `parse_mode` (L605): only 0/1/2 are valid; anything else is dropped.
- Mode semantics: **0 = disabled — the entry is SKIPPED at parse (L672),
  never registered, never PT_ATTACHed, no FAIL notify, no timeout wait**;
  1 = early (stable 150 ms / min_mod 6 / settle 150 ms); **2 = late (stable
  1000 ms / min_mod 16 / settle 600 ms, per-plugin wait cap 25 s, L471-476,
  L779)**.
- Disabled lines consume NO slot (skipped before `register_plugin`), so the
  combined file registers **5 plugins**: aligned_entry, path_test,
  composition, qdrv1, observer — 5/10 (6/10 even if ingame_facts were
  re-enabled).
- Registration order == file order == ploader injection order (sequential
  per-pid loop L776-820). Order is otherwise not load-bearing.
- **ploader does NOT dedupe paths** (confirmed by simulate_ploader_ini.py
  `duplicate_line_hazard` variant: a duplicated line registers and injects
  TWICE). The edit must be append-only and count-verified.

Decisions (with reasons):

1. `modbridge_ingame_facts.prx = 0` — **STAYS, byte-identical.** Parser-inert
   (never loaded), so it is NOT a second C2 writer: qdrv1's
   `/data/modbridge-ingame-facts.jsonl` stays single-writer, which is all
   STAGE-PLAN-QDRV1 sec 1 actually required ("consider unstaging the igf6
   one-shot probe" — mode 0 already achieves that). Keeping the line
   preserves audit provenance; the igf6 .prx file may stay on disk unloaded.
2. `modbridge_path_test.prx = 2` — **STAYS.** One-shot freshness oracle: at
   module_start it appends exactly two lines (`PATH_TEST: ...` /
   `ACTIVATION_TEST: ...`) to `/data/plugins/UE4SS.log`, returns 0 always,
   no threads (worker-minimal-path-test STAGE-PLAN sec 1-2). Harmless at 6
   total plugins (see audit, sec 5); its appends double as observer
   growth-event cross-checks.
3. Both new lines **APPENDED `= 2`, AFTER the composition line** so the
   composition's startup ladder begins first and the ridealongs load last.

Reference shape (line ORDER of existing lines must follow the ACTUAL Step-0
RETR pre-image — history shows order was never recorded host-side; this is
the rollback-runbook sec 4.3 expected layout plus the two appends):

```ini
[PPSA33286]
/data/plugins/modbridge_runtime_aligned_entry.prx = 2
/data/plugins/modbridge_path_test.prx = 2
/data/plugins/modbridge_ingame_facts.prx = 0
/data/plugins/modbridge_supplier_composition.prx = 2
/data/plugins/modbridge_qh_driver.prx = 2
/data/plugins/modbridge_runtime_aligned_observer.prx = 2
```

HONESTY CAVEAT (carried from worker-rollback-runbook sec 4.3): the exact
console bytes were never recorded host-side. The Step-0 RETR below is
mandatory and authoritative; the block above is a shape reference, never a
replacement file.

## 2. Step-0 preflight (all sha256-pinned, before any change)

1. RETR `/data/plugins/ploader.ini` -> `/tmp/<TAG>/ploader.ini.pre`; record
   sha256, byte count, line count, and last byte. FAIL if: no
   `[PPSA33286]` header, OR any `modbridge_qh_driver` /
   `modbridge_runtime_aligned_observer` line already present (duplicate
   hazard), OR any expected existing line is missing (reconcile first —
   see rollback-runbook sec 4.3 variant rules).
2. RETR `/data/plugins/UE4SS.log` stat (size + mtime) — freshness baseline.
3. CONFIRM `/data/townfall_runtime_aligned_observer.log` is ABSENT (observer
   precondition; it must be created fresh by the module).
4. RETR `/data/modbridge-ingame-facts.jsonl` size + sha256 IF PRESENT
   (igf-era baseline; qdrv1 lines are distinguishable by
   `build_id":"qdrv1-` prefix; the file is opened O_APPEND|O_CREAT so old
   lines persist harmlessly — record the baseline, do not delete owned
   evidence).
5. On-console backup: STOR the untouched pre-image to
   `/data/plugins/ploader.ini.<TAG>.bak`.

## 3. PRX upload + readback (SHT-strip law)

Channel law (pinned worker-v3-startup-diag REPORT sec 2): the ftpsrv
decryption readback of a staged fself is the **segment-covered prefix of the
inner (bare-sdk) ELF** — make_fself embeds per-phdr `filesz` slices only;
the SHT tail is not embedded and `e_shnum` is zeroed in the embedded header.
Full-file hash equality through this channel is IMPOSSIBLE by construction.
Verification is therefore exact-length + body-hash + Ehdr-exception.

Exact expected constants (computed this session from the verified build-dir
bare-sdk ELFs; qdrv1's bare sha equals its build-receipt `inner_elf_sha256`):

| Module | staged fself sha256 | bare-sdk sha256 | readback bytes = max(p_offset+p_filesz) | readback body sha256 (bytes 64..end) |
|---|---|---|---|---|
| qdrv1 | `52c32216...` (26157 B) | `7ae1e4e39c5f312ec5927be6bec82d92c860ab8b3b65ef4a9826fd296c8ce1d9` (88744 B) | **87112** | `71406d3ccc7c1da0688b68ff35fd35c4a7ef9241b4d02ce427504cd392002cf5` |
| observer | `1afa0997...` (30152151 B) | `d0a3d6feebae18680962fb976726e86ea1ea397e183185793b75c290c6c0290f` (30145992 B) | **30143976** | `3f89a5b66bba614926f7e7b1273c1d2aadeb505b2797dcd7389b12cebeee6622` |

Upload pattern (BINARY; per worker-minimal-path-test sec 3, FTP 2121):

```sh
TAG=<timestamp>; FTP=<standing ftp base url>
# upload to FINAL name (distinct files; nothing replaced), then RETR readback
curl -s -T modbridge_qh_driver.prx                "$FTP/data/plugins/modbridge_qh_driver.prx"
curl -s "$FTP/data/plugins/modbridge_qh_driver.prx" -o /tmp/<TAG>/qh_driver.readback
curl -s -T modbridge_runtime_aligned_observer.prx "$FTP/data/plugins/modbridge_runtime_aligned_observer.prx"
curl -s "$FTP/data/plugins/modbridge_runtime_aligned_observer.prx" -o /tmp/<TAG>/observer.readback
```

Verify each readback against the SHT-strip law (exact snippet):

```sh
python3 - "$READBACK" "$BARE_SDK" <<'EOF'
import sys, struct, hashlib
rb = open(sys.argv[1],'rb').read(); bare = open(sys.argv[2],'rb').read()
phoff = struct.unpack_from("<Q", bare, 32)[0]
n = struct.unpack_from("<H", bare, 56)[0]
end = 0
for i in range(n):
    o = phoff + i*56
    off, _, _, filesz = struct.unpack_from("<QQQQ", bare, o+8)
    if filesz and off+filesz > end: end = off+filesz
ok = len(rb) == end and rb[:4] == b"\x7fELF" and rb[64:end] == bare[64:end]
hdr_diff = [hex(i) for i in range(64) if rb[i] != bare[i]]
allowed = set(range(40,48)) | set(range(58,64))   # e_shoff, e_shentsize, e_shnum, e_shstrndx
hdr_ok = all(i in allowed for i in range(64) if rb[i] != bare[i]) \
        and struct.unpack_from("<H", rb, 60)[0] == 0   # e_shnum zeroed
ok = ok and hdr_ok
print("READBACK-OK" if ok else
      f"READBACK-FAIL len={len(rb)}/{end} body={'OK' if rb[64:end]==bare[64:end] else 'MISMATCH'} hdr={hdr_diff}")
sys.exit(0 if ok else 1)
EOF
# qdrv1:    $BARE_SDK = worker-record-sink-addon/determinism/run1/bare-sdk/modbridge_qh_driver.prx
# observer: $BARE_SDK = worker-observer-packaging/build-1/bare-sdk/modbridge_runtime_aligned_observer.prx
```

Expected: `READBACK-OK`, readback sizes exactly 87112 / 30143976. Any FAIL
-> STOP, re-STOR from the verified local fself once, re-verify; a second
failure means abort the staging (leave console as-is, reconcile).

## 4. Ini edit + verify (append-only law)

1. Build the new bytes LOCALLY from the Step-0 pre-image (byte-exact,
   append-only; handle a missing trailing newline explicitly):

```sh
cp /tmp/<TAG>/ploader.ini.pre /tmp/<TAG>/ploader.ini.new
tail -c 1 /tmp/<TAG>/ploader.ini.pre | od -An -c | grep -q '\\n' || printf '\n' >> /tmp/<TAG>/ploader.ini.new
printf '/data/plugins/modbridge_qh_driver.prx = 2\n'                 >> /tmp/<TAG>/ploader.ini.new
printf '/data/plugins/modbridge_runtime_aligned_observer.prx = 2\n'  >> /tmp/<TAG>/ploader.ini.new
```

2. `diff` pre vs new must show EXACTLY the two appended lines (three if a
   newline had to be added). Never rewrite the header, never touch/reorder
   existing lines (append-only; ploader register order = file order).
3. Run `simulate_ploader_ini.py /tmp/<TAG>/ploader.ini.new`-style check:
   registered count must be exactly 5, all mode 2 except ingame_facts (which
   must NOT appear as registered), order per file. (The simulator's
   CANONICAL constant can be swapped for the actual bytes.)
4. STOR the new ini, RETR fresh readback, `diff` against the local new bytes
   (plain file: byte-exact law). Only then proceed to launch.

## 5. Interaction audit (all sources cited; verdict: NO interference)

Writers/readers per file:

| Path | Writers | Readers | Verdict |
|---|---|---|---|
| `/data/plugins/UE4SS.log` | UE4SS itself; path_test (2 lines, once, `O_WRONLY|O_CREAT|O_APPEND` 0644, path_test STAGE-PLAN sec 1); composition on FAILURE paths only (`write_ue4ss_log_line`, runtime_module_entry.c L162-190, `O_APPEND|O_NOFOLLOW` 0600, strictly before its runtime threads start) | observer ONLY: `stat` per poll + fresh `O_RDONLY|O_NOFOLLOW` fd + single `pread` <=512 B tail, drain fallback capped 1 MiB, never writes/truncates/lseeks (OBSERVER-DESIGN sec I/O table L88-93, L112-118) | Append-only writers vs a read-only tail reader: no locks anywhere, no truncation, partial-line races land in the documented `ue4ss_tail_truncated` path; shrinks re-baseline. `PATH_TEST`/`ACTIVATION_TEST` appends will surface as observer growth events — expected cross-check, not noise. |
| `/data/modbridge-ingame-facts.jsonl` | qdrv1 only (own `sceKernelOpen/Write/Close` trio, `O_APPEND|O_CREAT` 0600, one `sceKernelWrite` per <1 KB line; DESIGN sec 4-6) | host-side rung_check after capture | igf6 probe is `= 0` -> skipped at parse (main.c L672), NOT a writer; composition provably never writes it (qdrv1 CLAIM: tree-wide grep — nothing calls OnEngineTickDispatch/FormatRecordJsonLine). Single writer, append-only. |
| `/data/modbridge/roots/PPSA33286/runtime-status.txt` | composition only (mfa_status sink, one-sink-per-process rule WIRING-SPEC S4) | host | path_test reads `loadlist.txt` (different file); observer only `stat`s the two Mods-dir candidates (OBSERVER-DESIGN L62-66) — stat never conflicts. |
| `/data/townfall_runtime_aligned_entry.log` | composition diagnostic channel (`PROBE_LOG_PATH`, generated/probe_config.h L3) + frozen aligned-entry module | host | observer never appends to it (OBSERVER-DESIGN isolation section). |
| `/data/townfall_runtime_aligned_observer.log` | observer only (`O_APPEND`, created 0600) | host | absent pre-stage; single writer. |

Threads and timing:

- ploader injects the 5 plugins SEQUENTIALLY in file order; each injection
  is one PT_ATTACH + remote `sceKernelLoadStartModule` cycle (main.c
  L776-820). The ridealongs' `module_start`s run after the composition's,
  with ~1.6 s minimum spacing (mode-2 stable+settle) — six entries add
  roughly 10-15 s total, far inside the 120 s `INJECTOR_TIMEOUT_S` (L735).
- observer: exactly ONE worker thread, 30 polls x 2 s nanosleep (~60 s,
  EINTR-retried <=8), then summary and join (OBSERVER-DESIGN L39-44, L69-73).
- qdrv1: exactly ONE detached driver thread, 30 polls x 4 s (~2 min,
  DESIGN sec 4); its admission gate refuses (`bindings_absent` /
  `gate_closed`) until the GUA header admits — starting BEFORE engine-ready
  is by design; the harness dispatch begins only after 6 consecutive
  admitted polls with count > 10000 (DESIGN sec 5).
- Neither ridealong imports a mutex (qdrv1 surface = 11 frozen rows: igf 7 +
  nanosleep/pthread_create/pthread_detach/pthread_self; observer surface =
  stat/open/pread/read/nanosleep-family/pthread_* per OBSERVER-DESIGN
  L112-118). Spinning is bounded (qdrv1 bounded spin in the record path;
  observer EINTR retries). CPU cost per poll is a handful of reads/stats —
  negligible against the composition's ladder.
- qdrv1 reads pinned GUA memory (header-window reads only); the observer
  never touches game memory. Read-read overlap with the composition is
  inert.
- Bounded-window honesty: both windows run from each module's OWN
  module_start. If the title takes longer than ~60 s (observer) / ~2 min
  (qdrv1) to reach the observed states, those windows close with honest
  negatives (stat_failed / admission_rcode != 0 lines). That is design
  bounds, not interference; a new window needs a NEW launch (never re-rate
  unchanged evidence).
- Pre-existing ploader quirk (unchanged by this staging): if ANY plugin of
  the title fails mid-list, `all_ok=0` keeps the entry un-injected and the
  whole list is retried — already-loaded modules get
  `sceKernelLoadStartModule` called again (main.c L775-822). Mitigation:
  read every ploader notification line; on any FAIL, STOP and triage; do
  not relaunch the identical layout hoping for a different result.

## 6. Expected evidence files at session end (5 logs)

| # | File | Producer | Success signature |
|---|---|---|---|
| 1 | `/data/plugins/UE4SS.log` | UE4SS (+ path_test oracle lines; composition failure lines only if failing) | grew during session; contains `PATH_TEST: /data/plugins/UE4SS.log writable at ...` + `ACTIVATION_TEST: loadlist.txt readable, first 16 bytes: <hex>` for THIS launch (freshness oracle); observer tail lines may show them |
| 2 | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | composition-5 ladder | engine-ready-or-later status lines (RUNTIME-ACK ladder) |
| 3 | `/data/townfall_runtime_aligned_entry.log` | composition diagnostic channel (+ frozen aligned-entry) | constructor/ladder events; NO new failure lines |
| 4 | `/data/townfall_runtime_aligned_observer.log` | observer (NEW file) | `worker_create_result value=0x00000000`; per-poll `ue4ss_log_size`; `ue4ss_log_grew` + `ue4ss_tail=` lines; summary `observer_polls_completed value=0x0000001e` + `observer_complete value=0x00000000` (STAGE-PLAN sec 3 of worker-aligned-entry-observer) |
| 5 | `/data/modbridge-ingame-facts.jsonl` | qdrv1 (NEW lines carry `build_id":"qdrv1-`) | per-poll `qh_counter` envelopes (`admission_rcode` 1..7 -> 0 once admitted); `refused_bindings_absent`/`refused_layout_gate_closed` counters; `ticks_accepted` 0->1->2; terminal `{"record":"native_query_harness",...,"status":"completed"...}`; `summary` at ~2 min (STAGE-PLAN-QDRV1 sec 2) |

Rung timing (from STAGE-PLAN-QDRV1 sec 3): wait for runtime-status
engine-ready, then run `rung_check.py` and re-run every ~20-30 s; a PASS
needs snapshots straddling the 8 s 0->1->2 ramp; after the terminal record
the file freezes and later runs honestly report ticks-flat FAIL.

## 7. Rollback

1. Game closed, ploader NOT armed: flip BOTH new lines to `= 0` (two
   single-character edits on the composition-untouched file) or restore
   `/data/plugins/ploader.ini.<TAG>.bak`; RETR + byte-verify readback.
   Mode 0 = skipped at parse: no attach, no FAIL notify, no timeout
   (rollback-runbook sec 4.2 evidence).
2. Optionally delete `/data/plugins/modbridge_qh_driver.prx` and
   `/data/plugins/modbridge_runtime_aligned_observer.prx` if discarding;
   the observer/qdrv1 LOGS are owned evidence — keep them for review.
3. Confirm aligned_entry, path_test, composition lines and their
   logs/PRXs are byte-identical to the Step-0 preflight captures.
4. Never replay a failed activation without a fresh preflight and a
   distinct TAG. Never relaunch an identical rejected binary.

## 8. Combined-staging section for worker-console-batch-runner (`stage-qdrv1` extension)

Machine-consumable step list. Gates are stop-on-mismatch
(`--stop-on-first-failure` semantics). `PRX_DIR_Q` = worker-record-sink-addon,
`PRX_DIR_O` = worker-observer-packaging, `FTPB` = standing FTP base.
The qdrv1-only behavior of the existing `stage-qdrv1` step is superseded by
SC-1..SC-5 (superset: same upload/readback pattern, second PRX added, ini
step made explicit). Emit user-action prompts exactly at SC-6 (arm ploader)
and SC-7 (user launches Townfall manually).

| Step | Action | Command / source of bytes | Gate (STOP on mismatch) |
|---|---|---|---|
| SC-0 | preflight | RETR ploader.ini -> `.pre`; UE4SS.log stat; observer-log absence; jsonl baseline sha if present; STOR `.pre` -> `ploader.ini.<TAG>.bak` | header `[PPSA33286]` present exactly once; NO qdrv1/observer lines already present; all 4 expected existing lines present; .bak STOR readback == `.pre` |
| SC-1 | upload qdrv1 | `curl -s -T $PRX_DIR_Q/modbridge_qh_driver.prx $FTPB/data/plugins/modbridge_qh_driver.prx` | local sha == `52c32216...` before upload |
| SC-2 | readback qdrv1 | RETR -> `.readback`; run sec 3 snippet vs `$PRX_DIR_Q/determinism/run1/bare-sdk/modbridge_qh_driver.prx` | `READBACK-OK`, size == 87112, body sha == `71406d3c...` |
| SC-3 | upload observer | `curl -s -T $PRX_DIR_O/modbridge_runtime_aligned_observer.prx $FTPB/data/plugins/modbridge_runtime_aligned_observer.prx` | local sha == `1afa0997...` before upload |
| SC-4 | readback observer | RETR -> `.readback`; sec 3 snippet vs `$PRX_DIR_O/build-1/bare-sdk/modbridge_runtime_aligned_observer.prx` | `READBACK-OK`, size == 30143976, body sha == `3f89a5b6...` |
| SC-5 | ini edit | sec 4 append-only procedure from `.pre` | diff == exactly the 2 appended lines (+possible leading NL); registered-count check == 5; STOR + RETR diff byte-exact |
| SC-5b | pre-launch collection | capture ploader notification surface text after launch | exactly 5 OK lines (`OK (<ms>): <filename>`), zero FAIL lines; any FAIL -> STOP, sec 7 |
| SC-6 | USER ACTION | prompt: arm ploader | — |
| SC-7 | USER ACTION | prompt: user launches Townfall manually (never automated) | — |
| SC-8 | evidence collection | RETR the 5 files of sec 6 (observer log + jsonl may take ~2-3 min post-launch) | signatures per sec 6 table; rung_check re-run ~20-30 s until ramp straddled or window honestly closed |

Verdict ledger additions for the runner: `QDRV-UPLOADED`, `QDRV-READBACK`,
`OBS-UPLOADED`, `OBS-READBACK`, `INI-COMBINED`, `PLOADER-5OK`, plus the five
evidence verdicts `EVID-UE4SS`, `EVID-STATUS`, `EVID-ENTRYLOG`,
`EVID-OBSERVER`, `EVID-JSONL`.

## 9. Honesty boundaries

Staging preparation only. No console contact was made; no behavior on
console is claimed. Rung-1/2 PASS from the qdrv1 channel evidences the
RECORD PATH and real GUA count observations, NOT execution on the engine's
OS game thread (`game_thread_id_key` is the driver-thread fold). A positive
observer result evidences UE4SS writer activity during the window, nothing
more. Composition-5 behavior claims remain with its own plans.
