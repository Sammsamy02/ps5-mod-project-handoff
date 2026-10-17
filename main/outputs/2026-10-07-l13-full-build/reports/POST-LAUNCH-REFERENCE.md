# l13 post-launch log reference

Prepared 2026-10-07 from retained source and static build metadata only. **No fresh launch results are claimed here.** This worker performed no console contact or source change. Root owns the approved PRX upload/new klog and all console actions.

Candidate identity: SHA256 **`cd58fa12c1864eb11f3f1eb72a66361c8f7468f9b349629e98261a3aac6a705f`**, **30,907,159 bytes**, SELF magic `4f153d1d` ([candidate manifest](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-full-build/deployment/candidate-manifest.json)). This identifies the intended artifact, not a claimed live readback.

Use the current runtime-status `session` and the matching supplier sequence beginning at `module_start_entered`; PIDs repeat across reboots. Do not select the largest historical PID. Keep raw logs, including NUL bytes, and use a separate normalized display for searches. An incomplete capture cannot establish the absence of a later event. Module/injector success precedes asynchronous Unreal initialization.

## Admission: exact l13 patterns

Source: [UnrealInitializer.cpp:799](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-local-admission/units/UnrealInitializer.cpp:799), [admission block:1035](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-local-admission/units/UnrealInitializer.cpp:1035), [production policy](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-local-admission/units/PoolAdmissionEngine.hpp:10).

| Literal pattern / fields | Meaning and limit |
|---|---|
| `[Seeds][L13] pool admission: one total 60000ms budget (including operations and retry sleeps); mode=...; VirtualQuery first; copyin only on inconclusive VQ` | Admission reached its settings/start announcement. The budget started earlier, before metadata preparation. All modes retain VQ-first proof. |
| `[Seeds][L13] VirtualQuery needed=[0x...,0x...) region=[0x...,0x...) protection=0x... flags=0x...` | Successful query reply. Record **needed start/end**, **region start/end**, protection and flags. Ends are exclusive. This line alone is not admission. Source requires each queried byte to be contained, `protection & 1` and `flags & 0x10`, continuing across regions if needed. |
| `[Seeds][L13] VirtualQuery address=0x... rc=0x...: unavailable/inconclusive` | Nonzero API return, permitting copyin fallback; not proof the address is unmapped. The production adapter does not classify a raw error as `Unmapped`. |
| `[Seeds][L13] admission not ready; retry in ...ms within shared deadline` | An unavailable result is being retried. Sleep is at most 5000 ms or remaining budget. No separate per-attempt 60-second budget. |
| `[Seeds][L13] shadow fallback considered after VQ/copyin unavailable: declined; image identity cannot prove live heap readability; deadline-aware shadow byte source unavailable` | Expected refusal to use disk identity as heap proof. **Not admission.** Usually at most once per admission session. Absence is normal if VQ/copyin succeeds or `mode=oracle`. |
| `[Seeds] slot 1 fname pool 0x...: admitted via ... (all exact needed ranges readable; elapsed ...ms)` | Pool admission pass. The source is exactly **`virtual-query`**, **`copyin-owned-read`**, or **`virtual-query + copyin-owned-read`**. |
| `[Seeds] FName pool validated at ... (CurrentBlock ..., Blocks[0] 0x...; source: ...; 'None' anchor ok; 'None' id 0x..., 'ByteProperty' id 0x...; elapsed ...ms)` | Companion validation result. Preserve addresses/IDs/source/elapsed. This proves the checked exact anchor/entry ranges, not all 128 KiB of a block or completed engine startup. |
| `[Seeds] REFUTED: ... Terminal for this launch; derive afresh with new evidence, never replay.` | Completed refusal; preserve the entire reason. Relevant reasons include `one total admission deadline exhausted`, `VirtualQuery proved no CPU-read protection`, `VirtualQuery proved uncommitted reservation`, `range excluded by recorded fault`, `copyin fault; range excluded and shadow barred`, and metadata/anchor/entry mismatch. |

