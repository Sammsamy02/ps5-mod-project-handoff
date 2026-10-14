# PRODUCT-ROADMAP.md — the five milestones after Item Spawner

worker-product-roadmap, 2026-10-04. Planning deliverable; no console runs, no code
changes. Basis: `2026-10-04-zcode-controller/ZCODE-FORK-HANDOFF.md`,
`worker-checkpoint-reconcile/CURRENT-STATE.md` (~21:30Z snapshot),
`worker-risk-update/RISK-REGISTER-V2.md`, `worker-acceptance-evidence/EVIDENCE-SPEC.md`,
`2026-09-30-console-only-roadmap.md`, `2026-10-02-native-nexus-acquisition-{plans,queue}.md`.

---

## 1. Product goal (from the fork handoff / console-only requirements)

A finished private Nexus mod-manager app that runs **entirely on the PS5**: account
sign-in, catalog navigation, authorized downloads, archive options, analysis,
conversion, caching, installation, removal and validation. A computer may build and
test the app once; **no computer, phone or AI service may be required to prepare or
run a supported mod**. First test title selected at test time (today: Townfall,
UE 5.6.1 SHIPPING). User-facing classification must stay honest: Supported /
Needs conversion / Unsupported / Failed — "Supported" only with an implemented PS5
backend plus evidence for the applicable game/build/mod family.

## 2. Where we are: the Item Spawner milestone (M0, in flight — not claimed)

The current milestone is the user's acceptance ladder: genuine game-thread callback →
valid object query → harmless reflected access → **unchanged Item Spawner activation**
→ visible spawn through real input (`EVIDENCE-SPEC.md`). Status today: identity/axes
resolved, GUA live-validated, FINAL staged pair stageable; rungs 1–5 have a complete
evidence contract but **no rung is PASSED yet**. Blockers on the M0 path
(R-COMP4-01 composition gate, R-D1-01 console staging of the relinked payload, Gate
A0 write probe, pad `hardware_runtime_validated:false`) are shared prerequisites for
M1 below; they are not repeated here as separate milestones.

Everything below is defined **relative to M0 being accepted**: one real mod
(Item Spawner) provably installs, activates, and visibly works through real input on
the console.

## 3. Prioritization (user-specified order a–e)

1. **M1 — Full E2E console run** (a): proves the product's spine end to end before
   breadth is added. Uses the owner-placed-archive route first, deliberately
   decoupling lifecycle proof from live-Nexus risk (matches RISK-REGISTER-V2
   reading-order items 1–5).
2. **M2 — Multi-mod support** (b): the app becomes a *manager*, not a one-mod demo.
3. **M3 — Real Nexus download on console** (c): removes the last computer-owned leg
   (archive acquisition) and the "download_authorized:false / owner-placed zip" stopgap.
4. **M4 — Additional games/engines** (d): scales the proven pipeline to a second
   title without title-switched code.
5. **M5 — App polish for daily use** (e): turns a working pipeline into something a
   user tolerates daily.

Rationale for M2 before M3: multi-mod exercises install/activate/uninstall cycles,
records, and disk budgets that M3's downloaded archives will immediately stress;
doing M3 first would mean proving the Nexus leg against a lifecycle that can only
hold one mod.

---

## Milestone 1 — Full E2E console run, zero developer intervention

**Delivers to the user:** from the PS5 app alone — register a mod archive (owner-placed
zip), prepare it, install it into the game, activate it, launch, play with it, then
uninstall and verify clean state — with **no developer hands and no developer tools**
in the loop. The app's `installation_supported` flag flips from `false` to evidence-backed
`true` for one game + one mod family.

**Technical work required:**
- Console execution of the already-designed staging plan: Gate A0 FTP write probe →
  pre-image backups → STOR ×9 → per-file RETR readback (MASTER-STAGING-PACKAGE;
  R-G3-01, R-FTP-02 rules).
- Stage the D1-relinked payload ELF (R-D1-01 residual) and the composition-4-fixed
  runtime; config-file-driven entry `/data/modbridge/intake/e2e-cmd.json` instead of
  argv (G1 mitigation for R-G1-02).
- Bind worker-8's app backend actions (register/prepare/install/uninstall) to the
  composed CLI **on console** — the named R-ACT-01 controller integration step.
