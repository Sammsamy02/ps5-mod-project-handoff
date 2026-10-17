# STAGE-PLAN — modbridge_runtime_aligned_observer

Proposal-only staging plan for the observer PRX alongside the frozen,
still-active aligned-entry module. Console behavior is NOT claimed; every
console step below requires fresh explicit user authorization, Townfall fully
closed, FTP on, and manual loader dispatch (never automated). The frozen
module's PRX, its log `/data/townfall_runtime_aligned_entry.log`, and its
88-byte input are preserved untouched; the observer only ADDS entries.

## 0. Precondition

- Packaged artifact exists and is verified locally first:
  `build_local.py` (compile + `ld.lld -r`) then the harness packager
  (needs `dotnet`, currently absent on this Mac — see OBSERVER-DESIGN.md
  section 4 / receipts/finish-pass.txt) producing
  `build-1/modbridge_runtime_aligned_observer.prx`.
- Record artifact bytes + sha256; refuse to stage on any mismatch.
- Fresh preflight capture of current console state, all sha256-pinned:
  `/data/plugins/ploader.ini` (current = after-ploader state of the frozen
  placement, 594 B, sha256
  `6fb31fa526f678436a354f645fa13553e564367f10e0b0abf458186c686d4ff7`),
  the active `modbridge_runtime_aligned_entry.prx`, its log, and the input.
  The observer's own log must be confirmed ABSENT before staging.

## 1. Files to place (staged under unique temp names, byte-verified, then moved)

| Artifact | Console path |
|---|---|
| observer PRX | `/data/plugins/modbridge_runtime_aligned_observer.prx` |
| observer log | `/data/townfall_runtime_aligned_observer.log` (created by the module; must NOT pre-exist) |

Suggested temp names (same pattern as the frozen placement):
`/data/plugins/.aligned-observer.<placement-id>.candidate.pending` and
`/data/plugins/.ploader.aligned-observer.<placement-id>.pending`, with backup
`/data/plugins/.ploader.before-aligned-observer.<placement-id>.backup`.
Read back and hash every byte before activation.

## 2. ploader.ini edit (= 2 alongside existing entries)

Leave EVERY existing line byte-identical (including the frozen
`modbridge_runtime_aligned_entry.prx = 2` line — both modules coexist; they
write different logs). Append ONE line under `[PPSA33286]`:

    [PPSA33286]
    /data/plugins/modbridge_runtime_ctor_trace.prx = 0
    /data/plugins/modbridge_runtime_buffered_lua.prx = 0
    /data/plugins/modbridge_runtime_registration_trace.prx = 0
    /data/plugins/modbridge_runtime_worker_channel.prx = 0
    /data/plugins/modbridge_runtime_import_diagnostics.prx = 0
    /data/plugins/modbridge_runtime_import_probe.prx = 0
    /data/plugins/modbridge_log_probe.prx = 0
    /data/plugins/modbridge_entry_A0.prx = 0
    /data/plugins/modbridge_entry_A2.prx = 0
    /data/plugins/modbridge_runtime_aligned_entry.prx = 2
    /data/plugins/modbridge_runtime_aligned_observer.prx = 2

(Exact current content must be re-read at staging time; the block above is
the frozen placement's after-state plus the one new line. Order of lines in
the file is not load-bearing; presence and `= 2` are.)

Sequence: (1) stage PRX at temp path, hash-verify; (2) verify input/log
preconditions; (3) write new ploader.ini to temp, hash-verify, keep backup of
current; (4) activate the mode-2 config LAST; (5) user launches Townfall
manually.

## 3. Expected log lines

`/data/townfall_runtime_aligned_observer.log` is created on module_start with
lines of the form (M =
`modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556`):

Startup, in order:

    [PPSA33286] M module_start_entered value=0x00000000
    [PPSA33286] M entry_incoming_sp_mod16 value=0x0000000?   (0 or 8)
    [PPSA33286] M entry_call_sp_mod16 value=0x0000000?       (0 or 8)
    [PPSA33286] M constructor_count value=0x00000???         (same count as frozen lineage)
    [PPSA33286] M import_validation_begin value=0x000000bb   (187)
    [PPSA33286] M import_validation_missing_count value=0x00000000
    [PPSA33286] M observer_revision_no_input value=0x00000000
    [PPSA33286] M constructor_begin value=0x00000000  / constructor_return ... (per constructor)
    [PPSA33286] M full_runtime_constructors_complete value=0x00000000
    [PPSA33286] M worker_create_result value=0x00000000      (0 = worker started)

