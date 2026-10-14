# ANALYSIS.md — where diagnostic_event lives in composition-6 and why launch-9 lost the values

Worker: worker-diagnostic-sink, 2026-10-04/05. Host-side analysis only; no
console contact. Line/page numbers cite the files as read this session.

## 1. Where diagnostic_event is DEFINED and linked

Exactly ONE definition is linked into the composition-6 PRX:

- **Defining TU:** `diagnostic.c` at the lineage root,
  `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/diagnostic.c`
  (sha256 `931d79691b6363f046731182720ef7bec13a51c4a99e778943b199a97fb7d8b3`).
  The comp6 driver's overlay (`worker-comp6-driver/staging/lineage-overlay-2/`)
  carries it as a SYMLINK to that lineage file (verified 2026-10-05); the
  controller's `outputs/2026-10-04-zcode-controller/comp6-integration/` copies
  (UE4SSProgram.cpp, NativeLoaderContext.*, lmf_*) contain NO diagnostic_event.
  `outputs/ps5-mod-bridge/native-runtime-startup/diagnostic.h` is the shared
  prototype header.
- **Link path:** `worker-prx-link-gaps/package_prx.py` `BOOTSTRAP_UNITS`
  (line 128-140, `diagnostic.c` at line 138), compiled in `link_pipeline`
  (~line 657) with the frozen flags and resolved against
  `PIN['lineage']` = the driver-built overlay, then merged into the final PRX
  by `ld.lld -r` with the composed runtime object.
- **Callers in the same PRX:** the wired entry
  (`worker-module-start-wiring/runtime_module_entry_r4.c`, ~30 call sites) and
  the bridge (`runtime_start_bridge_r13.cpp`, line 62 `extern "C"` declaration,
  34 `::diagnostic_event` call sites: bridge_vmentry_entries,
  bridge_vm_walk_stop, bridge_kve_distinct0..3, bridge_kve_eboot,
  bridge_exepath_valid, bridge_exepath_sig, bridge_lease_reg,
  bridge_lease_reg_spans, and the failure fan-outs).
- The sdk-tree copies under `outputs/ps5-mod-bridge/native-runtime-startup/`
  and `outputs/2026-10-04-zcode-team/worker-observer-packaging/observer-src/diagnostic.c`
  are the same lineage text; the comp6 PRX gets the lineage-root copy above.

## 2. The current sink, and why it is not durable for launch-10

`diagnostic.c` (lines 39-69, 145-154) implements a SINGLE channel:

- `diagnostic_open_channel()` opens `PROBE_LOG_PATH` =
  **`/data/townfall_runtime_aligned_entry.log`** (`generated/probe_config.h:3`)
  — the SHARED entry log — with `O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600`
  via `sceKernelOpen`, caching the fd in `channel_fd` (line 14).
- `diagnostic_event` formats
  `[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 <tag> value=0x…`
  and `write_line()` writes it ONLY if `channel_fd >= 0`.

Four independent reasons the launch-9 values were not durably recorded:

1. **Marker collision makes attribution impossible.** The marker string is
   pinned to the ALIGNED-ENTRY build id (`f6530c79…`), not the composition-6
   candidate. The aligned-entry PRX, the older composition PRXs, and comp6 all
   write lines with the IDENTICAL marker to the IDENTICAL file —
   `worker-diag-channel-postmortem/CHANNEL-POSTMORTEM.md` §1 documented this
   collision ("marker-level attribution between them is impossible"). The
   launch-9 snapshot (`launch9-evidence/aligned-entry-log-snapshot.log`,
   2,343,756 B, append-only across ALL launches) contains 92 `bridge_` lines,
   at least some from EARLIER sessions (e.g. line 15969-15976 predate the r13
   `bridge_lease_reg_spans` tag; the log carries no timestamps or pid), so a
   verifier cannot prove which values belong to launch-9.
2. **Dead-channel silent drop.** Once `channel_fd` is negative — the channel
   was never opened, or ONE open failed — every later event no-ops forever
   (`write_line` lines 44-46). No retry, no fallback, no counter: the exact
   silent `-22`/dead-channel failure mode the postmortem reconstructed.
