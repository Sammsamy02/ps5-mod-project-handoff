# agent-GJ — Stellar Blade blind-test prep (Claude round-5 A3a)

2026-10-08. Mac-only, battery mode: no builds, no console, no tree edits, no
fabricated measurements. All artifacts in `outputs/2026-10-08-l25-prep/blade-prep/`.

Deliverables: `per-version-table.json` + `PER-VERSION-TABLE.md` (D1),
`run-blade-profile.py` + `runs/` receipts (D2), `BLIND-TEST-KIT.md` (D3),
`TEST-PLAN.md` (D4), `PREREQ-CHECKLIST.md` (D5).

## 1. Table coverage (measured vs seeded)

- **1 measured row: UE 5.6 (5.6.1-release, Epic commit 6978b63c…)** — pinned
  from the local official-source receipts
  (`outputs/2026-10-07-l19-r4-official-engine-source/SOURCE-RECEIPT.json`,
  the vendored-engine-headers answer to "pin Townfall's exact minor
  version"; the profile's `engine_revision` 5.6.1-0+++UE5 agrees). The row
  carries the FULL measured fact set with receipts: FNamePool shape
  (block=id>>16, offset=(id&0xFFFF)*2, cursors +0x8/+0xC, Blocks[] +0x10,
  u16 header wide=bit0/len=>>6, capacity 0x20000, 8192 blocks, EName table
  +0x14040), GUObjectArray (FUObjectItem 24 B, Object@0/Flags@8; FUObjectArray
  + TUObjectArray field offsets; chunked-65536), FName ctor slot-2 ABI + 32-B
  prefix + wildcarded generalized pattern, ToStringInternal + SCO + listener
  prefixes/labels, FMallocBinned3 17 measured vtable slots (+3 explicitly
  unmeasured slot ranges named).
- **6 seed bands, confidence 0.0, TRAINING-KNOWLEDGE-marked**: 5.4-5.5,
  5.0-5.3, 4.23-4.27, 4.12-4.22, 4.7-4.11, 5.7-5.8 (forward rows keep the
  version axis continuous). Each carries expected-deltas + a confirm_when
  evidence list; the generator NEVER emits seed values as pins.
- Honesty laws baked into the JSON: labels are per-image measurement
  provenance (never transferable between games, even same engine version);
  seeds only steer scanning; live base observation required (ASLR).
- Version axis itself: the 244-marker patternsleuth table (4.7-5.8, 30
  versions) is the classifier; positive control 5.6/shipping/stats-off
  margins reproduced byte-exactly.

## 2. Generator readiness — tested NOW on both local binaries

`run-blade-profile.py` (611 lines): path in → axes (ref_axes subprocess, the
pinned C+Python-agreement machinery) → table join → anchor arms → verdict +
profile JSON + per-pin confidence, or REFUSAL. Zero game names in logic
(Rule-10 grep audit: 0 game-name string comparisons; names appear only in
docstrings/provenance).

- **Positive (Townfall decrypted eboot, sha 3b6cfad4…): ACCEPTED** — engine
  5.6/shipping/stats-off with margins identical to the retained profile
  receipts; FName-ctor exact prefix hit at 0x23D590, ToString 0x23FA00, SCO
  0x490AA0 (all expected-label hits TRUE); UTF-16 census matches SeedTable
  exactly (Engine/Renderer/AnimGraphRuntime/Landscape/RenderCore unique,
  FMallocBinned3 ×4); the new LEA-xref-census arm reproduces the SeedTable
  receipt EXACTLY: GUObjectArray label 0x93BA110 = 1565 lea + 0 mov
  rip-refs. Emitted profile: schema-1, 20 member pins + 17 vtable pins from
  the measured engine row, base=unresolved (live observation law).
- **Negative (Astro Bot reconstructed ELF, sha 7723d150…): REFUSED** —
  0/244 markers, the refusal path fires as designed (FZ's dry-run result
  reproduced); the Sony-signed `.self` also refuses at container stage with
  the decrypt-off-law message.
- **Correction discovered and fixed during testing** (honesty note): FZ's
  dry-run table implied the 9 GUO/5 GM patternsleuth patterns multi-hit
  Townfall; the pinned record (worker-required-slots-feasibility REPORT.md)
  says the opposite — 0/9 and 0/14 exist image-wide (compiler idiom drift).
  My first bar wrongly refused the positive control; the runner now
  implements the arms that ACTUALLY pinned Townfall (xref census + prefix
  consensus + UTF-16 census) and keeps the shipped patterns as probes.
- **Honest limits**: (a) "top lea target" is NOT a unique GUO discriminator
  (hot globals outrank it) — the census emits a candidate POOL; unique
  selection is explicitly deferred to live shape validators + two-source
  law; (b) the GMalloc deep derivation route (literals→RELA vtable→bootstrap
  store) is not reimplemented — GMalloc pins stay low-confidence with the
  route flagged (console V1/V2 validators gate slot-3 arming anyway).

## 3. The 5-mod blind-test pick (engine-level preferred; zero pad rows)

Of the 13 staged mods, 7 are engine-level by design (the stock UE4SS set),
3 are Townfall-specific, 3 fail exclusion laws (ActorDumper hardcoded 0x98
offset; jsb inert/profiler; Keybinds C++-surface). Picks:

| Type | Pick | Note |
|---|---|---|
| event/hook | **CheatManagerEnablerMod** (1,407 B) | Engine.PlayerController:ClientRestart; loud per-owner markers |
| value/array-write | **ConsoleEnablerMod** (3,173 B) | ConsoleKeys array-growth struct write on Engine InputSettings CDO + built-in read-back print |
| config-io | **ueselftest io rows + ConsoleCommandsMod** (15,549 B) | engine-level io proven by self-test; mod-level `io.open("w+")` reachability-caveated (needs console surface) — else N/A honestly |
| sustained-tick | **StaminaMod** (2,811 B) | LoopAsync(1500) registration+survival; body no-ops by design; Townfall class names only in dead branches (never-edit-mods) |
| menu-class | **N/A-for-blind-test (honest) + proxy** | no game-agnostic UMG menu mod exists in the 13; ConsoleEnabler's UConsole SCO+Set is the engine-surface proxy + ueselftest |

shared/UEHelpers (9,962 B) stages as a REQUIRED dependency (Blade's mod root
starts empty). First reserve: LineTraceMod (engine-level, needs one pad row —
kept out to preserve PS5Pad.ini byte-identity).

## 4-5. Test plan + prereqs (files, summaries)

TEST-PLAN.md: GI-not-landed noted; manual first-launch = profile(ACCEPTED)
→ unmodded baseline boot → ueselftest combo (0-FAIL critical families bar)
→ mod markers → 3-pass law (byte-identical staging, clean exits, build
change resets count). Failure-triage law: every failure → engine/version-
level fix in runtime or table, NEVER a Blade special case (owner table
included). Rollback: mod-root rows removal, frozen-known-good PRX restore
with decrypt-off readback, Townfall 3-launch regression after any runtime
change.

PREREQ-CHECKLIST.md: user's own dump commands documented NOT performed
(default-connection decrypt-on download + `-Q "SITE SELF"` provenance
cross-check, stage under a new observations dir, never re-upload); disk
~0.5 GB needed vs 106 GiB free; tooling gaps = engine version UNCERTAIN
until classified (PAK v11/IoStore v3 do NOT identify it), PRX build later
on mains, GI automation pending, GG mod root for PPSA13197, licensing audit
gate for any release.

## Open items handed forward

1. GI schema reconciliation when FIRST-LAUNCH-AUTOMATION.md lands (table
   laws block is the invariant contract).
2. GMalloc deep-route arm if static GMalloc pinning is ever required pre-
   console (today the console validators own slot 3).
3. If Blade classifies ≠ 5.6: seed-row refusal is BY DESIGN; the table
   upgrade campaign on the Blade binary becomes the next priority-(a) work.
