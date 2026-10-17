# agent-Y — l25 harvest tool (per-launch evidence pull + marker analysis)

Date: 2026-10-08. Written and host-tested on the Mac only. **The console was
never contacted** — all local testing used `--dry-run`, offline function-level
validation against the saved l24 evidence files, and a fake-curl shim. The
script will be RUN LATER BY ROOT after each authorized launch.

## Files delivered (this directory)

| Path | What |
|---|---|
| `harvest.py` | python3, stdlib only. Pulls evidence, analyzes markers, writes `HARVEST-<label>.md` |
| `markers.json` | 28-entry marker table: 24 cited from today's reports + 4 `PENDING agent report` placeholders (agent-O/agent-P absent) |
| `HARVEST-<label>.md` | produced per run (not present yet — no live run has occurred) |

## The one command root runs after each launch

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/harvest.py --label <launch-label>
```

Concrete example (label names the launch; used in every output filename):

```
python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/harvest.py --label l25-launch-1
```

Variants:

```
# no klog analysis (e.g. no tap was running):
python3 .../harvest.py --label l25-launch-2 --skip-klog

# different console address (default ps5-host-not-configured.invalid):
python3 .../harvest.py --label l25-launch-3 --console-ip ps5-host-not-configured.invalid

# plan-only, zero network, zero writes (safe anytime):
python3 .../harvest.py --label rehearsal --dry-run
```

Exit codes: `0` report produced (read it — pull failures do not abort the
run); `2` usage error (missing/unsafe `--label`); `3` markers.json
missing/invalid.

## What it pulls (subprocess curl, each `--max-time 20`, per-item continue)

No `SITE SELF` is used anywhere — text logs only, no executables, so the
ftpsrv decrypt-on-download rule cannot bite (l24 HARVEST-PLAN section 3 note).

| # | Remote (ftp://IP:2121...) | Local file | Path citation |
|---|---|---|---|
| 1 | `/data/plugins/UE4SS.log` | `harvest-<label>-ue4ss.log` | l16 console-test SESSION.json harvest chain; its `harvest-first.log` line "fetched /data/plugins/UE4SS.log -> UE4SS.log (9253 bytes)"; l24 HARVEST-PLAN section 3 item 1 |
| 2 | `/data/modbridge/roots/PPSA33286/runtime-status.txt` | `harvest-<label>-runtime-status.txt` | task contract; l16 harvest-first.log "fetched ... runtime-status.txt (206 bytes)"; l24 HARVEST-PLAN section 3 item 2; PPSA33286 = Townfall title id (probe_config.h PROBE_TITLE) |
| 3 | `/data/modbridge-supplier-diag.jsonl` (l24-era DIAG STREAM) | `harvest-<label>-supplier-diag.jsonl` | l16 SESSION.json references the local copy `supplier-diag-this-launch.jsonl`; exact remote path proven by the same session's harvest-first.log "fetched /data/modbridge-supplier-diag.jsonl ... (3266259 bytes)"; restated l24 HARVEST-PLAN line 121; marker `modbridge.supplier-diag/v1` verified in `/tmp/l24-diag.jsonl:1` |
| 4 | `/data/townfall_runtime_aligned_entry.log` (l24-era ALIGNED-ENTRY log) | `harvest-<label>-aligned-entry.log` | `PROBE_LOG_PATH` in `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/generated/probe_config.h`; corroborated by `/tmp/l24-aligned.log:1` carrying exactly `PROBE_MARKER` "modbridge-runtime-aligned-entry/v1 build=f6530c79..." (agent-F evidence list; l24 REVIEW-PACKET section 8). NOTE: the l16 SESSION.json/klog provenance itself does not name this path — the spike header + matching harvested marker is the citation. |
| 5 | klog — NOT an FTP pull | picked up in place | local TCP tap files `klog-*.log` (stream host ps5-host-not-configured.invalid:3232, proven by l16 `klog-process.log:1` and l24 `klog_capture.py`). Ranked dirs tried in order: (1) the harvest dir itself, (2) `/tmp/l25-harvest`, (3) `/tmp/l24-harvest` (l24 tap OUT_DIR precedent). First dir with hits wins; newest mtime within it. |

**agent-T-klog-capture.md was ABSENT** when this was written, so the ranked
klog dirs follow the verified l16/l24 tap precedent and are marked PENDING in
both `harvest.py` (KLOG_RANKED_DIRS comment) and `markers.json` `_meta`.
When agent-T lands, update `KLOG_RANKED_DIRS` — nothing else changes.
Similarly, agent-O-listener-instrumentation.md and agent-P-phase-tags.md are
absent: their expected marker families are present as 4 placeholder entries
(`AddUObjectDeleteListener`, `listener-`, `phase=`, `[l25]`) with
`"source": "PENDING agent report"`; replace the placeholder patterns with the
reports' exact strings when they arrive.

## Marker table (markers.json)

28 entries, each `{pattern, meaning, phase, source, files?}`. Files default
to `["ue4ss","klog"]` (task contract); diag-sink tags (pad chain,
`r3_marshal_pumped`, `runtime_start_ok`, `mfa_*`) extend to
diag/aligned/runtime-status so the table stays useful without extra flags.
24 cited (agents F/G/H/I/J + l24 HARVEST-PLAN ladder + task-specified nine:
`Adding shutdown listener`, `TypeScan] scan-complete`,
`[R3] FuncPtr-marshal armed`, `[l22] Lua mod started`, `mfa_activate_failed`,
`module_start failed`, `r3_marshal_pumped`, `copyin`, `nonsleeping`), 4 PENDING.

## Report contents (HARVEST-<label>.md)

1. timestamp (UTC), console, mode, klog source
2. per-file pull status + bytes + sha256 (rc 0 → ok; rc 78 → absent; rc 0 + 0
   bytes → empty (recorded, l23 autotune precedent); else failed + curl rc/stderr)
3. session boundary analysis: last `[YYYY-MM-DD HH:MM:SS]`-prefixed timestamp
   = current session init end; session count = UE4SS banner count
   (`UE4SS - v3.0.1` format, verified in `/tmp/l24-ue4ss-4.log:2`);
   modbridge `[t=...ms pid=...]` appended lines counted separately
4. every marker hit: line numbers + ctx-1/HIT/ctx+1 (cap 60 hits/file/marker,
   surplus summarised; lines trimmed at 240 chars)
5. VERDICT: `init-passed-:528?`, `mod-started?`, `crash-signature?`, `admission?`

### Verdict semantics (one honest refinement over the literal spec)

`init-passed-:528?` = marker present after the last `Adding shutdown
listener` — **but** modbridge-format `[t=...ms pid=...]` lines after it are
appends from a LATER session's failed module_start (l24 precedent, agent-J
section 5), so they are reported separately and do NOT count as same-session
progress. This keeps the l24 stall signature an honest NO instead of a false
YES when a later mfa failure merely appended a line.

## Local test evidence (zero console contact)

1. `--dry-run` (`--label l25-launch-1`): rc 0, 137 lines — prints all four
   exact curl commands, the ranked klog dirs + current resolution, the full
   28-entry marker table (cited vs PENDING), and the report skeleton; the
   harvest directory contained only `harvest.py` + `markers.json` afterward
   (no writes). Full output excerpt in the agent-Y final message.
2. Argument handling: missing `--label` → rc 2; `--label "../evil"` and
   `--label "a/b"` rejected rc 2 (label restricted to `[A-Za-z0-9][A-Za-z0-9._-]*`
   since it lands in filenames); `--skip-klog` and `--console-ip ps5-host-not-configured.invalid`
   reflected in the plan; `-h` rc 0.
3. Offline live-path validation (imported functions, real l24 evidence,
   nothing written): `/tmp/l24-ue4ss-4.log` → last timestamp
   `2026-10-08 13:47:29` at line 608, banners=1, modbridge append at line 609
   (matches agent-J exactly); init=NO (stall, later-session append only),
   admission=FAILED (mfa lines), crash=YES (klog copyin 6188 / nonsleeping 8
   hits), mod-started=NO; synthetic healthy log → init=YES, mod-started=YES,
   admission=OK; no-listener log → init=NO.
4. Pull classification via fake-curl shim in /tmp (removed after): rc 0+bytes
   → ok+sha256; rc 78 → absent; rc 28 → failed with note. Per-item
   continue-on-failure confirmed.

## Operational notes for root

- Run the klog tap BEFORE launch (it is a live TCP stream at
  ps5-host-not-configured.invalid:3232, not a fetchable file); the harvest picks up its output
  from the ranked dirs. If the tap wrote elsewhere, copy/drop the
  `klog-*.log` into the harvest dir and re-run, or pass `--skip-klog`.
- One harvest per launch, after the game is closed normally (~20 s wait,
  l24 HARVEST-PLAN section 1 step 9). No failed-step replay, no second
  launch without new authorization (PS5 rapid-fire rule).
- Raw pulled files are preserved unchanged (provenance rule); the report is
  additive only.
- Task-contract note: the task text located the l16 provenance under
  `~/.zcode/worktrees/...`; that path does not exist on this Mac — the
  identical tree lives under `~/.codex/worktrees/...`, which is what was read
  (SESSION.json, harvest-first.log, klog-process.log, KLOG-COMPLETED.json all
  found there).
