# CLAUDE ROUND-3 RECEIPT — what each directive got

Written by agent FP (ZCode), 2026-10-10 early UTC. Receipt for the next
Claude review, mapping your round-3 directives (answers to
`CLAUDE-MENU-UP-SPAWN-BLOCKED.md` §10) to agents, artifacts, and status.
Honesty note: FJ–FO were still running in parallel (offline, console in REST
MODE) when this receipt was written — statuses say "in flight" where their
artifacts had not yet landed on disk; root fills exact shas/paths from the
build receipts and agent reports at wake. Baseline evidence for everything
below: `evidence/claude2-menu-session-20261010.log` (grep -a).

## Q1 — instrument-first, but in zzdiag v11 (NOT the mod)

You were asked mod-instrument vs route-reverse-first; you ordered a third
way: **instrument zzdiag, not the mod.**

- Agent: **FJ** — zzdiag v11 pickup probe.
- Artifact: `zzdiag-v11/` (v11 main.lua + EXPECTED-MARKERS.txt) + agent-FJ
  report (follows the v8/v9/v10 dir convention).
- What it must do: reproduce the `pickup.PickupInventoryItem.ItemId` read
  path with verbose per-hop logging (type, raw tostring, pcall error text),
  emitting `[PRB-*]` markers so the exact failing hop is named — on the
  CURRENT build, since the probe launch rides the RESIDENT efbfc656.
- Status: **in flight (offline tonight) → pending wake-time** deploy via
  `harvest/lua_fast_deploy.sh` + user launch #1 (WAKE-RUNBOOK-20261010.md
  steps 2–3). No mod bytes are touched — the mod stays byte-identical for the
  step-5 acceptance.

## Q2 — refusal logging: first-10 + call-site tag + hex units

- Agent: **FK** — [FN] seam-refusal instrumentation hardening.
- Artifact: agent-FK report + staged refusal-logging patch (source rows via
  root's overnight rebuild).
- What it must do: every seam refusal logs (a) first-10 per reason, (b) a
  call-site tag, (c) hex units of the offending buffer — so the observed
  `reason='embedded' len=4 at=1` refusal (fired once, at the 00:54:35
  24-pickups scan moment) becomes attributable to a concrete site instead of
  a counted mystery.
- Status: **in flight (offline tonight)**; lands in the overnight rebuilt PRX
  if root's composition included it (root: record yes/no in the build
  receipts). Wake-time evidence arrives with the FIRST launch that exercises
  the seam (probe or acceptance).

## Q3 — full conversion-site inventory + route ALL reverse sites

- Agents: **FL** (inventory) and **FM** (routing).
- Artifacts: agent-FL conversion-site inventory report (every engine→Lua
  string conversion site); agent-FM routing change + CHANGES rows (root
  rebuilt/repackaged/`verify_build.sh --manifest` the routed PRX overnight;
  sha recorded in `outputs/2026-10-08-l26-package-A/` receipts).
- Why: session evidence shows the forward bridge families fired and passed
  ([LFB-M] fname-narrow ×10, fstring-fwd ×10, ftext-fwd ×10, all ok=1) while
  **fstring-rev / ftext-rev have ZERO lines all session** (grep-verified 0
  matches) — the ItemId read returns through a different, unrouted converter
  that still carries the seam (your H3).
- Status: **inventory in flight; routed build pending root's overnight
  manifest-PASS** → wake-time deploy per WAKE-RUNBOOK step 4 ONLY after
  verify_build.sh --manifest PASS; if the probe (Q1) names a hop the routed
  build does not cover, the named hop is fixed first.

## Post-spawn items

- **Freeze known-good + byte-identical original-mod acceptance** — agent
  **FN**: identity doc for the byte-identical ORIGINAL TownfallItemSpawner
  (mod's console main.lua archived at
  `evidence/townfall-main.lua.console-copy`; sha/size checks around the
  restore). Status: **in flight (offline)**; executed at wake as user launch
  #2 — MENU up AND item spawned = MENU+SPAWN acceptance (WAKE-RUNBOOK step
  5). No rerun of unchanged evidence; one acceptance launch.
- **12-mod generality ladder** — agent **FO**: per-mod ladder schema (loads
  without uninstall → keybinds → FindFirstOf-class lookups → UI via the
  proven Create/StaticConstructObject path → mod-specific marshaling; one
  launch per mod, clean exits, harvest.py verdict each). Candidates staged
  in `multimod-prep/` (CANDIDATE-INVENTORY/SHEETS/TEST-PLAN,
  ZZDIAG-RETIREMENT). Status: **schema in flight (offline)**; execution
  starts only AFTER step-5 acceptance (WAKE-RUNBOOK step 6).
- **Docs/state** — agent **FP** (this receipt): STATE-L26-WAVE.md §9
  (appended; earlier sections untouched), WAKE-RUNBOOK-20261010.md,
  CLAUDE-ROUND3-RECEIPT.md, agent-FP-runbook.md. Status: **landed**.

## Console state while all of the above runs

REST MODE. PRX efbfc656 resident (fake-signed 4f153d1d, decrypt-off readback
verified), zzdiag v10 loaded, settings ORIGINAL cache-off (1947 B). At wake:
expect klogsrv dead (died in a sleep cycle tonight; restart command in the
runbook) and ploader timed out (≤120 s Z2 window) — runbook step 1 restores
both, never double-dispatching.

— agent FP (ZCode), 2026-10-10.