- Run E2E stages 0/2a/2b/3/4-5/6 per `worker-e2e-rehearsal` GO conditions: install →
  activate (C3 runtime ack reaching `mods-started`) → uninstall → clean-state check
  (libc dirent/fsync durability answered by stage-3 rollback probes, R-LIBC-01).
- Make the app's TSV records store the single source of state transitions during the
  run; capture C1–C4 evidence per stage.
- Failure-path rehearsal: at least one induced failure per stage with rollback proof.

**Already built vs new:**
- Built (host-proven, none of it advances a rung): installer vertical slice 108/108;
  composed payload 116 imports; F-R1 zip→mfb bridge 75/75 (console-linked); F-R2 exit
  shim 87/87; uninstall D1 journal fix 186/186; E2E rehearsal 61/61 GO; host-side
  prepare→records closure; app backend 61/61 real-binary tests; staging manifests.
- New: **all on-console execution of the above**; app↔CLI console binding; clean-state
  verification on the real filesystem; the "no developer intervention" definition-of-done
  run performed by the user alone.

**Dependencies:** M0 acceptance (ladder rungs through activation), composition-4 fix,
D1 payload staging, Gate A0, signed-in owner session only if the download leg is
included — deliberately excluded by using the owner-placed-archive route first.

**Complexity: M–L.** ~1–2 weeks team effort, 3–6 console sessions. Most parts exist;
the cost is console-side iteration under the standing channel risks (R-E2E-FR3,
R-FTP-01/02, R-PLD-01) and the one-launch-per-staged-build ritual.

**Exit criteria:** one continuous user-only session (no dev tooling) produces C1–C4 +
records evidence for register→prepare→install→activate→play→uninstall→clean-state;
`installation_supported:true` published in capability JSON with the receipt; every
failure mode exercised has a recorded rollback.

---

## Milestone 2 — Multi-mod support

**Delivers to the user:** install, enable/disable, update and uninstall **several**
mods on one game from the app; the mod list UI becomes real (list, state, per-mod
actions, disk use); dependencies between mods visible and enforced where known.

**Technical work required:**
- Generalize the install/activate/uninstall chain from a fixed staged bundle to N
  mods: per-mod directories under the runtime's mod root, per-mod `enabled.txt`,
  activation order, and per-mod records transitions (records store already models
  transitions per mod — needs N-mod concurrency rules).
- Collision and conflict policy: duplicate files, load-order-sensitive content,
  shared container (utoc/pak) overrides — user-facing Supported / Needs conversion /
  Unsupported / Failed per the console-only contract, each with an actionable reason.
- Disk budget and capacity gates for N archives (acquisition queue already enforces
  group capacity — reuse for installs).
- App UI: mod list, per-mod lifecycle actions, state surfacing from the records store;
  re-verify UI files against the authoritative worktree first (records/UI audit
  provenance caveat, REFILL-QUEUE Q11).
- Apply the DLL-mod retention criteria (`worker-dll-mod-retention`) so translated-DLL
  mods follow the same lifecycle once the constrained-DLL family lands.

**Already built vs new:**
- Built: records store with exact per-mod transitions; zip→mfb bridge handles generic
  archives (75/75); UE4SS runtime already injects multiple plugins (ploader shows
  "injecting 2 plugins"); acquisition closure supports up to 8 mods as a group;
  container capability matrix + utoc header spec; pak index delta settled (4699 vs
  4544 never cross-applied).
- New: N-mod install/uninstall state machine on console, conflict detection and
  classification, activation ordering, the list UI, multi-mod E2E regression.

**Dependencies:** M1 (the lifecycle being multiplied must be proven first).

**Complexity: M–L.** ~2–3 weeks team effort, 2–4 console sessions. Mostly app/state
work plus one console-heavy conflict-matrix shakedown.

**Exit criteria:** ≥3 simultaneously installed mods (e.g. Item Spawner + two passive
Lua mods) install, coexist, toggle independently, uninstall in any order, and leave
clean state; one documented conflict case classified with an actionable reason.

---

## Milestone 3 — Real Nexus download on console

**Delivers to the user:** browse/pick a mod, sign in to Nexus **from the console**,
and let the app perform the authorized download itself — removing the owner-placed-zip
stopgap and the last computer-in-the-loop step. The full "download→prepare→install→
activate→uninstall" spine becomes literally true with no sideloading.

