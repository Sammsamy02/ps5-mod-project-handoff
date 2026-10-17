# Agent Z14 — marker catalog completion + verdict-logic upgrade (l25 prep)

Date: 2026-10-08. Edit scope honored: ONLY
`outputs/2026-10-08-l25-prep/harvest/markers.json` and
`outputs/2026-10-08-l25-prep/harvest/harvest.py` were modified (plus this
report and synthetic test fixtures under `/tmp/z14-synth-l25/`). All existing
marker entries and behavior preserved; the update is additive.

## 1. Marker catalog: 28 -> 54 entries (54 cited, 0 PENDING)

Every new pattern was grepped from the actual candidate sources (not just the
reports); each entry's `source` field cites report section + file:line.

### Added from agent-O-listener-instrumentation.md (19 entries; prefix `[L25][DeleteListener]`)

| pattern (verbatim) | source file:line (typescan-r4/candidate/src/) |
|---|---|
| `[L25][DeleteListener] shutdown-listener registration begin (pre-marker)` | UnrealInitializer.cpp:567 |
| `[L25][DeleteListener] shutdown-listener registration returned in` | UnrealInitializer.cpp:573 |
| `[L25][DeleteListener] class-delete-listener registration begin` | UnrealInitializer.cpp:548 |
| `[L25][DeleteListener] class-delete-listener registration returned in` | UnrealInitializer.cpp:553 |
| `[L25][DeleteListener] Add enter (GUObjectArray=` | UObjectArray.cpp:489 |
| `[L25][DeleteListener] header pre: Data=` | UObjectArray.cpp:508 (also matches Remove header pre :598) |
| `[L25][DeleteListener] header post: Data=` | UObjectArray.cpp:554 (also matches Remove header post :609) |
| `[L25][DeleteListener] REFUSED (` | UObjectArray.cpp:493/:513/:522/:532/:566/:572 (codes 1-5 per :436-442) |
| `[L25][DeleteListener] RISK: capacity full` | UObjectArray.cpp:541 |
| `[L25][DeleteListener] fresh allocation` | UObjectArray.cpp:547 |
| `[L25][DeleteListener] ANOMALY: Num advanced by` | UObjectArray.cpp:559 |
| `[L25][DeleteListener] engine called FClassDeleteListener::NotifyUObjectDeleted` | ClassListener.cpp:174 |
| `[L25][DeleteListener] engine called FShutdownDeleteListener::NotifyUObjectDeleted` | ClassListener.cpp:232 |
| `[L25][DeleteListener] engine called FClassDeleteListener::OnUObjectArrayShutdown` | ClassListener.cpp:219 |
| `[L25][DeleteListener] engine called FShutdownDeleteListener::OnUObjectArrayShutdown` | ClassListener.cpp:243 |
| `delete-listener registration SKIPPED (variant B; unsafe long-term: stale object pointers not cleared)` | UnrealInitializer.cpp:557/:578 |
| `[L25][DeleteListener] Remove enter` | UObjectArray.cpp:585 |
| `[L25][DeleteListener] Remove REFUSED` | UObjectArray.cpp:588/:602/:615/:621 |
| `l25_dl_` (16 diag tags: add_begin/pre/post/add_done/refused/risk/remove_begin/remove_done, class_begin/class_done, shutdown_begin/shutdown_done, first_notify_class/first_notify_shutdown, shutdown_cb_class/shutdown_cb_shutdown) | UObjectArray.cpp:488-623; ClassListener.cpp:90-93; UnrealInitializer.cpp:549-575 — files: diag,aligned |

Report-vs-source verification note: agent-O's report section 5 lists the
family "all prefixed `[L25][DeleteListener]`" and every listed string matched
the candidate sources verbatim at the expected regions (report cited
:544-558/:561-579 call-site blocks and UObjectArray.cpp:481-576 callee;
actual send sites at the lines above). The report's item 18 Remove family and
item 3 Add enter were also included (task list + report grep list union).

