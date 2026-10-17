# Launch-9 result — composition-6 startup test (2026-10-05, pid 273)

## Verdict

**No crash. Two fail-closed refusals at two distinct stages. One full harness pass.**
The game reached its menu normally. No mods loaded (the Item Spawner popup was not
expected and did not appear). Every failure below is a designed loud refusal — none
touched game memory beyond reads.

## Per-plugin scoreboard (8 plugins referenced, 4 present, 4 missing)

| plugin | present | outcome |
|---|---|---|
| exec_write_probe_armed (xwp) | yes | ran; all 6 mprotect rows SKIPPED by its transition table; 0 attempts, 0 memory touched; route `sceKernelMprotect_import` resolved. Harmless as parked. |
| supplier_composition (composition-6) | yes | ran; passed path validation, configured roots (Resolution B), VMMAP walk, lease REGISTRATION; refused at lease READ: `runtime_start_failed code=2 detail=Owned read range includes unregistered bytes` (observed at runtime-status session 234 window) |
| runtime_aligned_entry (build f6530c79) | yes | aligned start refused EARLIER: `runtime_start_failed code=1 detail=no observed mapping interval intersects the module image extent with valid geometry` — this is the TERMINAL launch-9 state (runtime-status session 273, t=44372083, one tick after mirror line `[t=44372082ms pid=273]`). Self-contained fallback harness then ran 100% PASS: full constructor chain complete, module_start returned 0, embedded Lua VM created → lua_load → lua_call → result match → close → memory check, all 0. **First fully-green in-game harness, including a live Lua VM, inside the real game process.** |
| path_test | yes | ran; DIAG module_start reached (no failure) |
| fc1pool / qh_driver / observer / carrier | NO (0B) | not injected; produced no lines (expected for missing files) |

## Milestones proven by this launch

1. **Resolution B (configured roots) works in the real game** — no "executable path
   empty", no "must name existing regular files" failure from the composition family
   this launch (those texts only appear from pre-Resolution-B builds in older log
   entries).
2. **The entry's complete self-contained runtime is alive in-game**: 232 constructors,
   worker thread, and a real Lua VM executing and verifying code — all inside the
   shipping game process, zero errors.
3. **Fail-closed design held everywhere**: no crash, no partial writes, game stable.

## Open failures (both root-causes dispatched to workers)

- **Lease coverage** (composition): the PLH ReadLease walked pages outside the
  registered set despite the r11 outward page-quantization fix. Suspects: exec-only
  text pages inside the lease range, unmapped gaps, the 128-span registration cap,
  or VMMAP protection-bit semantics. → worker-lease-coverage-fix (running).
- **Facts-adapter geometry** (entry aligned start): zero VMMAP intervals found
  intersecting the module image extent, while the composition passes the same check
  in the same process — the entry's standalone inputs (extent derivation /
  configured-roots fields) must differ. → worker-facts-geometry-fix (running).

Note: the predicted Shape-A `[Seeds] missing [gmalloc]` stop was never reached —
both refusals happen upstream of the initializer. The armed 4-slot seed table and
the GMalloc V1/V2 validator diff (worker-launch10-gmalloc-integration, +75/−1,
host-tested) are both ready and waiting for the launch-10 rebuild.

## Artifacts in this dir (sha256)

- `UE4SS-log-final.txt` edf404a5… (7591 B, append-only mirror, final at ~12:05 console time)
- `runtime-status-final.txt` 57c40d4c… (session 273 terminal state)
- `aligned-entry-log-snapshot.log` 4b33b985… (2,343,756 B at 11:58, +133,450 B over the 2,213,030 B session-eve baseline)
- `xwp-jsonl-final.jsonl` 20cff601… (15 records, build igd1-6ebab07f2111)

Baselines used: UE4SS.log baseline fetch failed (411 B error page) — classification
relied on runtime-status.txt mtimes/values, r4-timestamp correlation, and the
entry-log size delta instead.
