# l18 saved-evidence timeline

The largest visible delay is before PRX entry: roughly65 seconds by cross-clock reconstruction. Once entry is reached, runtime wiring reports success after4.082 seconds, and the logged pool/FName path succeeds before the startup refuses unresolved build axes at21:09:44 UTC. This is analysis only; nothing was changed.

## Clock and launch boundaries

Use the latest1076-event segment [supplier-diag-current-launch.jsonl](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/harvest-20261007T211125Z-6b6de9/supplier-diag-current-launch.jsonl); prior appended PID106 data belongs to older boots. Supplier `t_ms` and runtime-status session106/447503 are CLOCK_MONOTONIC milliseconds. Supplier implementation explicitly names that shared clock at [diagnostic.c](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-diagnostic-sink/staging/diagnostic.c:194). UE4SS dates are local America/Chicago (UTC−5 on this date), whole seconds. Klog capture connection stamps are host UTC, not timestamps on every received kernel line. The full capture continues beyond harvest; use frozen supplement for conclusions bounded to21:11:36.

Fresh boot reconnect is klog:2524 at21:02:50UTC. Authorized loader reload is confirmed PID105 at21:07:35.567UTC in [RESULT.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/authorized-reload-20261007T210734Z-8687fd/RESULT.json). Game title activity wall timestamp is klog:5135 at21:08:33.696UTC; game PID106 EXEC is klog:5172 with `<378601 msec>`. These are distinct events. Subtracting this EXEC counter from supplier entry443465 yields64.864s (1.081min), **medium confidence** because the saved evidence does not explicitly define EXEC’s clock epoch. Game activity→last UE4SS whole-second date is about70.3s. Do not claim a precise per-poll split or exact PRX load wall time.

## Observed and inferred stages

| Stage | Saved time / elapsed | Evidence and limit |
|---|---|---|
| module_start_entered | monotonic443465ms; entry+0ms | supplier seq1, value0 |
| full_runtime_constructors_complete | monotonic444185ms; entry+720ms | supplier seq481, value0 |
| worker_entry | monotonic444187ms; entry+722ms | supplier seq483, value0 |
| runtime_wiring_begin | monotonic444189ms; entry+724ms | supplier seq485, value0 |
| copyin_startup_elapsed_ms | monotonic445264ms; entry+1799ms | supplier seq975, value992 |
| code_image_cache_ok | monotonic447432ms; entry+3967ms | supplier seq1009, value1 |
| code_image_elapsed_ms | monotonic447450ms; entry+3985ms | supplier seq1021, value2130 |
| pad_input_deferred | monotonic447513ms; entry+4048ms | supplier seq1059, value6 |
| runtime_start_ok | monotonic447547ms; entry+4082ms | supplier seq1064, value0 |
| module_start_return | monotonic447548ms; entry+4083ms | supplier seq1066, value0 |
| worker_sleep_complete | monotonic449231ms; entry+5766ms | supplier seq1068, value0 |
| buffered_lua_result | monotonic449242ms; entry+5777ms | supplier seq1076, value0 |
| Pool admission |2297ms from shared cache budget origin | UE4SS:57; all13 checks pass on first recorded evaluation; no retry line, copyin oracle0 |
| FName signature and native call | total2355ms from same origin | UE4SS:61–70; authenticated file branch for live VQ0x4; ByteProperty id3 roundtrip |
| Stop |21:09:44UTC, whole-second date | UE4SS:71–73 and runtime-status state2 failed1; layout axes refused before engine readiness waits |

The2130ms cache is included in admission2297ms and FName2355ms. Incremental counter differences are167ms to admission and58ms further to FName, including other startup/telemetry. Do not add2130+2297+2355. Runtime_start_ok confirms a worker was started; it is not an initialized/menu-ready acknowledgement. That startup thread can overlap module return and the separate Lua-check worker. The worker’s5.044s sleep overlaps cache and init; it is not a sequential5s game readiness delay.

## Every selected wait / poll and reachability

Ploader mode2 is configured for both active PRXs in [ploader.ini](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/prelaunch/ploader.ini). Historical original source [main.c](/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c:519): 50ms tick,20 stable hits, min16 modules,600ms settle, max500 ticks (nominal25s). Each tick counts128 handles then readiness counts128 again plus dependencies; time in those calls is additional to50ms. It can exhaust and try anyway while the process is alive. This launch has no captured per-tick counts or entry/exit timestamps. The saved05r staged source adds a real25s wall deadline and proven-stable memo; **its presence on disk does not prove the deployed loader uses it**. Deployment records identify PID/config, not loader binary/source hash.

Outer loader discovery sleeps50ms and checks `INJECTOR_TIMEOUT_S120` between scans, outside the nested plugin readiness/injection work ([main.c](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-ploader-speedup/staged/main.c:794)). 120s is a lifetime ceiling checked at that boundary, not a fixed120s wait; nested burns may overrun it. Attach waits poll waitpid every10ms, attach timeout2s plus attach settle; PT remote call has a30s SIGTRAP wait. No actual l18 durations for these waits are available.

PRX cache preparation and64-handle acquisition are reached (2130ms/992ms). Pool admission retries would log `admission not ready`; none appears. After the build-axes stop, these engine-readiness loops are **not reached**: object construction≥10000 (5ms retry), KismetStringLibrary lookup (50ms), Conv_NameToString lookup (50ms), CDO lookup (50ms), required object lookup (250ms); selected current initializer lines1755–1931. Its old10ms detour FName verifier is bypassed by seeded behavioral verification. Program’s5ms event loop and future1s-rate-limited lazy carrier rescans are not observed as menu progress.

## Popup timing

No saved image, video, or display acknowledgement establishes when each popup appeared. Ploader source calls the notification API for: Injector started after config/lock; Injecting2 plugins after detecting the new process; each plugin OK/FAIL after inject+credential restore+detach; Finished after run_plugins exits, or Timeout followed by Finished. Their send calls have no l18 wall timestamps in the frozen klog. Supplier module_start/runtime_start_ok/worker entries establish lifecycle stages, not a rendered toast. The PRX’s saved diagnostic implementation writes channels; it does not itself prove notification rendering. Report any user-seen popup timing as user observation, not measured log timing. Path test wrote UE4SS at monotonic518615.145ms (entry+75.150s), demonstrating the second plugin ran later, **not the time its OK popup displayed**.

## Speed candidates — analysis only

| Priority | Candidate | Potential minutes saved | Risk / evidence |
|---|---|---|---|
|1|Real wall deadline for mode2 readiness|~0.65 per exhausted readiness burn|Medium. Historical64.5→25s simulation; preserve dependency/death checks; latest cause attribution remains inferred.|
|2|Memo only proven stable state with rechecks for plugin2|~1.03–1.16 when first readiness was proved|Medium; zero claimed savings after exhaustion. Historical prediction, not l18 measurement.|
|3|Whole-image cache overhead|At most0.0355 observed|High if authentication weakened; no safe shortcut established.|
|4|64 startup handles|At most0.0165 observed|Medium/high ownership risk; not justified as a minutes-scale priority.|
|5|Postcache admission+FName interval|At most0.00375 observed|High risk for225ms maximum; preserve gates.|
|6|Independent5s Lua probe sleep|0 critical-path saving shown|It overlaps startup and is not a game readiness gate.|

First determine the deployed loader lineage from existing identity evidence before treating05r as missing or present. These are estimates, not changes or a new console proposal.
