# CHANNEL-POSTMORTEM — the composition-4 diagnostic channel and the silent -22

Worker: worker-diag-channel-postmortem (FINISH pass, 2026-10-04).
Host-only analysis + host-compiled tests. No console contact, no nested subagents.
Delivered entry audited (not modified): `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_module_entry.c`
(sha256 `1c041f8bef1607994ce905591f7e9ed91326465e45081da55e56d6ea2e501b80`, verified at every test run).

Source cites below are relative to `outputs/ps5-mod-bridge/native-runtime-startup/`
unless prefixed otherwise. Line numbers refer to the pinned files in
`SOURCES.sha256` order at analysis time.

---

## 1. Where the diagnostic channel writes

The composed PRX's diagnostic channel is a single, append-only file, opened once
and cached for the process lifetime.

**Path.** `generated/probe_config.h:3` in the bootstrap compile:

```c
#define PROBE_LOG_PATH "/data/townfall_runtime_aligned_entry.log"
```

**Open.** `diagnostic.c:56-62` (`diagnostic_open_retained`), called via
`diagnostic_open_channel` at `diagnostic.c:64-69`:

```c
return sceKernelOpen(PROBE_LOG_PATH,
    O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);   /* diagnostic.c:60-61 */
```

`static int channel_fd = -1;` (`diagnostic.c:14`) caches the descriptor —
including a NEGATIVE result: once an open fails, `diagnostic_open_channel`
keeps returning the cached failure (`diagnostic.c:66-68`) and the channel is
dead for the process.

**Line format.** Every event is
`[PPSA33286] modbridge-runtime-aligned-entry/v1 build=f6530c7905934e5a8cc201aa9400ebb0 <tag> value=0x…`
(`diagnostic.c:145-154`, marker from `generated/probe_config.h:4`). Both the
composition-3 PRX (b8bfae77…) and the composition-4 PRX (de372302…) carry the
identical pinned path and marker (strings inspection, CLAIM.json
`channel_where`) — the same file and marker as the known-good aligned-entry
probe module, so marker-level attribution between them is impossible
(CLAIM.json `channel_collision`).

**The write path is best-effort silent.** `write_line` (`diagnostic.c:39-54`)
returns without writing when `!diagnostic_ready()` (`:41-42`) or when the
cached fd is negative (`:44-46`), and stops on the first partial/failed write
(`:49-53`). `diagnostic_ready()` (`diagnostic.c:15-23`) only proves that the
three kernel import bindings are non-NULL — it says nothing about whether the
OPEN succeeds.

**Where the channel does NOT write:** `/data/plugins/UE4SS.log` and
`<mod root>/runtime-status.txt` belong to the NEW wired entry only (section 3);
`/data/townfall_supplier_composition.log`, named by the earlier postmortem
docs, is pinned by NO built binary (CLAIM.json `documented_path_was_wrong`).

---

## 2. Why composition-4's -22 would have been silent

`lifecycle/module_entry.c:57-58` (byte-identical logic survives into the wired
entry at `runtime_module_entry.c:303-304`):

```c
    if (!diagnostic_ready() || diagnostic_open_channel() < 0)
        return leave_entry(-22);
```

Three independent silences stack here:

1. **The error is dropped at the comparison.** `diagnostic_open_channel()`
   returns the failed open's negative code (the errno equivalent), and the `< 0`
   comparison discards it. module_start returns the fixed `-22` with zero
   diagnostic bytes anywhere.
2. **The channel that could report it is the thing that failed.** After the
   failed open, `channel_fd` stays negative, so any later
   `diagnostic_event`/`write_line` no-ops forever (`diagnostic.c:44-46`).
3. **There is no second sink in composition-3/4.** No UE4SS.log append, no
   status file, no console write exists on this path in the baseline
   `module_entry.c` — `:51-52` (-20, guard busy) and `:55-56` (-21, wrong
   state) are equally silent. Only the `PROBE_ACTIVE` re-entry (`:53-54`,
   `return 0`) is silent by design.

Consequence for the composition-4 console observation: a `-22` return is
byte-for-byte indistinguishable from "the module never ran". The controller
checkpoint `zcode_composition_startup_diagnosis` recorded "No UE4SS.log, no
runtime-status.txt, no diagnostic log" without a per-marker readback of
`/data/townfall_runtime_aligned_entry.log`, so the silent `-22` remains
UNFALSIFIED on console, alongside the separately proven missing activation
wiring (CLAIM.json `channel_never_verified`). The in-game path test
(`/zcode_path_test_target_run`) proved `/data/plugins/UE4SS.log` IS writable —
which is why the fail-open design targets that file.

Inherited quirks worth recording (not re-litigated here): the `-20` path at
`module_entry.c:51-52` returns WITHOUT `leave_entry`, leaving the guard set for
the process lifetime (frozen baseline behavior, preserved by the wired entry at
`runtime_module_entry.c:297-298`); `module_stop`'s guard rejection
(`module_entry.c:84-85`, wired entry `:338-339`) is equally silent and out of
scope.

---

## 3. Audit of the delivered wired entry's fail-open markers

The wired entry adds two sinks on top of the preserved probe lifecycle:

- `write_ue4ss_log_line` (`runtime_module_entry.c:166-192`): appends ONE line
  `modbridge module_start failed: <stage> code=<n> detail=<sanitized>` to
  `/data/plugins/UE4SS.log` (`:68`), opened with the proven flag set
  `O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW` mode `0600` (`:180-181`). Guarded by
  `if (!diagnostic_ready()) return;` (`:168-169`) — it never calls unproven
  imports.