3. **Open-attempt count is zero for the sink itself at startup.** The channel
   is opened only by the wired entry's `module_start`
   (`runtime_module_entry_r4.c:342`); events emitted before that point
   (constructor-time `constructor_begin`/`import_validation_begin` events from
   the entry's init-array walk) are dropped by design in every PRX generation.
4. **Not machine-readable.** Even when the lines land, they are
   human-readable text in a 2.3 MB mixed append-only log — there is no
   supplier jsonl the launch-10 verifier can parse for
   `bridge_lease_reg`/`bridge_lease_reg_spans` counts (LAUNCH9-RESULT.md
   artifact list: runtime-status.txt, UE4SS.log mirror, entry-log snapshot,
   xwp jsonl — NO supplier jsonl).

Net: the composition-6 registration counts observed in-process at launch-9
(lease REGISTRATION passed; the refusal came later at lease READ,
runtime-status session 234) left no attributable, parseable durable artifact.
That is the gap the staged sink closes.

## 3. The staged fix (see DIFF.md for the patch + wiring)

Additive second channel inside the SAME defining TU, so the signature, call
sites, entry-channel behavior, and the TU set stay untouched:

- Every `diagnostic_event(tag, value)` ALSO appends
  `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":N,"pid":P,"tag":"…","value":D,"value_hex":"0x…","t_ms":MS}`
  to **`/data/modbridge-supplier-diag.jsonl`** — a NEW file no other module
  writes (verified by repo grep; only planning docs mention the intended name).
- IO pattern copied from the console-proven xwp writer
  (`worker-writability-probe-build/src/record_sink.c` + `src/module_entry.c`):
  lazy `sceKernelOpen(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600)`, bounded
  partial-write retry (64 attempts), EVERY failure tolerated, never fatal,
  `open_errno` recorded and self-reported on the first recovered line. Unlike
  the baseline channel, a failed open is RETRIED (bounded at 64) so a
  transient early failure cannot permanently silence the channel — the
  host fixtures caught my first cap (8) locking the sink out permanently, and
  it was raised with that rationale recorded in the source.
- `t_ms` is CLOCK_MONOTONIC milliseconds — the same domain the vendored
  mfa_status writer uses for runtime-status.txt `t=` values
  (`worker-f-r2-fix/composition/vendored/modforge_activation.c:69-71`), so
  launch-10 can correlate supplier-diag lines with the status channel.
- Zero new imports: `sceKernelOpen/Write/Close` (already bound by this TU),
  `clock_gettime`, `getpid`, `__error` — each verified present in the frozen
  191-import contract AND already undefined in the shipped comp6
  `combined-supplier.o`, so the combined-object unresolved set the §3.3
  pre-check enforces is unchanged.

## 4. Honest limits

- Host logic proof + exact-toolchain compile rehearsal only; NO console
  behavior is claimed. Whether /data writes succeed in the real process is
  exactly what launch-10 will show (the xwp precedent proves the same
  open/append/O_NOFOLLOW pattern writes `/data/modbridge-exec-write-probe.jsonl`
  on console).
- If the sink cannot open the file AT ALL (all 64 attempts fail), the values
  are lost silently — the same failure surface as the xwp's primary channel
  (its mitigation is the UE4SS.log mirror; the task scoped THIS sink as
  strictly additive and forbade touching the entry channel, so no mirror is
  added here). The controller can request a mirror variant later if launch-10
  shows the file absent.
- The baseline `diagnostic_event(NULL, …)` dereference risk is unchanged
  (baseline `append()` never checked NULL; my sink's tag path is NULL-safe but
  the legacy line builder runs first and would crash exactly as before — no
  regression introduced, no pre-existing crash fixed, scope kept minimal).
- `supplier_write_failures` is counted but not externally visible (process
  lifetime, in-memory); seq gaps in the jsonl are the durable trace of dropped
  lines.