**Technical work required:**
- Console-side authenticated Nexus session: live OAuth/PKCE against production Nexus
  — **explicitly untested today** (acquisition plans are proven against an *owned*
  HTTPS provider; live OAuth scope and production GraphQL remain untested). TLS path
  on console: mbedtls/native-https spike lineage must be finished and receipted.
- Wire the acquisition-plan + acquisition-queue resident-app modules (35-test selector
  suite, 55-check app suite, entitlement/visibility/link-authorization per worker)
  into the on-console app; keep `download_authorized` computed per transfer, never
  inferred.
- Activate the F1 native download ELF on console (linked, 123 imports / 0 missing;
  staged-deferred pending a signed-in owner session, one-job-per-process) through the
  curl-owner v1 interface bound host-side in `worker-e2e-final` — needs the console
  binding done for real.
- Transfer policy: retry/resume bounds, deadline/cancellation, unique per-attempt
  destinations (R-FTP-02 rule), entropy/storage admission gates already designed.
- Feed downloaded archives straight into M1's prepare→install stages; no interim
  computer step.

**Already built vs new:**
- Built: bounded prerequisite closure + source-identity selection (1–8 mods, 128
  relations), deterministic prerequisite-first plans, idempotent acquisition jobs API,
  PKCE sign-in flow (host), native download ELF linked, F1 RDWR semantics closed,
  curl-owner v1 interface, honest refusal of caller-supplied URLs/credentials.
- New: production Nexus connectivity from the console (OAuth, TLS, GraphQL), the
  signed-in owner session procedure, real archive bytes flowing through the F1 ELF
  on hardware, download→prepare console handoff.

**Dependencies:** M1 (staged payload + intake gates), M2 recommended (multi-mod
acquisition groups map onto multi-mod installs), user cooperation for Nexus sign-in
(credentials/MFA stay user-controlled per standing rules).

**Complexity: L.** ~3–5 weeks team effort, 4–8 console sessions. Highest external
uncertainty of the five: production API behavior, TLS on this firmware, and
network policy on hardware are all first-contact items. Plan bounded probes with
fail-loud records; do not script blind retries.

**Exit criteria:** one console-only session: sign-in → select mod (+ prerequisites) →
authorized download of real archive bytes → prepare → install → activate → uninstall;
every non-ready plan produces an actionable reason with zero partial downloads.

---

## Milestone 4 — Additional games/engines

**Delivers to the user:** a second game (and mod family) classified Supported with
evidence — or an honest Unsupported/Needs-conversion verdict with the exact missing
backend named. The manager stops being a Townfall appliance.

**Technical work required:**
- Produce a second title's engine/build profile end-to-end through the *existing*
  evidence pipeline (identity scan → axes → layout correspondence → staged pair →
  harness rungs) with **zero title-switched code** — the console-only contract's core
  test. Any per-title constant must be data (profile), never a source branch.
- Reuse the container layer: utoc/pak/iostore readers exist; close named gaps only
  where the second title requires them (Zen TOC signing, Oodle presence, encrypted
  fixtures — currently unexplored, R-CONTC-01; do not guess byte semantics).
- First target: a second UE title (highest reuse); Bethesda bsa/ba2 and me3 file
  overrides follow the header-inspection spike + conversion path
  ("Needs conversion" via verified on-console converter).
- Extend the mod-family matrix (assets/params, UE4SS Lua/Blueprint first; ReShade /
  texture-overlay via the reversible overlay path later), each family gated on an
  implemented PS5 backend with evidence.

**Already built vs new:**
- Built: game-independent selection design; evidence-based profile framework proven
  once (Townfall: identity/axes/GUA/staged pair); container readers; UE4SS Lua
  portability study; Bethesda header inspection spike; runtime + installer chains are
  title-agnostic by design.
- New: everything second-title: profile evidence, any new container variants, family
  backends, a second acceptance-ladder campaign on hardware.

**Dependencies:** M1–M3 stable (the pipeline being generalized must not be moving);
per-title user-provided test title and save; the profile pipeline's per-game cost is
the dominant unknown.

**Complexity: L–XL.** 4–8+ weeks team effort for the first second-title; console
sessions comparable to the Townfall campaign (the layout-correspondence campaign was
the long pole the first time). Honest note: the Townfall profile took the team a full
session arc even with the machinery built; a second title proves the *process*, and
each new *engine family* (non-UE) is effectively a new project slice.

