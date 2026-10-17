# REPORT — worker-insession-diag (2026-10-05/06)

Bounded unit: the DIAGNOSTIC half of the in-session command channel (one-game-
session reuse). The Lua-reload half stays PROPOSAL ONLY (worker-reload-support/
SURVEY.md §3(b) already names the bounded reload shape; nothing staged here
touches it). No composition, no console dispatch, no build, no game claims.

## 0. What was staged (complete file list)

```
outputs/2026-10-04-zcode-team/worker-insession-diag/
├── REPORT.md                                   this note
├── staging/
│   ├── insession_cmd_core.h        grammar-v1 API + constants (C/C++ dual)
│   ├── insession_cmd_core.c        strict scanner: pure, fixed buffers, no I/O
│   ├── insession_cmd_poll.h        the ONE entry point + call-site law
│   ├── insession_cmd_poll.cpp      bounded poll, frozen imports, rename-consume
│   └── ue4ssprogram-eventloop-hook.diff   the +5-line loop hook, NOT APPLIED
├── fixtures/
│   ├── host_shims.c                POSIX shims + failure injection (host only)
│   ├── test_insession_host.c       s0-s7 scenario driver (host only)
│   ├── throttle_host.c             idle-cost fixture (host only)
│   ├── run_host_tests.sh           harness + receipt writer (precedent-shaped)
│   └── receipts/host-test-pass.txt PASS receipt (sha-pinned sources)
```

Read-only references (never modified): worker-reload-support/SURVEY.md,
worker-diagnostic-sink/staging/diagnostic.c (+ its fixtures/include tree),
worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp, worker-console-batch-runner/
console_batch.py, the supplier-contract.json candidates named in §2.

## 1. Grammar v1 (exact, frozen for v1)

Intake file: `/data/modbridge/intake/in-session-cmd.json` (fixed path; host
STORs it — the proven e2e intake dir). Consumed file:
`/data/modbridge/intake/in-session-cmd.json.consumed` (POSIX rename; the e2e
`.consumed` convention, console_batch.py cmd_gone_ok).

```
accepted  {"cmd":"noop"}
accepted  {"cmd":"diag","tag":"<1..48 chars of [A-Za-z0-9_.-]>","repeat":<1..1024>}
rejected  everything else
```

- Key order flexible (real object scanner, not a literal match); whitespace
  around the single object tolerated; escapes NOT implemented (a tag needing an
  escape is rejected — allocation-free and injection-proof).
- STRICT: unknown key, duplicate key, unknown cmd value (e.g. "reload"),
  out-of-range repeat, bad tag charset, escape, trailing bytes, oversize
  (>512 B) — all REJECT: loud log line, file consumed, NOTHING executed,
  never re-read. A typo must never silently no-op.
- Effects (complete list for v1):
  - diag  → ONE `diagnostic_event("insession_<tag>", repeat)` call (the sink
    is already open, append-only, same PRX; one sdiag row + one entry-channel
    line, `value` carries `repeat` — one bounded row per command, never
    `repeat` rows) + ONE UE4SS.log line.
  - noop  → ONE UE4SS.log ack line only.
  - reject → ONE loud `INSESSION-CMD REJECTED (...); consumed, not executed`
    UE4SS.log line.
- **NO command in v1 writes engine state of any kind.** The reload half of the
  SURVEY (queue_reinstall_mod_by_name route) is NOT implemented here.

## 2. The zero-new-imports proof

The poll TU uses exactly these external symbols: `sceKernelOpen`,
`sceKernelClose`, `read`, `rename`, plus `strcmp`/`strlen` (parser core) and
the same-PRX `diagnostic_event`. Membership in the composed candidate's
196-strong import contract (`supplier-contract.json`,
`counts.required_strong=196, missing_strong=0`), verified by decoding the
contract JSON of FIVE candidates across the lineage:

| candidate | required_strong | missing_strong | sceKernelOpen | sceKernelClose | read | rename | strcmp | strlen |
|---|---|---|---|---|---|---|---|---|
| worker-prx-link-gaps/prx-candidate-composition2-1 (the original 196) | 196 | 0 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| launch10-build/prx-candidate-l10-1 (shipped) | 196 | 0 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| launch10-build/prx-candidate-l10-2 (shipped) | 196 | 0 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| launch11-combined/prx-candidate-l11-2 | 196 | 0 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| worker-d1-determinism-diagnosis/prx-candidate-composition6-4 | 196 | 0 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

