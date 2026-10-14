# OBSERVER-DESIGN — modbridge_runtime_aligned_observer

New revision of the console-proven aligned-entry module lineage. Status:
design + host-test + local target-compile/link smoke complete; **console
behavior is NOT claimed**. The frozen module
`.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/`
(including its `build-1/modbridge_runtime_aligned_entry.prx`, its log
`/data/townfall_runtime_aligned_entry.log`, and its 88-byte Lua input) is
never modified; the observer stages alongside it.

## 1. Identity (all distinct from the frozen module)

| Property | Frozen aligned-entry | This observer revision |
|---|---|---|
| Module name / export library | `modbridge_runtime_aligned_entry` | `modbridge_runtime_aligned_observer` |
| PRX filename | `/data/plugins/modbridge_runtime_aligned_entry.prx` | `/data/plugins/modbridge_runtime_aligned_observer.prx` |
| Own log (O_APPEND, created 0600) | `/data/townfall_runtime_aligned_entry.log` | `/data/townfall_runtime_aligned_observer.log` |
| Marker | `modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0` | `modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556` |
| Worker payload | consumes memory-only Lua startup input | observes UE4SS startup evidence (read-only) |

Shared and unchanged: lifecycle (`lifecycle/*.c`, constructor invocation,
`force_align_arg_pointer` entry), import machinery (`import_references.S`,
`import_check.c`), diagnostic channel (`diagnostic.c`), 191-row import
contract (files byte-identical to frozen, see `receipts/frozen-copy-identity.txt`).
Removed vs frozen: `probe/startup_input.c`, `probe/memory_lua.c` — the
observer loads no input and performs no UE4SS memory access. Added:
`probe/observer.{c,h}` + observer constants in `source/generated/probe_config.h`.

## 2. Behavior

Startup (identical contract to frozen, new log): `module_start` (guarded,
re-entrant-safe) opens the retained O_APPEND channel on the observer log,
emits `module_start_entered`, entry alignment bits, `constructor_count`,
runs import validation (`import_validation_begin`,
`import_validation_missing_count`), checks the `__cxa_finalize` binding
(`dso_finalizer_binding_missing` only on failure), emits
`observer_revision_no_input`, invokes `.init_array` constructors
(`constructor_begin`/`constructor_return` pairs), emits
`full_runtime_constructors_complete`, then starts **exactly one worker
thread** (`worker_create_result`) and returns.

Worker thread (`source/probe/probe.c` worker_main): 30 polls, one 2 s
`nanosleep` between polls (~60 s total; EINTR-retried up to 8 times, else the
loop jumps to the summary). Each poll (`poll_once`, `source/probe/observer.c`
primitives):

1. `stat("/data/plugins/UE4SS.log")` — success emits `ue4ss_log_size` (64-bit,
   two lines, low word then high word); a non-regular file adds
   `ue4ss_log_not_regular`. First success establishes the baseline
   (`ue4ss_log_baseline`). Later:
   - growth → `ue4ss_log_grew` value=byte-delta, then ONE bounded tail read:
     a single `pread` of at most 512 bytes ending at the size just observed;
     only if `pread` fails, a bounded forward `read()` drain capped at
     1 MiB total (`ue4ss_tail_drain_capped` if the cap is hit). Emits
     `ue4ss_tail_bytes`, `ue4ss_tail_text_len`, optionally
     `ue4ss_tail_truncated`, and one sanitized text line (below). Any read
     failure emits `ue4ss_tail_read_failed` errno. Baseline updates.
   - shrink (rotation/truncation) → `ue4ss_log_shrank` +
     `ue4ss_log_new_size`, re-baseline, NO tail read.
   - same → no event.
   stat failure emits `ue4ss_log_stat_failed` errno.
2. `stat` existence of `/data/modbridge/roots/PPSA33286/Mods/TownfallItemSpawner`
   → `mod_root_present` value=1 (regular) or 2 (directory) / `mod_root_absent`
   value=0 / `mod_root_stat_failed` errno.
3. Same for the UE4SS-layout variant
   `/data/modbridge/roots/PPSA33286/ue4ss/Mods/TownfallItemSpawner`
   → `mod_root_ue4ss_*` events.

After poll 30 (or sleep failure): summary `observer_polls_completed`,
`observer_growth_events`, `observer_tail_failures`,
`observer_root_ever_present` (0/1), `observer_final_size` (64-bit pair),
`observer_complete`; the thread returns. `module_stop` joins it
(`worker_join_result`). No other thread is ever created.

Log line formats (single line each, `\n`-terminated):

    [PPSA33286] modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556 <tag> value=0x<hex8>
    [PPSA33286] modbridge-runtime-aligned-observer/v1 build=e9a58a9a8740438ebcb59136252a0556 ue4ss_tail=<sanitized text>

64-bit values (`*_size`, `*_baseline`, `*_new_size`, `*_final_size`) appear as
TWO consecutive lines with the SAME tag: first `value=` low 32 bits, second
`value=` high 32 bits (frozen low/high convention). `ue4ss_tail=` text is
sanitized to printable ASCII 0x20–0x7E (other bytes → `?`, CR dropped),
at most 255 chars, reporting the last COMPLETE line of the 512-byte tail
window (no trailing newline → final partial line; window starting mid-line
with no in-window newline → `ue4ss_tail_truncated`).

