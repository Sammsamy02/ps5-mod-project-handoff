# Console expected-results delta — Static TLS as the 7th family (worker-static-tls-7fam)

Baseline: the 6-family all-families console pass (checkpoint
`zcode_allfam_dll_console_run`, build 133c5e9e: attach 7/7, ten family keys,
worker 4/4, consumer_imports 78, results [86,93]). This document states
exactly what a console run of THIS worker's build is expected to add or
change. Evidence class: host-validated + SDK-ELF compile-only candidate; no
console contact was made by this worker.

## New / changed keys

| Field | Baseline | Expected with --static-tls | Why |
|---|---|---|---|
| `static_tls` | absent | `{"enabled":1}` | report key asserted exactly when the flag is on (build.py, result_report.c) |
| `counts.consumer_imports` | 78 | **82** | consumer binds 4 more PE imports: `__tls_index`, `__tls_start`, `__tls_end`, `__tls_import` (run_body formula +4; host run measured 82) |
| `counts.helper_imports` | 10 | 10 (unchanged) | helper carries no TLS directory and imports none of the adapter names |
| ELF undefined imports | 55 | **55 (delta ZERO)** | the two new native/ABI units call only internal `mb_win_threads_*` surfaces; no new SDK-library symbol is referenced (verified by build.py's elf-undefined + provider coverage steps) |
| win64cc IR definitions | prior families | **+1 exactly**: `@tls_import_thunk(i32, i64)` in `mb_win_static_tls_abi` | expected-dict entry `mb_win_static_tls_abi: 1`; the dispatch adapter is a plain-native definition containing one win64cc *call* through an ms_abi pointer type (mb_win_entry pattern), not a definition |

## Unchanged (must not shift)

- `results` stays `[86,93]`; `dll_calls` 3/3; `worker.calls_started/returned`
  4/4 (2 begin/collect + 2 file I/O); file I/O out-contract
  (`0x46494f31`/`0x46494f32`) untouched.
- `entry`: attach/detach calls [1,1], attach_result [7,7]/[0,0] detach,
  `contract_errors` 0.
- All six prior family keys stay `{"enabled":1}`; clock sessions [1,1]/[1,1].
- `cleanup` retires `{pending:0,retired:1}` — the static TLS owner closes
  only after the consumer unregistered with zero live blocks (unload
  cleanup proof).
- Worker-cleanup-fault modes keep first failure stage 25 /
  `MB_WIN_WORKERS_NATIVE_ERROR`, worker 2/2 before the retained admission.

## New in-fixture evidence (codes -120..-127, surfaced as failure stages if violated)

-120 index assigned+stable; -121 block = template copy; -122 same-thread
idempotence; -123 scratch write/readback; -124 callback prefix [(1,1),(1,2)]
then this window's [(2,1),(2,2)]; -125 `__tls_start`/`__tls_end` bounds and
template; -126 entered-thread block persistence into the begin window;
-127 collect-window checks: internal-thread isolation (same index, NULL
slot, readable template), block persistence, prefix log. The final detach
callback audits the complete log structurally (process-attach pair,
ascending attach pairs, descending detach pair) via the image's TLS index
cell high bit, audited by the composition inside the entry wrapper while the
image is mapped.

## What a console run would still NOT establish

No Windows-loader parity: no TEB/`__tls_array` emulation (the thunk and
`__tls_index` data are this family's adapter surface), lazy (not eager)
blocks for never-notified threads, no loader lock, callbacks are component
evidence routed through the composition's win64cc adapter. Target/console
execution remains the console checkpoint's to prove; this worker's receipts
carry `target_executed=false`, `console_contact=false`.