There is no emitted VQ call-begin tag. Both query log shapes occur **after** the SDK returns; a missing VQ line does not distinguish an unentered call from an unreturned call. “Start/end” above are address bounds, not timing events.

## Copyin diagnostics, only if that route is needed

Source: [bridge:962](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-local-admission/units/runtime_start_bridge_r14_diag_spanpub.cpp:962).

- `pool_copyin_parent_raw/_errno/_flags`: retained parent-directory setup. A nonnegative descriptor with normalized errno zero is setup evidence, not a memory read.
- `pool_copyin_probe_addr_lo/_hi`, `pool_copyin_probe_len`: exact requested address/length; combine address as `(hi << 32) | lo`. `pool_copyin_path_len/_word` records the unique sink path. `pool_copyin_open_flags/_mode/_dirfd/_raw/_errno` and `pool_copyin_readopen_raw/_errno` distinguish write-open from readback-open failure.
- `pool_copyin_probe_fresh`: **1 = complete successful capture**, **0 = source-address fault**, **2 = unavailable/transport/deadline failure**, **3 = invalid range**. `_rc` is the raw last result encoded as uint32; interpret signed SDK errors correctly. `_errno` is normalized. `_sink` and `_stop` are landed bytes; matching length alone is insufficient if reopen/readback failed.
- `pool_copyin_probe_fault_off` plus `pool_copyin_fault_addr_lo/_hi`: EFAULT at the attempted source offset; excludes that requested range. An open failure does not establish EFAULT of game memory.
- `pool_probe_op_rc`, `pool_probe_stop_addr_lo/_hi`, `pool_probe_op_pagealign`: a short write. Pagealign tests a **16 KiB** boundary (`0x3fff` mask); writes are bounded to **4 KiB** chunks. Short count alone is not success or a proven source fault; follow the terminal result.

l13 disables eager startup A/B/C/D oracle work. Missing legacy `io_ctrl_d` is not a regression. A VQ-only admission can correctly have no per-probe sink/read diagnostics.

## Runtime, input and menu ladder

| Pattern | What it proves |
|---|---|
| `runtime_start_ok`, `module_start_return` | Early runtime/module startup only. |
| Runtime-status `state 2 failed 1 <reason>`; `Stopping mod 'TownfallItemSpawner' for uninstall` | Startup failure/mod teardown. Files still present under Mods do not mean the mod remains active. |
| Runtime-status `state 2 failed 0` plus live mod/startup evidence and no current-session terminal/stop | Corroborates continued runtime; absence of a stop in a partial log is insufficient. |
| `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`; `Event loop start` | Mod script reached its banner; input-processing loop started. These are separate milestones. |
| `pad_input_deferred=6`, `pad_input_deferred_error=0x80920004`, `pad_input_deferred_input_gate=1` | First standard-port open deferred. This does not prove the later borrow failed. |
| `pad_open_class=0`, `pad_retry=N`, `pad_retry_failed=<raw error>` | Poll-side acquisition attempt. Intervals are 200→3200 **polls**, not milliseconds. |
| `pad_borrow_begin=<user>` → `pad_borrow_returned=<raw result>` | Delayed GetHandle attempt/completion. Begin is before helper-thread creation; begin without returned is incomplete evidence, not uniquely a firmware hang. Returned nonnegative (including zero) is a handle candidate; high-bit raw values require signed interpretation. |
| `pad_open_class=1` → `pad_available=<handle>` → `pad_first_state_read=<buttons>` | Borrow adopted; then at least one connected, successful ReadState. Initial sample is baseline and creates no key edge. |
| `pad_key_edge=118` / `0x76` | F7 edge from the configured chord reached Handler. Still not proof that the queued menu action ran. Edge logs cap at 60 per process, followed by `pad_key_edge_silent=60`. |
| `[R3] FuncPtr-marshal armed on carrier ...` or `FuncPtr-marshal event carrier armed: ...` | Hook armed, not yet proof it fired. `event carrier scan: 0 override(s)...` is different from no scan line. |
| `[R3] marshal is the game-thread pump (initializer deferred its EngineTick registrations)` | Marshal callback fired far enough to publish its thread identity. `marshal not armed: initializer owns the game-thread pump (pump_pending=false)` is an alternate ownership path, not automatic failure. |
| `closures will queue until a carrier arms`; `ExecuteInGameThread: FuncPtrMarshal requested but the carrier is not armed yet; the closure will await arming`; `closure enqueued but the ... pump is not registered` | Delivery blocked/pending after enqueue; distinguish from absent raw input. |
| `[TownfallItemSpawner] Menu created.` / `UI creation failed: ...` | UI construction path completed or failed. `Menu created.` is not proof of visible overlay. No standalone `AddToViewport` log is emitted by this mod. |
| `Spawned <name> xN ID=<id>` / `Failed at <stage>. See UE4SS.log. ...` | Spawn workflow result; user-visible pickup remains separate evidence. |