- `write_status_failed` (`:198-219`): best-effort
  `mfa_status_write(&status, "failed", code, note)` (`:217`) through the
  vendored C status API — the RUNTIME-ACK point-4 sink.
- `runtime_failure` (`:221-226`) fans a failure out to all three: diagnostic
  event + UE4SS.log line + status `failed` line.

**Audit question: do these markers cover -20/-21/-22, or only post-lifecycle
failures? Answer: ONLY post-lifecycle.** Every `runtime_failure` call site is
downstream of `probe_lifecycle_start` returning 0: `:266`
(`mfa_activate_failed`), `:289` (`runtime_start_failed`), `:346`
(`runtime_stop_failed`). The early returns keep the baseline silence:

| path | wired entry lines | marker? |
|---|---|---|
| -20 entry-guard busy | `:297-298` | none |
| 0 idempotent ACTIVE re-entry | `:299-300` | none (by design, correct) |
| -21 wrong state | `:301-302` | none |
| -22 channel open failure / bindings null | `:303-304` | none |
| constructor failure (rc != 0) | `:319-327` | channel event only (`runtime_start_skipped`) |
| mfa_activate failure | `:266` | diag + UE4SS.log + status `failed` |
| runtime start failure | `:289` | diag + UE4SS.log + status `failed` |
| runtime stop failure | `:346` | diag + UE4SS.log + status `failed` |

**The -22 silence is by OMISSION, not incapacity.** In the
`diagnostic_open_channel() < 0` sub-case the bindings ARE proven, so
`write_ue4ss_log_line`'s own guard would pass and one call would mark the
failure. Host proof (`host-test/audit/`, receipt
`receipts/audit-test-pass-delivered-entry.txt`, 33/33 checks):

- **a1 (discriminating):** channel open forced to fail while
  `diagnostic_ready()` is true → `module_start` returns `-22` and ZERO marker
  bytes appear in UE4SS.log or the status file. If the entry called the marker
  helper on this path, the line WOULD appear (a5 proves the writer works the
  moment it is called).
- **a2:** bindings-unproven sub-case → `-22`, zero bytes (structural: nothing
  can be written by construction).
- **a3:** -21 wrong-state after a failed start → `-21`, zero bytes; positive
  control shows the diag channel WAS open on the -1 path
  (`EVENT runtime_start_skipped 0xffffffff`) — so the constructor-failure path
  reports to the channel only, which is acceptable but worth knowing.
- **a4:** recursive re-entry → inner `-20`, zero bytes; outer start unaffected.
- **a5/a7 (positive controls):** post-lifecycle failures DO mark — exact line
  observed: `modbridge module_start failed: mfa_activate_failed code=-2
  detail=MFA_E_NO_LOADLIST fake activation refusal loadlist.txt`, plus status
  line `state 1 failed -2 …`; with the UE4SS.log append refused (a7), the
  status sink still fires independently. Success path (a6) stays silent.

---

## 4. The exact 3-line patch for the silent -22 (proposal, host-proven, NOT applied)

Replace `runtime_module_entry.c:303-304` — exactly these two lines:

```c
    if (!diagnostic_ready() || diagnostic_open_channel() < 0)
        return leave_entry(-22);
```

with exactly these three lines:

```c
    int channel = diagnostic_ready() ? diagnostic_open_channel() : -1;
    if (channel < 0)
        { write_ue4ss_log_line("diag_channel_open_failed", -22, "diagnostic channel unavailable before wiring"); return leave_entry(-22); }
```

Properties (all host-proven, `receipts/patched-proof-pass.txt`, 8/8):

- preserves the short-circuit: `diagnostic_open_channel()` is never called with
  unproven bindings (the ternary), so the bindings-null sub-case behaves as
  delivered;
- `write_ue4ss_log_line`'s internal `!diagnostic_ready()` guard
  (`:168-169`) keeps the bindings-null sub-case a safe no-op even though the
  call is now unconditional inside the block (proof p2);
- the marker line observed (proof p1):
  `modbridge module_start failed: diag_channel_open_failed code=-22 detail=diagnostic channel unavailable before wiring`;
- `module_start` still returns `-22` (proof p1), and the success path is
  untouched (proof p3).

Analogous one-call additions (NOT proof-run, NOT requested) would close -20 and
-21 the same way, e.g. at `:297-298`:
`write_ue4ss_log_line("entry_guard_busy", -20, "re-entered while the entry guard is held");`.

The proof artifact is `host-test/audit/patched_proof_runtime_module_entry.c`
(delivered source + exactly the 3-line replacement, anchor-asserted) and
`host-test/audit/run_patched_proof.sh`. Nothing outside this worker directory
was modified.

---

## 5. Receipts and raw provenance

| receipt | result |
|---|---|
| `receipts/host-test-output.txt` | attempt 1, HOST-TEST-FAIL — raw provenance, kept unchanged; cause was a harness argv-indexing bug, not the code under test |
| `receipts/host-test-pass-patched-design.txt` | repaired harness, self-designed fail-open patch: 6 scenarios, 26 checks, PASS |
| `receipts/audit-test-pass-delivered-entry.txt` | delivered-entry audit: 7 scenarios (a1-a7), 33 checks, PASS |
| `receipts/patched-proof-pass.txt` | 3-line patch proof: 3 scenarios (p1-p3), 8 checks, PASS |

All host tests re-verify the staged source sha256 before compiling and never
touch the delivered files. Console behavior is NOT claimed anywhere; host
tests prove logic only.
