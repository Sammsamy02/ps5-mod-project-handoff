# REPORT.md — worker-session-docs-final-sync, 2026-10-04 (written last)

Documentation-only final sync of the session doc trilogy to the FINAL batch tool. The tool is
FROZEN and untouched (in-place `worker-console-batch-runner/console_batch.py` re-hashed at
`b00b3a69…`, byte-identical to `worker-batch-final-ini-adoption/console_batch.py`, whose own
usage string and source were the sole ground truth). No console contact, no nested subagents.

## Deliverables (this directory, `outputs/2026-10-04-zcode-team/worker-session-docs-final-sync/`)

- `CLAIM.json` (written FIRST) — scope, synced-against facts, planned edits, as-found hashes.
- `EDIT-LOG.md` — every in-place edit (14 master-plan, 19 decision-brief, 5 user-guide) with
  before/after/why, plus the deliberately-not-edited list.
- `as-found/` + `diffs/` — as-found snapshots (sha-pinned in CLAIM) and unified patches
  (RETURN-SESSION-MASTER-PLAN 122→ lines, CONTROLLER-DECISION-BRIEF 190, USER-LAUNCH-GUIDE 54).
- Synced docs IN PLACE (controller-synced artifacts, not tool files):

| Doc | as-found sha256 | synced sha256 |
|---|---|---|
| `worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md` | `1fc69f32…` | `0ab2c2aa…` |
| `worker-controller-decision-brief/CONTROLLER-DECISION-BRIEF.md` | `285b8a8c…` | `2a35d322…` |
| `worker-user-launch-guide/USER-LAUNCH-GUIDE.md` | `c1f4ce7e…` | `b1dd6d69…` |

## What was out of sync, now fixed

1. **Master plan Phase-2 step list** — was: dll8, e2e, `stage-qdrv1` (single-PRX append), a MANUAL
   observer ridealong marked "NOT yet in the tool", and a MANUAL settings restage that froze against
   a pre-v2 pair (779 B `9f3fa127…`). Now: dll8 → e2e → `stage-ridealongs` (FINAL 8-entry ini adoption
   incl. the **INI-BOOTSTRAPPED** one-shot whole-file STOR branch, exactly **7 registered**, THEN the
   FOUR PRX uploads — matching the tool's real ride-step order ride.2b–4 before ride.6–9) →
   `stage-settings` (freeze against the recorded v2 inventory 834 B `9570ead3…` + 446 B `3afd10ce…`,
   v3 lockstep upload, byte-exact readback 1448 B `d8faa4ec…` + 447 B `4d98ecd7…`). Also fixed the
   false "`all` would reach the launch gate too early" rationale (the final tool's `all` includes both
   staging flows and blocks at the arm-ploader gate), the Phase-3 sink list (seven sinks + ge.3b),
   the rollback pointer (`rollback-ridealongs`, v2 rollback pair), and the stale
   prerequisites/command cross-check (freshness gate APPLIED, 8fam receipts LANDED, observer staging
   LANDED — all owned by the recorded chain).
2. **Decision brief command table + ini-bootstrap branch** — §7 now lists the exact final CLI
   (plan + 10 flows + selftest; `--dll 7|8`, `--dry-run`, `--assume-yes`, `--run-root`,
   `--continue-on-failure`, plan `--only`), sha `b00b3a69…`, selftest **372 ok / 0 failed** (fake
   drive 90/0); the "observer staging absent (grep clean)" rows are flipped to PRESENT
   (`stage-ridealongs`, in-tool SHT-strip law with host recompute at ride.1); §3.3 rewritten to
   `stage-ridealongs` with the new verdict class **INI-BOOTSTRAPPED** (`ini-bootstrap-verdict.txt`;
   decision: sanctioned one-time rewrite — ADVANCE and record, never a second whole-file STOR,
   EXPECTED branch on the Track-8 pre-image; ensure-only / AMBIGUOUS-ini / readback-FAIL branches
   kept); §3.4 is now `stage-settings`; old manual sections folded into a "legacy fallback only"
   §3.5. Count fixes: "count == 5"/"simulate count ≠ 5" rows (old 6-entry law) and "injection of
   5 plugins" → **7 registered / 7 plugins** with the EXACTLY-7-`OK (<ms>)`-line, ZERO-FAIL
   post-launch expectation; ge.2 "five sinks" → **seven evidence sinks** + new ge.3b bullet; CP5
   shape (a) restage-proof sha anchor moved to `d8faa4ec…` (v3). Standing-law item 7 no longer says
   "NEVER `all`".
3. **User guide handoff moments** — settings staging is now explicit controller work that completes
   BEFORE the launch, and the on-screen post-launch gate is stated as exactly **seven OK lines and
   no FAIL line** (§1, §4, §5, §7, footer). Plain-language register preserved; no tool jargon added.

## Verification (all green)

- **Commands**: every `console_batch.py <sub>` mention in the trilogy was checked against the
  frozen tool's own usage string/subcommand set — all exist (the single scanner hit,
  "console_batch.py freshness gate", is prose, not a command). Documented flags (`--dll 7|8`,
  `--dry-run`, `--assume-yes`, `--run-root`, `--continue-on-failure`, `--only`) all exist.
- **Counts**: 7 registered (master-plan gate + step 3; brief §0b/§3.3/§3.4/§5; user guide "seven OK
  lines") matches `FINAL_INI["registered_expected_final"] = 7`; 10 flows (brief §0b + §7) matches
  the `FLOWS` map; seven evidence sinks matches ge.2's own wording; 372/0 matches the adoption
  receipts. No stale "5 registered", "five sinks", "779 B `9f3fa127…`", or "NEVER `all`" strings
  remain in the trilogy.
- **EVE-STATE first-five-commands**: UNCHANGED and still valid — nothing was renamed in the final
  tool (dll8/e2e keep exact names and flags; the two curls and `comp6_one_pass.py` are outside the
  batch revision; comp6_one_pass.py confirmed still on disk). No edit needed; EVE-STATE's
  quick-reference lines (7 registered, v3 settings 1448/447, CP2 tick `0x33ac8c0`) already match.
- **Read-through**: full pass over all three docs post-edit; numbering (Phase-2 steps 1–5 preserved
  through Phase-3 step 11), gate lines, and the brief's §3.3→§3.4→Phase-3 cross-references are
  coherent.

## Flagged, not fixed (outside the trilogy or non-tool facts)

1. `EVE-STATE.md` "Then (not in the first five…)" paragraph still narrates `stage-qdrv1` + separate
   observer/FINAL-ini adoption steps — the final tool folds those into `stage-ridealongs`. Cosmetic;
   its counts and shas already match. Owner: worker-session-eve-coherence if a v2 is wanted.
2. Master plan Prereq 6 (patches.json 51→54 "[PENDING worker-e2e-suite-completion]") vs EVE-STATE's
   "promotion: APPLIED" — not a batch-tool fact; left per the brief's own stale-receipt rule
   (re-verify bytes; trust the newest verified state, here EVE-STATE).
3. The tool-shipped `USER-SESSION-GUIDE.md` (worker-console-batch-runner + identical adoption copy)
   was already rewritten by the adopter for this exact revision — verified current, untouched.

## Honesty boundaries

No console contact; nothing executed on a console; all console constants remain [TARGET] until the
session runs. All timing figures are the docs' original host-side estimates (none invented). The
sync asserts only what the frozen tool source and its terminal receipts state; no >8-quality or
full-parity claims are made anywhere.