**Exit criteria:** second title boots with ≥1 mod installed/activated via the app,
classified Supported with cited evidence; classification table in the app reflects
per-title/per-family status with actionable reasons for the rest.

---

## Milestone 5 — App polish for daily use

**Delivers to the user:** an app a person chooses to open daily: responsive progress
instead of synchronous waits, understandable errors in user language, crash-safe
resume, validation results with rollback state, coherent sign-in/catalog/browse flow,
and honest capability reporting.

**Technical work required:**
- Async job model with progress + cancel (current worker-8 backend is synchronous,
  cancel is a no-op for terminal jobs); stream long operations instead of monolithic
  calls (R-BOUND-01 streaming variant is post-E2E — commission on real corpus demand).
- User-facing error taxonomy mapped from internal refusal/unresolved states; never
  silently convert unresolved/evidence states into Supported.
- Install-time validation + Failed state with rollback state reported separately
  (console-only contract §3); boot-health surfacing that never equates "booted" with
  "mod works".
- Re-verify and then finish the records/UI audit on the authoritative worktree (Q11);
  UI polish beyond `modbridge-ui-preview.png` (layout, onboarding, log viewer).
- Durability: fsync/dirent behaviors (R-LIBC-01) encoded in app flows; orphaned-job
  reconciliation after crashes; watchdog adoption for app-driven heavy processes
  (R-RW-01 discipline).
- Packaging/versioning and a controlled update story for the app itself.

**Already built vs new:**
- Built: modforge-service/1 ABI, records store, capability JSON, honest-refusal
  patterns throughout, UI preview, worker-8's 61/61 backend suite.
- New: async/cancel/progress, error taxonomy, validation UX, audit-finished UI,
  durability hardening, update path.

**Dependencies:** runs best after M1–M3 (polish against the final shape, not a moving
target); individual items (async jobs, error taxonomy) can start earlier in parallel.

**Complexity: M.** ~2–4 weeks team effort, 1–3 console sessions. Low research risk;
the discipline risk is keeping honesty surfaces (unresolved → actionable reason)
while making everything friendlier.

**Exit criteria:** a non-developer completes register→uninstall for a multi-mod setup
using only the UI, with progress, cancellable long steps, and a correct Failed+
rollback report on an induced failure.

---

## 4. Summary table

| # | Milestone | Priority | Complexity | Team effort (honest range) | Console sessions | Key risks |
|---|---|---|---|---|---|---|
| M0 | Item Spawner ladder accepted | current | — (in flight) | blocked on R-COMP4-01/D1/Gate A0 | 2–4 | R-EXEC-01, R-PAD-01 |
| M1 | Full E2E console run | (a) | M–L | 1–2 weeks | 3–6 | R-FTP-01/02, R-E2E-FR3, R-ACT-01 |
| M2 | Multi-mod support | (b) | M–L | 2–3 weeks | 2–4 | conflicts/classification honesty |
| M3 | Real Nexus download on console | (c) | L | 3–5 weeks | 4–8 | prod OAuth/TLS first contact, R-DL-01 |
| M4 | Additional games/engines | (d) | L–XL | 4–8+ weeks | ~campaign-scale | per-title evidence cost, R-CONTC-01 |
| M5 | App polish for daily use | (e) | M | 2–4 weeks (parallelizable) | 1–3 | honesty-surface erosion |

Sequencing: M1 → M2 → M3 → M4 → M5, with M5 items pulled forward opportunistically.
M3 could swap with M2 if the user needs real downloads sooner, at the cost of proving
Nexus against a one-mod lifecycle.

## 5. Honesty notes

- Every "built" claim above is **host-side or component-side evidence**; per the
  global NON-EVIDENCE rules it does not advance any console rung and does not make
  the product feature-complete. `installation_supported` and `download_authorized`
  remain `false` until the console runs in M1/M3 flip them with receipts.
- Effort ranges are engineering judgments against this team's observed cadence
  (12-context cap, one-launch-per-build ritual, Rosetta wedge discipline), not
  calendar promises.
- M0 is listed only as the dependency anchor; its completion is **not claimed** and
  is not a product milestone deliverable of this roadmap.