Per poll (every ~2 s, ~30 polls), when UE4SS.log is visible:

    [PPSA33286] M ue4ss_log_size value=0x????????            (low word)
    [PPSA33286] M ue4ss_log_size value=0x00000000            (high word)
    [PPSA33286] M ue4ss_log_baseline value=0x????????        (first poll only, low;+high)
    [PPSA33286] M mod_root_absent value=0x00000000           (or mod_root_present value=0x1/0x2)
    [PPSA33286] M mod_root_ue4ss_absent value=0x00000000     (or mod_root_ue4ss_present ...)

On each UE4SS.log growth:

    [PPSA33286] M ue4ss_log_grew value=0x0000002a            (delta bytes)
    [PPSA33286] M ue4ss_tail_bytes value=0x000001??          (<= 0x200)
    [PPSA33286] M ue4ss_tail_text_len value=0x0000001?
    [PPSA33286] M ue4ss_tail=[UE4SS] some startup line       (sanitized last line)

If UE4SS writes partial lines fast, `ue4ss_tail_truncated value=0x1` may
appear; if pread fails and the 1 MiB drain cap is hit,
`ue4ss_tail_drain_capped value=0x1`; hard read failures give
`ue4ss_tail_read_failed value=0x????????` (errno hex). A shrink
(rotation/truncation) logs `ue4ss_log_shrank` + `ue4ss_log_new_size` and
re-baselines without a tail read.

Summary (after ~60 s):

    [PPSA33286] M observer_polls_completed value=0x0000001e  (30)
    [PPSA33286] M observer_growth_events value=0x0000000?
    [PPSA33286] M observer_tail_failures value=0x00000000
    [PPSA33286] M observer_root_ever_present value=0x0000000? (0/1)
    [PPSA33286] M observer_final_size value=0x????????       (+high word)
    [PPSA33286] M observer_complete value=0x00000000

## 4. How to read results

- **UE4SS came up**: `observer_growth_events` > 0 AND at least one
  `ue4ss_tail=` line present. The `ue4ss_tail=` lines are the actual last
  lines UE4SS wrote — direct evidence of writer activity. `ue4ss_log_size`
  baseline alone (never grew) means the file existed but UE4SS wrote nothing.
- **UE4SS.log absent/never appeared**: repeated `ue4ss_log_stat_failed` lines
  (errno hex, 0x2 = ENOENT).
- **Mod root staged**: `mod_root_present` value=0x1 (regular) or 0x2
  (directory); `observer_root_ever_present` 0x1 means it appeared at some
  poll. Value 0x0 (`mod_root_absent`) at every poll means not staged at the
  observed path (also check the `mod_root_ue4ss_*` variant events for the
  UE4SS-layout location).
- **64-bit sizes**: two same-tag lines, first low word, second high word.
- **Failure decoding**: `module_start_return` nonzero → -20 entry busy,
  -21 lifecycle state, -22 own-log channel open failure,
  -10 `__cxa_finalize` binding missing; `import_validation_missing_count`
  nonzero is accompanied by `missing_import slot=0x?? name=...` lines naming
  the unresolved contract row(s). `worker_create_result` nonzero = thread
  creation failed (no observation happened).
- **Isolation**: the frozen `/data/townfall_runtime_aligned_entry.log`
  continues unchanged; the observer never appends to it, never writes
  `/data/plugins/UE4SS.log`, and holds no UE4SS references. Comparing the two
  logs is safe.

## 5. Rollback / cleanup

1. Set the observer line to `= 0` (or remove it) in ploader.ini — restore the
   backup file if preferred; byte-verify readback.
2. Delete `/data/plugins/modbridge_runtime_aligned_observer.prx` and, if the
   run is being discarded, the observer log (it is owned evidence; keep it if
   results are being reviewed).
3. Confirm the frozen aligned-entry line, PRX, log, and input are
   byte-identical to the preflight captures.
4. Never replay a failed activation without a fresh preflight and a distinct
   placement id.

## 6. Explicit non-goals

No promotion claim, no UE4SS-support claim, no score; observation only. A
positive result (growth events + tail lines) is evidence that UE4SS wrote its
log during the window — nothing more.
