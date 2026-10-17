# Cap #10 (managed / C# mod host): independent review

Reviewer: one independent reviewer. Date: 2026-10-12/13. Everything here is **Mac-only**. I made no console contact
and edited nothing outside this file. I created the `cap10/` folder only to hold this file.

## Verdict: BLOCK (there is nothing to integrate)

## What I checked

- Before this review, the team folder `caps-team-F/caps-team-work/cap10/` **did not exist**. I checked with `ls`
  before writing this file. Only `cap12/` was under `caps-team-work/`.
- PATCH/, APPLY.md, tests/, LAUNCH-CARD.md, NOTES.md: **all missing**. That matches what the team said.
- Host tests re-run: **0 found, 0 run, 0 passed**. By CLAUDE.md section 4, a check that scans zero items counts as a
  FAILED gate, not a pass.
- Patch apply / compile / breakage / hard rules (generic code, no detours, fail closed, two sources, no SDK, no
  console, mods untouched): **nothing to check**. Since nothing was written, no rule was broken.
- Launch card safety: **no card**. Rollback stays R11 bfd95db6.

## Findings

1. **BLOCK. No deliverable.** There is no staged patch, no tests and no launch card. The integrator must not plan a
   #10 launch from this team.

2. **FIX-SOON. The team's main reason for stopping is out of date.** The team says "#10 is excluded for now; no
   current mandate". The current CLAUDE.md says otherwise:
   - GOALS (lines 23-24): "#10 (C# runtime) BACK IN SCOPE (user, 2026-10-13): goes last and never holds Stellar Blade".
   - Section 0 (line 37): console order ends with "... #12, #10".
   So #10 is in scope with the lowest priority, not out of scope. Mac-side prep is allowed now, because section 0
   says teams A-F prepare caps 1-12 in parallel. A new dispatch is needed. "Leave it until the Unity phase" is no
   longer the rule.

3. **NOTE. The team stopped after a safety check, and that needs a fresh start, not an assumption.** Nothing in
   the team's result says what was being read. A new run should start from ChatGPT's cap10 prep
   (`outputs/2026-10-12-caps-prep-chatgpt/cap10/`, 29 entries: CONTRACT-AND-PROOF.md, HOST-PLAN.json,
   assembly_preflight.py and others). Treat it as UNVERIFIED and reproduce every receipt. Its own STATUS-TABLE
   row 10 lists open blockers: "genuine owner/call site; exact signature/artifact admission; GC/provider proof" and
   "helper excluded from build". A real staged patch for #10 has to close those blockers or say exactly what
   unblocks each one.

4. **NOTE. The team's notes on the user's report are partly superseded.** The user said:
   *"the textures i saw on leon were just like checkered gray and white ... when i opened the door outside it was
   just bright as day ... u can see the monster very clearly"*.
   - **Bright outdoors.** The integrator already traced this, Mac-only, from files pulled read-only with the game
     closed. See `outputs/2026-10-08-l25-prep/evidence/console-pulls-20261013-paks/FINDING-bright-outdoors.md`.
     Of the 4,699 entries in the console's `pakchunk0-ps5.pak`, exactly one differs from the `.bak`:
     `Townfall/Platforms/PS5/Config/PS5Engine.ini`. The changed file has `r.VolumetricFog=0`,
     `r.EyeAdaptationQuality=0`, `r.TonemapperGamma=1.3`, shadows cut back, Lumen reflections off, and a PC-only
     DX12 key. The team guessed the lights or fog "failed to load or was replaced while the mod was active". That
     guess points the wrong way: the Leon mod did not cause this. A changed **base-game pak** on the console did.
     Restoring the `.bak` is a console file change and needs the user's OK. **Worth chasing:** what wrote into a
     base-game pak on 2026-10-02? That would break the "only add mod paks, never change game files" model, so it
     belongs in the #7 mod-manager design.
   - **Gray/white checker on Leon.** The team's "texture data not found" is one possibility. Another, and in my
     view the more likely one, is this: Unreal draws a gray/white grid when a **material has no usable shader for
     the platform**. It then falls back to the engine default surface material. A PC mod pak only carries PC
     (D3D) shader code, not PS5 shader code. This is a Mac-only hypothesis with no second source yet. The local
     Epic tree has no Materials sources to cite. The pak work already has a design for this:
     `pak-overlay/UNSUPPORTED.md` section 7a, "Substitutions (L1/L2 shader fallback)". So it belongs to the #7/#8
     pak team (Team A, as the finding file says), not #10. To tell the two causes apart, the integrator needs the
     launch log decoded with `harvest/decode_session_log.py`, after the user confirms the game is closed. Look
     for material/shader-map warnings versus missing-bulk-data or texture warnings for the Leon package paths.

## What would unblock #10

- The integrator re-dispatches #10 under the current CLAUDE.md (in scope, last in order, never holds Stellar Blade).
- The new run delivers PATCH/ + APPLY.md + tests/ (non-zero item counts, saved output) + LAUNCH-CARD.md + NOTES.md,
  and then gets a fresh independent review.
