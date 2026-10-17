# EVIDENCE — pid 700, full-fix candidate 07ac3e1e boot (2026-10-05 18:40 local)

Archive worker: worker-evidence-archive · Scope: `outputs/2026-10-04-zcode-controller/launch10-evidence/pid700-fullfix/`
Source: `/tmp/fullfix-*` pulled from the console 2026-10-05 18:41 — until this archive,
`outputs/` contained **no pid-700 evidence anywhere** (worker-stage-metrics search, negative).
All numbers below were decoded PROGRAMMATICALLY from the archived bytes (script + outputs
preserved here); nothing is transcribed by hand from memory.

## 0. Archived files (byte-identical, sha256 verified before/after)

| file | bytes | sha256 |
|---|---|---|
| `primary/fullfix-sdiag.jsonl` | 1,557,187 | `5d40b782227eace0c6f7f78b2485630885e124c3d7d48eaec2a467d6e67d1403` |
| `primary/fullfix-status.txt` | 400 | `8236e3cf1ce086888c65fad76e6d11dcc5a70d1f10d29e9e8b7ee908961f5056` |
| `primary/fullfix-ue4ss.log` | 1,905 | `a976ca0f366d9f67d3b428198916248246bf69b7a2b5907e23b797cf8d00f4ae` |

Hashes recorded in `SHA256-before.txt` (taken in /tmp before copy) and `SHA256-after.txt`
(taken in `primary/` after copy) — identical; `cmp` additionally confirms byte equality.
Related-session artifacts from the same pull chain: see `related-sessions/` + `MANIFEST.md`
(hashes in `SHA256-related-sessions.txt`, all verified against /tmp originals).

Build identity: the boot ran `prx-candidate-l11-1`, final PRX sha256 `07ac3e1e6c325805…`
(`outputs/2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-1/artifacts-receipt.json:5-9`).
UE4SS log header: `UE4SS - v3.0.1 - Git SHA #e3ba1016562d6c0868c4`, `Build Configuration: Native ex (Clang)`,
all lines stamped `2026-10-05 18:40:10` (`primary/fullfix-ue4ss.log` lines 1-9).

## 1. Session identity (pid 700)

- `primary/fullfix-status.txt` line 2: `session	700	86714471` — session/pid 700, status monotonic ms 86714471.
- `primary/fullfix-sdiag.jsonl` is the CUMULATIVE durable-events sink (10,441 records, 19
  epochs, pids 317→700 in append order). The pid-700 epoch is the LAST block: **553 records,
  seq 1..553, t_first=86713681, t_last=86719427, span_ms=5746**.
- Correlation tool: `python3 outputs/2026-10-04-zcode-team/worker-durable-events/tools/epoch_view.py
  --sdiag primary/fullfix-sdiag.jsonl --ue4ss primary/fullfix-ue4ss.log --status
  primary/fullfix-status.txt --build-id 07ac3e1e` — full table preserved verbatim in
  `epoch-view-fullfix.txt`; the `-- epoch pid=700` section carries the per-epoch verdict
  `STARTED — conversion quartet met (D1-D5)`, terminal `runtime_start_ok`, and the
  status-agreement note (LATEST-STATE-ONLY, session pid matches).
- Per-event slice preserved as `pid700-timeline.txt` (553 rows: seq, t_ms, dt_ms, tag, value).

## 2. Spanpub events (pid 700) — the pool-capture envelope

From the pid-700 records (last-write-wins within the pid; exact seq/t_ms from `pid700-timeline.txt`):

| seq | t_ms (dt) | tag | value | value_hex |
|---|---|---|---|---|
| 504 | 86714420 (+739) | `bridge_spanpub_count` | 64 | `0x00000040` |
| 505 | 86714421 (+740) | `bridge_spanpub_total` | 126 | `0x0000007e` |
| 506 | 86714423 (+742) | `bridge_spanpub_cap` | 1 | `0x00000001` |
| 507 | 86714424 (+743) | `bridge_spanpub_env_low` | 4194304 | `0x00400000` |
| 508 | 86714426 (+745) | `bridge_spanpub_env_high` | 17 | `0x00000011` |