- The l11-2 contract records provider identity per symbol (the obfuscated
  contract evidence): `read`/`rename` → libkernel module / **libScePosix**
  library with catalog-mapping locators (`/mapping/read/0`, `/mapping/rename/0`);
  `sceKernelOpen`/`sceKernelClose` → libkernel/libkernel with
  `successful_wrapper_sha256` (the live-proven wrappers diagnostic.c already
  binds and `diagnostic_ready()` gates). `identity_complete: true` on l11-2.
- `sceKernelRead`/`sceKernelRename` are NOT in the contract and are NOT used.
- The r14 precedent is the same law: geometry-diag merged through the EXISTING
  diagnostic channel, "No new sink, no new imports, no ordering change"
  (runtime_start_bridge_r14_diag_spanpub.cpp:47-52, extern decl :119-121).
- Adding these TUs adds REFERENCES to already-present UND imports; the dynsym
  import set must come out at exactly 196 again. Per-build NID-aware audit
  stays mandatory at composition (PLOADER-IMPORT-ANALYSIS.md §4) — drift is a
  stop, not a judgment call.
- The consumption mechanism needed NO fallback: `rename` (proven atomic
  consume) is in-contract, so the read+truncate/read-and-rewrite alternatives
  were not needed. The idle poll is an open-try (O_RDONLY|O_NOFOLLOW) — the
  SURVEY's preferred shape; `stat` was not needed (it is also in-contract, so
  the SURVEY's stat caution is moot for the current candidate, but open-try
  keeps the idle path to ONE syscall and matches the SURVEY's wording).

## 3. The safety case (single-threaded event-loop discipline)

- **Which thread:** exactly ONE call site, inside `UE4SSProgram::update()`'s
  loop body (wpd1 staging copy, sha256 cb6cb14a...): started as its own thread
  at :709 (`m_event_loop = std::jthread{&UE4SSProgram::update, this}`), loop
  thread pinned at :1580 (`m_event_loop_thread_id`), the same thread that runs
  the PROSPERO event-batch executor (:1677-1707, under `m_event_queue_mutex`)
  and mod `fire_update` (:1734-1744). This is the loop's OWN serialization law
  — the poll adds no thread, no lock, no queue interaction, and never calls
  queue_event/queue_* (nothing marshals in v1). The `queue_*` family's
  `is_event_loop_thread()` marshal (:2364+) is the same law the loop already
  obeys; the poll is ON the marshal target thread by construction.
- **Placement** (staged diff, NOT applied): after the Unreal-init gate
  (:1661-1665) and pause gate (:1667-1671) — paused/unhealthy ticks never
  poll — after mod updates (:1744) so Lua update work is never starved, before
  the 5 ms sleep (:1754) so the poll can never lengthen an engine step.
- **Bounded:** internally throttled — one integer increment per tick, one
  open-try per `INSESSION_CMD_POLL_EVERY`(=100) ticks (~0.5-1.5 s cadence at
  the 5 ms floor). Read into a fixed 512 B buffer with an 8-attempt bound;
  oversize rejects without parsing.
- **Never fatal:** fully noexcept, no allocation on the dispatch path, fixed
  buffers; the PROSPERO loop try/catch terminates the process on ANY escape
  (:1759-1771), so the poll may never throw — diagnostic.c's tolerance law.
- **Single-writer diagnostics:** all startup `diagnostic_event` emit sites
  precede the loop; in-session the poll thread is the ONLY caller —
  diagnostic.c's no-lock single-writer assumption is preserved, not relaxed.
- **Consumption semantics — at-least-once, loudly:** execute THEN rename. A
  failed rename logs `INSESSION-CMD CONSUME-RENAME FAILED` and leaves the file;
  the command may re-execute on a later poll (host reconciles via FTP — host-
  tested in s7). Duplicates are observable and harmless for diagnostics; a
  silent loss would not be. One intake slot; host stages the next command only
  after the `.consumed` evidence appears (the e2e cmd_put stale-guard: never
  overwrite a stale intake).
- **Torn-STOR race (honest bound):** a poll can open a half-STORed file; the
  strict parser rejects it, the file is consumed with a loud line, the command
  is lost. Host discipline that avoids the race entirely (recommended in the
  admission note): STOR to `in-session-cmd.json.next`, then FTP RNTO to the
  intake name (atomic server-side rename), or stage pre-launch — the poll
  tolerates a pre-existing file once the pump is alive.

## 4. Host-test results (host logic proof ONLY — no console claim)

