# USER-SESSION-GUIDE — one console session, four stages, minimal user effort

Tool: `outputs/2026-10-04-zcode-team/worker-console-batch-runner/console_batch.py`
Built 2026-10-04 from the frozen plans (8FAM DLL, E2E installer, qdrv1 stage, CP0-CP7 runbook);
extended the same day by worker-batch-runner-8fam-patch for the 8-family stage
(`CONSOLE-STAGE-PLAN-8FAM.md` + `EXPECTED-DELTA-8FAM.md`), by worker-evidence-freshness-fixes
(freshness gates), and by worker-observer-staging-extension for the COMBINED ridealong staging
(`worker-game-session-ridealongs/STAGE-PLAN-COMBINED.md` — qdrv1 + observer in ONE ploader.ini).
**The tool runs only when YOU return and authorize it. Nothing in this guide has executed on the console yet.**

Recommended: run everything as ONE session —

```sh
cd /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-console-batch-runner
python3 console_batch.py plan          # optional: preview every command + gate, touches nothing
python3 console_batch.py all           # the full session in the recommended order
```

`all` = **dll8 → e2e → stage-ridealongs → game-evidence**: both payload-only stages first in ONE
game-closed window, then the COMBINED ridealong staging (qdrv1 + observer PRXs + the combined
6-entry ploader.ini), then a SINGLE game launch with evidence collection.
You can also run each subcommand separately (`dll8`, `e2e`, `stage-ridealongs`, `stage-observer`,
`stage-qdrv1`, `rollback-ridealongs`, `game-evidence`).
Note: the settings+loadlist restage (NEW-SETTINGS §3) is NOT part of `all` — per the master plan,
land it separately between staging and launch when the session needs it.

**Exactly ONE DLL stage runs per session.** `dll8` (ALLFAM8_DLL_TEST) is the PREFERRED single
run per CONSOLE-STAGE-PLAN-8FAM: it proves a strict superset of the 7-family run (all six
allfam families + static TLS + SEH64) in the same one upload/check/dispatch/readback budget.
The 7-family stage (`dll7`, ALLFAM7_DLL_TEST) is the documented **fallback attribution path**:
run it ONLY if the 8-family run fails on a SEH64-specific surface (any -130..-137 code, a
phase-2 stop in the stage 55/56 dual-owner window, or consumer_imports landing at 88/89) AND
the window still has budget — that separates "7-family candidate fine, SEH64 graft broke it"
from a shared-surface regression. Never run both by default. Switch with
`python3 console_batch.py all --dll 7` (or run `python3 console_batch.py dll7` alone).

---

## Phase 1 — payload-only window (tool does everything; your only job: keep the game CLOSED)

**YOU: make sure Townfall is fully closed** (and no ploader arm is pending). That is the only
precondition. Then the tool, without any further input:

1. `dll8` — verifies the 8-family ELF locally (sha `79281191…` / **336552 B** + receipt flags:
   compiled-not-executed, /data report path), checks the manager on 8084, uploads
   `ALLFAM8_DLL_TEST.elf` ONCE as a fresh alias, FTP-readbacks it and requires the hash to match
   `79281191…` EXACTLY (readback law — no dispatch of bytes-divergent payloads), dispatches
   EXACTLY ONCE, waits 15 s, reads the results JSONL and verifies all 4 records (positions
   37-40 in the recommended dll8-first order) against EXPECTED-DELTA-8FAM, including:
   all NINE family keys `{"enabled":1}` from record1 (incl. `static_tls` and the new `seh64`),
   `consumer_imports` **90** (+8 SEH64 binds: 7 KERNEL32 + 1 VCRUNTIME140) with `helper_imports`
   10, `results` [86,93], worker 4/4 `begin [5,1,258,259]`, entry 7/7 / [0,0] with
   contract_errors 0, `memory.last.length` **114688** (record2) → **163840** (record3 FINAL;
   the mapped image grew 334656 → 336552), cleanup `{pending:0, retired:1}` with SEH64 retired
   first, `first_failure` all zeros (any negative stage triages through the fixture map incl.
   the new SEH64 codes **-130..-137**). Any mismatch = STOP, no re-dispatch, everything
   preserved for reconciliation.