Input code is unchanged in l13. [Actual retained program:655](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp:655) initializes Unreal before creating its update thread at709; polling is at1732. If admission/startup fails, pad retry may never execute. Detailed borrow/callback flow is in [INPUT-NEXT-BUILD.md](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-local-admission/reports/INPUT-NEXT-BUILD.md).

## Clock and checklist caveats

Admission `elapsed ...ms` uses `std::chrono::steady_clock` from budget creation, including metadata/settings, operations and sleeps. It is not time since launch or since the announcement. The **60,000 ms limit is a scheduling deadline**: native calls, diagnostics and mandatory cleanup cannot be preempted, so observed wall duration can exceed it. The outer startup deadline/cancellation is separately authoritative. Do not subtract supplier `t_ms`, UE4SS wall timestamps, klog times or host capture timestamps from each other without confirming clock origins/offsets; use source elapsed values and within-stream deltas. Record pauses/gaps and capture start/stop.

The inherited [SMOKE-CHECKLIST.md](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l13-local-admission/reports/SMOKE-CHECKLIST.md) remains titled L12. Apply these corrections when reading it; this report changes no checklist or console config:

- **Row N:** exact `Unreal initialization startup deadline exceeded` with `state 2 failed 1`, no admission/refutation, and no rescue result is an **owner deadline before a completed admission verdict**. It does not prove VQ/shadow failed. If this occurs after some l13 stage output, retain that last reached stage rather than forcing the narrower “before rescue” description. A l13 `[Seeds] REFUTED ... one total admission deadline exhausted` is a completed local admission refusal, **not row N**.
- **Rows J/K:** use l13 fault/refusal strings above. No CPU-read protection/uncommitted coverage is evidence against that range; it does not alone prove the pool moved. A raw VQ API error is inconclusive.
- **Rows H/I/L/M and old success recipe:** shadow admission, shadow-file repair, `30 rounds x 5s`, `VirtualQuery coverage`, `span-membership` source, and oracle-mode-disables-VQ expectations are obsolete for l13. Shadow cannot admit, and all modes retain VQ-first proof.
- **Rows B/C:** missing visible UI does not uniquely prove a ProcessEvent slot error. Missing chord response requires the pad→edge→carrier ladder; TOUCH_PAD cannot solve acquisition failure. Do not prescribe config changes from this reference.
- **Rows A/D–G:** retain outcome distinctions and exact errors. For the currently scoped **GATE-1 test**, source-named admission + completed Unreal startup + continued mod lifetime is the milestone. Menu/navigation/spawn are recorded separately; menu absence alone does not negate admission, and admission alone does not mean the overall spawner goal is met.

Suggested local search terms: `[Seeds][L13]`, `VirtualQuery`, `admitted via`, `FName pool validated`, `elapsed`, `REFUTED`, `startup deadline`, `Stopping mod`, `Event loop start`, `[R3]`, `TownfallItemSpawner`, and supplier tags beginning `pool_` or `pad_`. Interpret the full lines and same-session sequence, not a grep count alone.