### Added from agent-Q-copyin-probe.md (3 entries)

| pattern | source file:line (copyin-probe/candidate/) |
|---|---|
| `[Seeds][L25] copyin probe state after admission` | src/UnrealInitializer.cpp:1026 (events copyin_probe_liveness_state/_oracle_calls :1024-1025) |
| `[Seeds][L25] WARNING: copyin kernel-write probe dispatched AFTER admission retirement` | src/UnrealInitializer.cpp:1047 (events copyin_probe_fire_post_admission/_addr_lo/_addr_hi/_len :1043-1046) |
| `copyin_probe_` (liveness + bridge-side state tags: copyin_probe_state/gate_armed/bank_retired/parent_fd_open/sinks_open/nonce_used/attempts_total/gate_released/attempts_late_total; substring-matches init-phase pool_copyin_probe_* too) | src/UnrealInitializer.cpp:1024-1046; lifecycle/runtime_start_bridge.cpp:1112-1118/:1131-1132 — files: diag,aligned,ue4ss |

Caveat recorded in the entry meaning: agent-Q delivered overlay+patch not yet
adopted by the build (WIRING.md steps pending), so absence of these markers
proves nothing until adoption.

### Added from agent-P-phase-tags.md (4 entries)

| pattern | source file:line (typescan-r4/candidate/src/) |
|---|---|
| `[l25] event loop started` | UE4SSProgram.cpp:1605 (diag event event_loop_started :1606) |
| `[l25] on_program_start entered` | UE4SSProgram.cpp:1458 (diag event on_program_start_entered :1459) |
| `[l25] marshal hook first invocation (carrier fired)` | LuaMod.cpp:4869 (diag event r3_marshal_first_invoke :4871) |
| `[l22] ExecuteInGameThread queued depth=` | LuaMod.cpp:5673 (diag event eigt_queue_depth :5675) |

### Placeholder upgrades (existing entries kept, sources resolved to cited)

- `AddUObjectDeleteListener`, `listener-` (agent-O placeholders) -> cited,
  noted as symbol-name/stray catch-alls superseded by the exact family.
- `phase=`, `[l25]` (agent-P placeholders) -> cited with file:lines; `[l25]`
  is now the documented catch-all for the family above.
- `r3_marshal_pumped` -> appended the l25 implementation citation
  (LuaMod.cpp:4920, agent-P).
- `_meta.pending` now lists ONLY agent-T-klog-capture.md (klog ranked dirs in
  harvest.py); `_meta.known_today_citations` gained agent-O/P/Q.

Final count: **54 entries, 54 cited, 0 PENDING marker sources.**

## 2. harvest.py verdict-logic changes (backward compatible)