`fixtures/run_host_tests.sh` compiles the STAGED units together with the REAL
precedent-pinned diagnostic.c (sha f5e96c41..., the r-s1 copy) against POSIX
shims (worker-diagnostic-sink fixture pattern), receipt written to
fixtures/receipts/host-test-pass.txt.

- **s0 grammar table: 37/37 cases** — accept/reject for: order-free diag,
  repeat boundaries (1/1024 pass; 0/1025/10000/100000/negative/string/leading-
  zero reject), tag boundaries (48-char pass; 49-char, empty, space, slash,
  non-ASCII, escape reject), noop strictness (extra keys reject), unknown cmd
  ("reload", "DIAG" case) reject, unknown/duplicate keys, empty/garbage/
  trailing-bytes/array/null, and NUL-independence (scanner stops at n).
- **s1 idle:** 5 polls with no file → exactly 5 open-tries, zero log lines,
  zero files.
- **s2 diag end-to-end (REAL staged diagnostic.c):** sink row
  `{tag:"insession_probe1", value:7, seq:1}`, entry-channel line
  `... insession_probe1 value=0x00000007`, intake GONE, `.consumed`
  byte-identical, exactly 1 rename across 3 polls.
- **s3 noop:** ack line only, sink untouched, consumed.
- **s4 malformed:** exactly one loud REJECT across 3 polls (never re-read),
  sink untouched, consumed.
- **s5 unknown cmd "reload":** REJECTED ("unknown cmd"), consumed, NEVER
  executed (no sink row, no dispatch) — the v1 safety core.
- **s6 oversize (600 B):** rejected without parsing, consumed.
- **s7 rename-failure injection:** executes, loud CONSUME-RENAME FAILED, file
  stays, re-executes once on the next poll, then consumes — the documented
  at-least-once semantics.
- **throttle (POLL_EVERY=100):** 250 ticks → exactly 2 opens, 0 renames.
- **s8 strict python validation:** all 3 supplier rows strict-parse
  (marker/schema/seq/pid/value/value_hex/t_ms, tags and values in execution
  order, t_ms nondecreasing, no "reload" tag ever); entry log 3 lines;
  test log 8 lines, exact inventory.
- **Totals: 112 C checks, 0 fails; throttle PASS; python validation PASS.**
  Receipt result: PASS (2026-10-06T02:13:12Z), sources sha-pinned in the
  receipt.

## 5. Admission note (for the integrating worker; NOT executed by this worker)

1. **Precondition:** the loading regression fix and a working runtime come
   first — none of this is useful before the pump is alive.
2. **Integration is two hunks + three new units** (staging/
   ue4ssprogram-eventloop-hook.diff, NOT applied): the include, the one
   `insession_cmd_poll();` call (PROSPERO-guarded), and the new files. The
   poll TU's console branch includes <DynamicOutput/Output.hpp> and
   <UE4SSProgram.hpp> (both already in the composition include set: wpd1:30,
   ue4ss_backend.cpp:3) and widens byte-by-byte (wpd1:1807-1822 discipline);
   the fixture cannot typecheck that branch (host uses the
   INSESSION_CMD_HOST_TEST branch) — first composition compile is its proof.
3. **Per-build import audit stays mandatory:** the derived contract must come
   out at exactly 196 strong again; any drift stops admission.
4. **Host staging discipline (when this ships):** never overwrite a stale
   intake (e2e cmd_put guard); prefer STOR `in-session-cmd.json.next` + FTP
   RNTO atomic rename, or pre-launch staging; fetch `in-session-cmd.json`
   (must be GONE) + `.consumed` readback + sdiag/UE4SS.log lines as the per-
   command evidence triad — the e2e cmd_gone shape.
5. **Deliberately absent (v1 scope guard):** no reload command, no engine-state
   command, no second call site, no configurable path, no queue interaction.
   The reload half remains the SURVEY's proposal (`queue_reinstall_mod_by_name`
   / Lua `RestartMod`), which would be a NEW grammar version with its own
   review — this unit's grammar table rejects any such command today by
   construction.

## 6. Honest bounds

- Host logic proof only: parser, dispatch decisions, file-op sequence, throttle
  and failure semantics are proven; CONSOLE behavior (FTTP STOR reachability
  mid-session, libkernel read/rename binding in the game process, UE4SS.log
  line shape on device, timing under real frame load) is NOT measured and is
  not claimed anywhere.
- The console-only branch of the poll TU (Output::send + StringType widen) is
  12 lines mirroring wpd1 usage verbatim; it compiles only at composition.
- Line numbers cited are from the exact staging copies named in §0/§3 and drift
  if those files change.