2. `e2e` — verifies the 13 cmd files (13/13 SHASUMS) + payload (`a21771f8…` / 681888 B), uploads
   `MODBRIDGE_INST_E2E.elf` ONCE, enforces the MODE=B identity gate, then runs the 15-dispatch
   stage 0..6 cycle (stages 4-5 are SIMULATED ack-file proofs, labeled as such), checking every
   expected line verbatim, the consumed-twin proof per dispatch, and the 35-line all-green ledger.
   It ends with the §7 full-tree reset (only ever touches `/data/modbridge/**`).

If anything mismatches: the tool halts, preserves every byte it fetched, and tells you what
diverged. Do not re-run a dispatch manually — reconcile first; the tool will not re-send on its own.

## Phase 2 — ridealong staging (tool does everything; game stays CLOSED)

`stage-ridealongs` (the COMBINED staging, STAGE-PLAN-COMBINED §8 SC-0..SC-7 — this is what `all`
runs) backs up your current `ploader.ini` (`.bak` on console + local pre-image copy), then:

1. Uploads BOTH ridealong PRXs under their OWN names — `modbridge_qh_driver.prx`
   (sha `52c32216…` / 26157 B) and `modbridge_runtime_aligned_observer.prx` (sha `1afa0997…` /
   30152151 B). Because the console's ftpsrv returns the SHT-stripped fself readback (the
   segment-covered prefix of the bare-sdk ELF — full-file hash equality is IMPOSSIBLE through
   this channel by construction), the tool verifies each readback against the §3 law instead:
   exact length (87112 / 30143976 bytes) + body hash (`71406d3c…` / `3f89a5b6…`) + header
   exception (only e_shoff/e_shentsize/e_shnum/e_shstrndx may differ; e_shnum must be zeroed).
   A failed readback re-STORs from the verified local file EXACTLY ONCE, then aborts.
2. Checks the pre-launch baselines: UE4SS.log size, observer log must be ABSENT (it is created
   fresh by the module), igf jsonl baseline recorded if present (owned evidence is never deleted).
3. Appends ONLY the missing `= 2` lines to the ini (append-only law; existing lines are never
   rewritten or reordered; the ini edit is idempotent — a line already staged exactly-once is a
   no-op). The end-state gate demands the combined 6-entry layout: header once, the 4 expected
   existing lines with their pinned modes, both ridealongs at `= 2`, and EXACTLY 5 registered
   plugins (the mode-0 ingame_facts line is skipped by the parser and consumes no slot).
   Anything ambiguous — duplicate lines (ploader does NOT dedupe and would inject twice),
   missing expected lines, unexpected modes — STOPS and shows the pre-image instead of editing.
4. STORs the edited ini and re-reads it; the readback must be byte-identical.
5. Prints the arm + launch handoff (Phase 3).

`stage-observer` is the observer-ONLY staging: same SHT-strip readback law, but it appends ONLY
the observer line to whatever the ini currently is — if `stage-qdrv1` (or a prior ridealongs run)
already staged the qdrv1 line, it is left untouched (idempotent coordination). A re-run on a
fully-staged ini changes nothing and re-uploads nothing; it still re-verifies the on-console
PRX against the readback law. `stage-qdrv1` remains available unchanged as the legacy
qdrv1-only path.

`rollback-ridealongs` (STAGE-PLAN-COMBINED §7) is the recovery path: with the game CLOSED and
the ploader NOT armed it flips BOTH ridealong lines to `= 0` — two single-character edits,
verified byte-level (mode 0 is skipped at parse: no attach, no FAIL notify, no timeout) —
with a fresh `.bak` and a byte-verified readback. It is an honest no-op when nothing is staged
at `= 2`. The observer/qdrv1 LOGS are owned evidence and are never deleted by the tool.

## Phase 3 — the launch (this is the part only you can do)

The tool prints this prompt and waits for you:

> 1. With the game fully CLOSED, ARM ploader.elf (Payload Manager UI or loadpayload entry).
> 2. LAUNCH Townfall.
> 3. Wait until the game has been running for ~60 seconds.

Then it fetches the evidence sinks (runtime-status, UE4SS.log, D1, C2), runs
`post_launch_check.py` and `rung_check.py`, and checks CP0-CP4 (module ran; activation; engine-ready
+ Tick `0x37ac8c0`; mods-started; the Item Spawner load banner). If a CP fails, it stops with the
runbook's triage pointer — that is the honest result, do not relaunch on top of it.