1. `init_passed_528`: NEW rule — a registration-returned post-marker
   (`shutdown-listener registration returned in` / `class-delete-listener
   registration returned in`) appearing after the LAST `Adding shutdown
   listener` yields verdict YES with an explicit why ("the
   AddUObjectDeleteListener stall call RETURNED - init passed :528 even
   without '[l22] Lua mod started'"), even when mod-start is absent. All
   pre-existing outcomes unchanged (generic same-session-after YES,
   modbridge-append NO, last-line-stall NO, no-marker NO). Variant-B SKIPPED
   lines do NOT trigger the new rule (no call was made); they keep the
   generic documented behavior and are flagged by their own field.
2. New `listener_verdict()` -> four new VERDICT rows in the report and in the
   `[harvest] verdict:` summary line:
   - `listener registered?` YES/NO/UNKNOWN (registration-returned markers, line numbers)
   - `listener refused (code)?` with codes decoded (1 null GUObjectArray,
     2 exception/layout-missing, 3 insane header, 4 duplicate,
     5 GMalloc unbound for growth) + line numbers
   - `variant-B skip detected?` YES when the SKIPPED line appears
   - `engine called listener?` TRUE when any `[L25][DeleteListener] engine
     called F*...` marker appears (per-listener hit counts; direct
     engine-dispatch proof)
   On pre-l25 logs these read NO with honest whys ("pre-l25 build, or the
   call hung/refused"), never UNKNOWN unless UE4SS.log itself is missing.
3. Dry-run skeleton lists the four new auto-fill rows.
4. BUG FIX (required for the above): the live path called
   `verdict(marker_results, ue4ss_lines, klog_lines)` against the
   2-parameter `verdict(ue4ss_lines, klog_lines)` — a latent TypeError the
   dry-run-only host testing never reached. Call site fixed to
   `verdict(ue4ss_lines, klog_lines)` with an explanatory comment.
5. Constants for the L25 strings live next to SHUTDOWN_LISTENER with their
   agent-O citations; markers.json remains the display catalog, harvest.py's
   verdict logic uses its own exact constants (no behavior depends on the
   JSON for verdicts).

## 3. Proof: dry-run + regression + synthetic

- `python3 -m py_compile harvest.py` -> OK.
- `python3 harvest.py --label z14-dryrun-proof --dry-run` -> exit 0; prints
  the 4 would-run curl commands, klog pickup (resolved
  /tmp/l24-harvest/klog-20261008-133536.log — local read only), the full
  54-entry marker table (all `source=cited`), and the report skeleton
  including the four new VERDICT rows. Zero files written (verified: no
  harvest-z14-dryrun-proof-* / HARVEST-z14-dryrun-proof.md exist).
- Regression on the REAL `/tmp/l24-ab2-ue4ss.log` (602 lines, 1 session,
  last timestamp 2026-10-08 14:07:39): init=NO with the exact pre-existing
  why ("'Adding shutdown listener' is the LAST non-empty line (line 602) -
  the l24 stall signature..."), mod-started=NO, admission=OK, and the same 4
  marker hits as before (Adding shutdown listener / TypeScan scan-complete /
  LayoutPreflight checked= / outcome=admitted). New listener fields read
  NO/NO/NO/NO with honest pre-l25 whys. Old verdicts unchanged.
- Synthetic logs in /tmp/z14-synth-l25/ exercising the new markers:
  - `ue4ss-success.log` (no [l22] mod-start anywhere): init=YES via the NEW
    registration-returned rule (line 14 = last shutdown listener; returned
    marker at line 18); registered=YES (lines 12, 18); engine called
    listener=YES (FClassDeleteListener::NotifyUObjectDeleted line 22,
    FShutdownDeleteListener::NotifyUObjectDeleted line 25); refused=NO;
    variant-B=NO; full live-style VERDICT section rendered correctly
    (generate_report dry=False path, no network).
  - `ue4ss-refused.log`: init=YES (call returned after clean refusal —
    init continues), refused=YES codes decoded (line 4 code 3 "insane
    header", line 9 code 1 "null GUObjectArray").
  - `ue4ss-variantb.log`: variant-B skip detected=YES (lines 2, 4);
    registered=NO (correct — variant B makes no call); init=YES via the
    pre-existing generic rule (SKIPPED is a same-session line after the
    marker; variant B genuinely continues past :528).
  - `diag.jsonl`: `l25_dl_` 5 hits, `copyin_probe_` 4 hits (3 diag + the
    ue4ss state line), `r3_marshal_pumped` 1 hit — diag/aligned routing
    works.

## 4. Honest limits

- No console contact, no builds, no live harvest run (root runs those later);
  all testing is --dry-run + direct function invocation per the host-only
  contract.
- agent-Q markers exist only in the un-adopted copyin-probe overlay; their
  absence in a real l25 launch is NOT evidence until WIRING.md adoption lands.
- agent-T (klog ranked dirs) remains the sole open PENDING item, affecting
  only KLOG_RANKED_DIRS, not the marker table.
