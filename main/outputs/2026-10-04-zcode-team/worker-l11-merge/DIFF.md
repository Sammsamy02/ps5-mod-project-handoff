# DIFF — merged LuaMod.cpp vs base

Worker: worker-l11-merge · 2026-10-05 · LOCAL ONLY (no composition, no packaging, no console).

- base:   `worker-l11-marshal-integration/staging/luamod-l11-marshal-consumer/LuaMod.cpp`
  sha256 `04c9d91664dd82c7831cfc7eff79b6c20c62df9e6fd6eced60d419310966ad9f` (8207 lines)
- merged: `staging/luamod-l11-merged/LuaMod.cpp`
  sha256 `dc68760253ec141f2eec4696050b8a2dd9f01b7f2894829d5f543516fb44f7f4` (8327 lines)
- full unified diff: `receipts/merged-vs-base.diff` (sha256 `46f2735b329e02d91de8c6bcc490ba89003d1d68c200bfda209d2e293e414123`)

Net: +120 lines, 7 hunks, zero deletions of pre-existing lines outside the two
single-line lazy-arm elisions that delta A itself makes. Every hunk is inside an
existing `#if/#ifdef defined(__PROSPERO__)`-guarded region introduced by the
lineage (consumer/delta content); nothing outside those regions is touched.

## Hunk inventory (old-file coordinates)

| # | hunk (old lines) | delta | content |
|---|---|---|---|
| 1 | 5004–5010 | A | EngineTick-method branch: lazy `ensure_process_event_hooked(mod)` elided (method selection only); enqueue-first comment |
| 2 | 5021–5027 | A | EngineTick else-branch: `__PROSPERO__` marshal-first-when-armed ladder (`is_funcptr_marshal_armed()` → `kFuncPtrMarshalExecutor`); non-PROSPERO keeps `ensure_engine_tick_hooked()` |
| 3 | 5033–5039 | A | ProcessEvent-method branch: lazy `ensure_engine_tick_hooked()` elided (mirror of #1) |
| 4 | 5050–5056 | A | ProcessEvent else-branch: `__PROSPERO__` marshal-first ladder (mirror of #2) |
| 5 | 5100–5105 | A | THE ORDER GUARANTEE: post-enqueue lazy arm inside real port `TRY` + two loud `LogLevel::Error` stall logs (closure enqueued-but-no-pump) |
| 6 | 7321–7326 | B | `#ifdef __PROSPERO__` gate `if (Unreal::R3MarshalPumpPending()) { one loud Warning } else` before the eager detour-arm block (LoadMapPre first) + 42-line r2 routing rationale comment |
| 7 | 8100–8105 | B | closing `}` of the gated eager-arm block after `RegisterProcessInternalPostCallback` |

## Attribution (machine-verified, see MERGE.md)

- merged = base + `worker-l11-enqueue-ladder/diffs/l11-enqueue-ladder.diff`
  (sha256 `3b19068a194ec82dd58fecac1a1d53051a05aa2965558d706db7de68bc3c4a04`, hunks 1–5)
  + `worker-l11-loadmap-guard/diffs/l11-loadmap-defer-r2.diff`
  (sha256 `d2ad2818db4b73383003097ab13d0aef0e6bc0d3b45a1e3347d9eee664dfcbb7`, hunks 6–7).
- Cross-diff proof: `worker-l11-enqueue-ladder/staging/luamod-l11-el-ladder/LuaMod.cpp`
  (`f50976ae1e2626b0…`) → merged differs by exactly delta B's 51 changed lines
  (`work/el-vs-merged.diff`); `worker-l11-loadmap-guard/staging/luamod-l11-loadmap-defer-r2/LuaMod.cpp`
  (`9a7a37d6c8377c02…`) → merged differs by exactly delta A's 73 changed lines
  (`work/lg-vs-merged.diff`).

No other line of the 8207-line base changes. Line-count arithmetic: base 8207
+ 69 (delta A net) + 51 (delta B net) = 8327.