Read-only guarantee: the only `open()` calls are (a) O_RDONLY|O_NOFOLLOW
fresh descriptors per growth event into UE4SS.log (closed immediately,
`close`), and (b) the retained O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW channel
on the observer's OWN log path. UE4SS.log is never written, truncated,
created, or renamed; no O_TRUNC/O_WRONLY/O_RDWR touches observed paths; no
UE4SS memory access; `lseek` never called (`pread` gives positional reads
without moving any shared file offset).

## 3. Import compliance vs the frozen 191-row contract

Contract: `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json`,
sha256 `58d94bc65997d1a9fac839cf126dc323806214893df7a20d3e435341aceab86c`
(pinned by both profile.json and the packager). Verified this pass:
`counts` = required_strong 191, required_weak 6, identity_covered_strong 191,
covered_strong 191, missing_strong 0, conflicts 0; all 191 rows distinct
names, 187 FUNC + 4 OBJECT (`__mb_cur_max`, `__stderrp`, `__stdinp`,
`__stdoutp`); weak set exactly `__dladdr __dlclose __dlerror __dlopen
__dlsym kernel_mprotect`.

The observer introduces **zero new import names**. Its full call surface maps
onto rows already carried (and console-resolved) by the frozen module:

| Symbol | Contract row (module / library) | Kind | Observer use |
|---|---|---|---|
| `stat` | libkernel / libScePosix | FUNC | UE4SS.log size poll; mod-root existence (with `S_ISDIR`) |
| `open` | libkernel / libScePosix | FUNC | O_RDONLY\|O_NOFOLLOW fresh fd per growth event only |
| `pread` | libkernel / libScePosix | FUNC | primary bounded 512 B tail read |
| `read` | libkernel / libScePosix | FUNC | bounded drain fallback ONLY if pread fails (cap 1 MiB) |
| `close` | libkernel / libScePosix | FUNC | close the fresh fd |
| `nanosleep` | libkernel / libScePosix | FUNC | 2 s poll cadence (EINTR-bounded) |
| `pthread_create` | libkernel / libScePosix | FUNC | the single worker thread |
| `pthread_join` | libkernel / libScePosix | FUNC | module_stop join |
| `pthread_self` | libkernel / libScePosix | FUNC | re-entrancy guard (`probe_worker_is_current`) |
| `__error` | libkernel / libkernel | FUNC | `errno` |
| `sceKernelOpen` | libkernel / libkernel | FUNC | observer's own log channel (direct-bound, diagnostic table) |
| `sceKernelWrite` | libkernel / libkernel | FUNC | observer's own log writes (direct-bound) |
| `sceKernelClose` | libkernel / libkernel | FUNC | observer's own log channel (direct-bound) |
| `__cxa_finalize` | libSceLibcInternal / libSceLibcInternal | FUNC | DSO finalizer binding check (direct-bound) |

Explicitly NOT in the contract and NOT used (grep-verified over
`source/`): `lseek` (contract lacks it by design; `pread` replaces it),
`fstat`, `fopen/fread/fseek`, `mkdir/rename/unlink/truncate/ftruncate`,
`system`, `dlopen` family (the six weak names stay carried-unreferenced by
the frozen table, unchanged), and every `scePad*`/`sceUserService*` row
(carried by the frozen composition object, not referenced by observer C code).

Mechanics (unchanged from frozen): `modbridge_import_table` in
`import_references.S` carries 187 strong + 6 weak `.quad` references
(`PROBE_TOTAL_REFERENCE_COUNT 193`); `diagnostic_logging_imports` carries the
4 direct-bound rows; 187 + 4 = the full 191-row contract identity.
`generated/import_names.h` lists the 187 table names in contract order
(`PROBE_IMPORT_NAME_COUNT 187`). `probe_imports_validate()` requires all 187
carried slots nonzero before any observer work; `diagnostic_ready()` requires
the three kernel bindings non-NULL. The packager (`source/harness/Program.cs`)
refuses to emit anything unless the actual unresolved set equals the contract
exactly (191 names, 187 FUNC + 4 OBJECT, exact weak set, every name with
exactly one typed owned reference).

## 4. Host test

`host-test/host_test.c` + `host-test/shim_drain.c` run the SHIPPED
`source/probe/observer.c` on the build host (no console code): growth
classification incl. 0-byte baseline, last-line extraction edge cases (no
trailing newline, CRLF, empty line, CR-only, non-printables, fragment
window, capacity truncation), real temp-file stat/exists, tail reads
(<cap, >cap, ==cap, missing file), the pread-fail drain fallback via the
renamed shim unit (within cap and cap-exceeded), and an end-to-end fake
UE4SS.log growth sequence ending in the exact last line `[UE4SS] ready`.
Rebuild/run:

    cc -Wall -Wextra -O1 -I source/probe -o host-test/host_test \
       host-test/host_test.c host-test/shim_drain.c source/probe/observer.c
    ./host-test/host_test     # HOST_TEST_PASSED all observer logic cases, exit 0

Latest evidence: `receipts/finish-pass.txt` (also covers the -Werror-clean
prospero target-compile of all nine units and the `ld.lld -r` composition
against the pinned full-runtime object).

## 5. Staging

See STAGE-PLAN.md (ploader.ini `= 2` entry alongside the existing entries,
expected log lines, interpretation, rollback).