## Phase 4 — in-game (your hands; the tool classifies, your eyes decide)

After CP0-CP4 are green, the tool prompts, in order:

> 1. **LOAD A SAVE** and get in-game. **Press OPTIONS briefly — ONE press and release.**
>    (Edge-triggered: one press = exactly one F7. Nav buttons are silent no-ops while the menu is
>    closed — that is by design, not a dead pad.)

The tool re-reads UE4SS.log and checks for `spawnable pickups found` + `Menu created.` — or classifies
one of the four known failure shapes (restage anomaly / UI build error / "load a save first" guard /
no reaction). Then:

> **CONFIRM VISUAL: is the Item Spawner menu overlay VISIBLE on screen?**
> (title, 12 rows, controls line; game input locked while open. Optional: press OPTIONS again — it
> should close silently.)

Then:

> 1. **UP/DOWN** to highlight an item, **LEFT/RIGHT** to page (visual only — no log lines exist for
>    navigation; do not expect any).
> 2. **Press CROSS once.**
> 3. Watch for a new pickup appearing in the world at your aim point.

The tool checks for the `Spawned <Item> x<N> ID=<id>` line and the per-stage failure signatures.
**The visible menu and the visible pickup are decided by YOUR eyes — the log lines never substitute**
(runbook G4; the tool states this in its own receipt).

---

## Practical notes

- Evidence: every run writes a timestamped run dir (default
  `worker-console-batch-runner/runs/<ts>-<flow>/`) with `receipt.json`, `events.log`, every fetched
  file, per-step verdict JSONs (including `qdrv1-readback-verdict.json` /
  `observer-readback-verdict.json` and the `ploader-edit-decision.json` /
  `ploader-rollback-decision.json` audit trail), the pre-image + `.bak` copies, and the
  plan-canonical console receipts
  (`allfam8-console-receipt.json`, `allfam7-console-receipt.json` if the fallback ran,
  `MODBRIDGE_INST_E2E-receipt.json`, `game-evidence-receipt.json`).
  To write directly into the plans' canonical controller dir instead, pass
  `--run-root ../../2026-10-04-zcode-controller/console` (path relative to the tool dir: it resolves
  to `outputs/2026-10-04-zcode-controller/console`). Save the full results copy as
  `allfam8-console-result-2026-10-04.jsonl` there, per the 8FAM plan's naming convention.
- Interrupted installs: if E2E stage 3 commits incompletely, the tool performs the ONE permitted
  `recover.json` dispatch and then HALTS for reconciliation — the 35-line green ledger no longer
  applies in that case, and the tool says so.
- Abort semantics: Ctrl-C or answering `abort` at a user gate stops the flow; steps already run are
  in the receipt; nothing is ever re-dispatched automatically.
- If your window only fits part of the session: run the subcommands in the same order
  (`dll8` first — smallest, fastest, cleanest attribution, and the superset; `e2e` next;
  `stage-ridealongs` + `game-evidence` only when a full launch window exists). Never interleave a game
  session with the payload window. If the console is already inside a game session when the window
  opens, defer all payload stages until it ends. After the launch, read the ploader notification
  lines (SC-5b): expect EXACTLY 5 `OK (<ms>): <file>` lines and ZERO FAIL lines — any FAIL means
  STOP and `rollback-ridealongs`, never a relaunch of an identical rejected layout.
- If the 8-family run fails SEH64-specifically and budget remains: run
  `python3 console_batch.py dll7` — the frozen fallback attribution path. It then appends at file
  positions 41-44 (dll8 having taken 37-40). Record which you ran; the verdicts are per-build.
- Host-tested, not console-proven: the tool's logic passed 271 selftest checks + a full simulated
  engine drive including the dll8 flow, a stale-artifact halt scenario, and the combined
  ridealong staging/rollback flows with the §3 SHT-strip readback law exercised both ways
  (`selftest`, `tests/fake_console_drive.py`). Console timings and on-target behavior remain
  [TARGET] until this session runs. The §3 readback constants were additionally recomputed on
  host from the pinned bare-sdk ELFs and match the plan's table exactly
  (qdrv1 87112 B / `71406d3c…`; observer 30143976 B / `3f89a5b6…`).