**Decoded envelope (programmatic):**

```
envelope = (env_high << 32) | env_low
         = (0x00000011 << 32) | 0x00400000
         = 0x1100400000 = 73,018,638,336
Blocks[0] (status, verbatim) = 0x22C0000000 = 149,250,113,536
assert 0x22C0000000 >= 0x1100400000  →  OK (delta 0x11BFC00000 = 76,231,475,200 ≈ 71.0 GiB)
```

**What this proves:** the pool's FIRST block pointer sits ~71 GiB ABOVE the top of the
captured/published readable envelope. Even with the span cap raised, the snapshot cannot
prove heap coverage at or past its own captured envelope — the omitted region is an omitted
mapping, not a proven-unmapped one. That is exactly the status text's refusal class:
**capture-truncated**, not "pool absent".

The cap bump itself is proven by these numbers: `bridge_spanpub_count=64` published of
`total=126` (LAUNCH10-RESULTS §5 G9 recorded the previous boots saturating at
`kMaxPublishedSpans=16` of 119/127; both prior statuses here say "published readable
spans 16"). 64/126 published, and the REFUTED still stands ⇒ the pool lives far outside
the captured window, so a bigger publish cap alone will never close this class.

Contrast with the two immediate predecessor epochs in this archive (durable events, not
status text): pid 645 and pid 672 BOTH published `count=0x10 (16)` of `total=0x7a (122)`,
envelope `env_high=0x11`/`env_low=0x003b0000` → 0x11003B0000 — i.e. the cap bump first
took effect in pid 700 (64 published), and the envelope moved slightly (low 0x3B0000 →
0x400000) while its top stayed 0x11_00000000. Same Blocks[0] 0x22C0000000 refutes against
either envelope (0x22C0000000 ≥ 0x11003B0000 also holds).

## 3. vmentry / walk

| seq | t_ms (dt) | tag | value | value_hex |
|---|---|---|---|---|
| 509 | 86714430 (+749) | `bridge_vmentry_entries` | 10070 | `0x00002756` |
| 510 | 86714431 (+750) | `bridge_vm_walk_stop` | 0 | `0x00000000` |

**What this proves:** the VM walk enumerated 10,070 entries and stopped CLEANLY (stop=0 =
end-of-map, no truncation flag) — the capture machinery itself worked; the envelope is
simply bounded. (10070 < 16384 pinned `kBridgeVmmapEntries`, so LAUNCH10-RESULTS §5 G7's
capacity question does not apply to this epoch.)

## 4. Full timeline (t_ms, pid 700) — milestones

All from `pid700-timeline.txt` (full 553-row timeline; dt relative to t_first=86713681):

| t_ms | dt | event | value |
|---|---|---|---|
| 86713681 | +0 | `module_start_entered` | 0 |
| 86713686 | +5 | `constructor_count` | 235 |
| 86713687 | +6 | `import_validation_begin` | 187 |
| 86714374 | +693 | `full_runtime_constructors_complete` | 0 |
| 86714376 | +695 | `worker_create_result` | 0 |
| 86714377 | +696 | `probe_lifecycle_result` | 0 (NOT the B4 dead-rebuild) |
| 86714379 | +698 | `runtime_wiring_begin` | 0 |
| 86714385/87 | +704/706 | `mfa_activated_low/high` | 37063 (0x90c7) / 0 |
| 86714410/11 | +729/730 | `stat_ownmod` / `_ec` | 1 / 0 (r10 fork resolved own module) |
| 86714416 | +735 | `bridge_exepath_source` | 0 |
| 86714417/18 | +736/737 | `bridge_lease_reg_spans` / `bridge_lease_reg` | 6 / 6 |
| 86714420-26 | +739-745 | spanpub set (§2 above) | count 64 / total 126 / cap 1 / env |
| 86714430-31 | +749-750 | vmentry 10070 / walk_stop 0 (§3 above) | |
| 86714455 | +774 | `anchor_match` | 262145 (0x00040001) |
| 86714457 | +776 | `anchor_cov` | 327680 (0x00050000) |
| 86714458 | +777 | `anchor_c_either` | 1 |
| 86714460/61 | +779/780 | `bridge_cfg_gamedir` / `bridge_cfg_workdir` | 1 / 1 |
| 86714463-67 | +782-786 | `lmf_extent_base_low/high`, `end_low/high` | 0x00400000/0, 0x0c0602c8/0 ⇒ extent [0x400000, 0xc0602c8) |
| 86714468 | +787 | `lmf_anchor_count` | 7 |
| 86714470 | +789 | `lmf_intervals_found` | 1 (coalesced; launch-9 zero-interval death still dormant) |
| 86714479-90 | +798-809 | pad defer set: `pad_input_deferred` 6, `_error` 0x80920004, `settings_mod_source` 1, `_input_gate` 1 | |
| 86714512 | +831 | `runtime_start_ok` | 0 |
| 86714514 | +833 | `module_start_return` | 0 (entry returned 0; per LAUNCH10-RESULTS §5 G4 quirk) |
| 86719415-27 | +5734-5746 | buffered-Lua harness: begin 88 → result 0 (load 0, call 0, match 0, memory check 0) | green |

D1+D2 extent, D3 anchor_count=7, D4 intervals=1, D5 cfg=1/1 — the conversion quartet is
met with the EXACT predicted geometry (LAUNCH10-RESULTS §2), and E3 `runtime_start_ok`
emitted at +831 ms. This is the first epoch in the durable sink to pass the FULL ladder
into Phase F seeds territory (all earlier start-ok epochs 631/645/672/686 are in the same
cumulative file and also carry runtime_start_ok; pid 700 is the freshest).

Status (`primary/fullfix-status.txt`, LATEST-STATE-ONLY — applies to pid 700 ALONE):
line 3 `state	1	activated	0` (state 1 = activated), line 4 `state	2	failed	1
[Seeds] REFUTED: …`. Both states coexist on file (400 B vs the 117-167 B single-state
files of earlier sessions) — the E4 `activated` line is LITERALLY on file for the first
time in archived outputs, exactly as LAUNCH10-RESULTS §4 predicted ("E4 `state 1
activated 0` survives in status for the first time").

## 5. The REFUTED line (verbatim) — first captured Blocks[0]

`primary/fullfix-status.txt` line 4:

```
state	2	failed	1	[Seeds] REFUTED: FName pool at 0x9765EC0: snapshot truncated: cannot prove heap coverage at or past the captured envelope (omitted mapping, not proven unmapped) (observed CurrentBlock 68, Blocks[0] 0x22C0000000; published readable spans 64). Terminal for this launch; derive afresh with new evidence, never replay.
```

- **Blocks[0] = 0x22C0000000, CurrentBlock = 68, published readable spans = 64** — per
  `outputs/2026-10-04-zcode-team/worker-blocks0-provenance/REPORT.md` (verdict "NO CAPTURE
  EXISTS — the 'candidate/synthetic' label stands"), no earlier invocation ever captured a
  Blocks[0] value; this line is the first real captured value to reach `outputs/`.
  The two prior sessions in this chain (inputfix 645, capbump 672) carried the same
  Blocks[0] 0x22C0000000 / CurrentBlock 68 with spans 16 (see `related-sessions/`), but no
  pre-fullfix artifact had been archived under `outputs/` before this pull.
- The pool validator itself stayed alive to REFUTE (no crash): the game process survived —
  entry returned 0, buffered-Lua harness green at +5746, and this is a caught, named
  `failed` state, not a crash tail.

## 6. The double-garble — "Mod is absent" (raw bytes)

`primary/fullfix-ue4ss.log`, cleaned line 22:
`[2026-10-05 18:40:10] Mod is absent from native Lua source: ue4ss/Mods/Town`

RAW bytes (file byte offset 1040, line 22 = 68 bytes containing **15 NUL bytes**):

```
4d 6f 64 20 69 73 20 61 62 73 65 6e 74 ... 73 6f 75 72 63 65 3a 20
75 00 65 00 34 00 73 00 73 00 2f 00 4d 00 6f 00 64 00 73 00 2f 00
54 00 6f 00 77 00 6e 00 0a
        → "u\0e\0 4\0s\0s\0/\0M\0o\0d\0s\0/\0T\0o\0w\0n\0"
```

i.e. every ASCII character of `ue4ss/Mods/Town` is followed by a NUL — the exact
4-byte-unit-writes-into-2-byte-wchar stride signature of the PS5 short-wchar conversion
bug (same class as the settings [::2] corruption, worker-construction-error ANALYSIS.md §1-3).
The wide string handed to the log device already contained the interleaved NULs, and the
UTF-8 writer (`first/src/DynamicOutput/WriterDevice.cpp:34`, `Unicode::EncodeUtf8`)
faithfully encoded each U+0000 — the double conversion (narrow→wide→log bytes) doubles the
damage into the visible "u e 4 s s / M o d s / T o w n" garble.

**Conversion site 1 — the live emitter (setup_mods ensure_str):**
`outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp:941`
(admitted into build 07ac3e1e per `launch11-combined/receipts/l11-replace-receipt.json`):
`Output::send<LogLevel::Error>(STR("Mod is absent from native Lua source: {}\n"), ensure_str(m_mod_path));`
— `ensure_str` on a `std::filesystem::path` dispatches
`to_charT_string_path<wchar_t>` → `arg.wstring()`
(`…/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp:407-412`).
The shipped `luamod.o` provably used the FIXED String.hpp + Utf16Utf8.hpp
(`run-l11-1/luamod.d` lists both), so the fixed `to_wstring`/`to_string` codec is in the
build — but the PATH→wide branch goes through `path::wstring()`, the one remaining live
conversion that still routes through the broken target locale. Hence the garble survives
the full-fix build.

**Conversion site 2 — the dead modbridge override:**
`outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp:43-54`:
a local `namespace modbridge { normalize_path_for_lua … path.string() … }` override
("All mod paths are pure ASCII; use the filesystem path's narrow string directly"),
explicitly written to dodge the codecvt bug. It is CORRECT but DEAD for this message:
it only converts the lookup key used to search `m_native_source->lua_mods()` (LuaMod.cpp:936);
the error emission converts `m_mod_path` AGAIN through `ensure_str` (site 1) and never
passes through the override. The garbled log line is the live proof that the mod lookup
itself could work while its absence MESSAGE still double-garbles.

Together: **the mod "Town" was declared absent via the double-garble** — the second
conversion (message path) bypassed the fix that the first (lookup path) already had.

## 7. One-sentence status

Runtime started (conversion quartet D1-D5 met, `runtime_start_ok` at +831 ms, E4
`activated` literally on file); the Phase-F FName-pool validation then REFUTED with the
capture-truncated class (Blocks[0] 0x22C0000000 ≈ 71 GiB past the 0x1100400000 envelope,
64/126 spans published, walk stop=0 clean — terminal for this launch, never replay); and
the mod was declared absent via the double-garble (message-site `ensure_str(m_mod_path)`
at l11-merged LuaMod.cpp:941 through `path::wstring()`, while the correct
`modbridge::normalize_path_for_lua` override at :43-54 is dead for the message path).

## 8. Tooling & reproduction

- Correlation: `python3 outputs/2026-10-04-zcode-team/worker-durable-events/tools/epoch_view.py
  --sdiag primary/fullfix-sdiag.jsonl --ue4ss primary/fullfix-ue4ss.log --status
  primary/fullfix-status.txt --build-id 07ac3e1e --out epoch-view-fullfix.txt` (rerun for
  this archive; table preserved).
- Decode/assert: filter `pid==700` records from `primary/fullfix-sdiag.jsonl`, take
  last-write-wins values for the seven tags in §2/§3, compute
  `envelope = (int(env_high,16) << 32) | int(env_low,16)`, assert
  `0x22C0000000 >= envelope` — PASSED (script executed during archive; timeline written to
  `pid700-timeline.txt`).
- Guards honored: t_ms deltas never taken across epochs; the status file is read as
  LATEST-STATE-ONLY for pid 700 alone (LAUNCH10-RESULTS §Post-verification scope corrections).
