# POSTPOOL-CANDIDATE — the complete candidate set for the launch AFTER the pool validates

worker-postpool-candidate · 2026-10-06 · PS5 UE4SS runtime port
Status: **PROPOSAL-ONLY STAGING.** Byte-verified copies and one mechanical diff
re-derivation; no shared-lineage edits, no composition run, no packaging, no
console contact, no game or in-game claims. Admission/composition/console
steps are controller- and user-owned. All log lines below are PREDICTIONS of
evidence shape from pinned sources — never captured results.

Workspace: everything under
`outputs/2026-10-04-zcode-team/worker-postpool-candidate/`
(`staging/`, `diffs/`, `settings/`, `manifest/`, `provenance/`, `work/`).

---

## 0. The launch ladder this candidate belongs to

1. **Launch 1 (already staged, user FTP-uploads when the PS5 wakes):** pool-
   validation candidate **FSELF `12fcc41f`**
   (`2026-10-04-zcode-controller/launch11-combined/prx-candidate-l11-2/`,
   bare `b23deb9b…`, sdk `6008bbc6…`, bridge.o `52b6b620…`, combined.o
   `ca6e7285…`; GO x2, determinism `6f215d89`). Expected: the capture-first
   initializer (`782dce71…`) validates the DM-class pool admission, the
   seeds complete, engine init begins, and the boot stops at the walkguard
   gate **GATE-1** (`Finding GameEngine::LoadMap`).
2. **Launch 2 = THIS candidate** (the "maximum evidence" set, §2): same
   boot class — GATE-1 stays armed (fold-0 walkguard `694af016…`) so the
   boot still stops at the walkguard gate — but every step now leaves more
   evidence (game-identity tags at module_start, in-session service alive
   after the refusal, richer pool-admission provenance), and the input-tag
   unit is aboard for the first boot where the event loop exists.
3. **The M-E probe (zero rebuilds):** the `HookLoadMap=false` settings
   variant (`a4b524a6…`) relocates GATE-1 from the LoadMap resolve to the
   GameEngine::Tick resolve. It does NOT pass; it names the next gate (§5).
4. **Launch 3+ (not this candidate):** T2 verdict → M-A measured fold flip
   (`RC_ASM_WALK_RX_GUARD 0→1`, object `f5627427…`) and, independently, the
   facf993a LuaMod once worker-r3-import-fix lands (§2 pending slot).

---

## 1. Answer to the settings question (dispatch item 2)

**YES — the current staged settings ini `b980cea7…` has `HookLoadMap=true`
(line 63).** Verified byte-identical against the on-disk pre-image
(`/tmp/ppsa33286-preimages-next/UE4SS-settings.ini.pre`, sha256
`b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`, 1,947 B).

The key is LIVE in the composed runtime (one stale worker note claims
otherwise; the sources say otherwise):
`REGISTER_BOOL_SETTING(Hooks.HookLoadMap, section_hooks, HookLoadMap)`
(`ue4ss-native-settings-defaults-2/src/SettingsManager.cpp:271`) →
`config.bHookLoadMap = settings_manager.Hooks.HookLoadMap;` (composed
`UE4SSProgram.cpp:1242`, unconditional path; the whole Hooks family
:1240-1254) → initializer `782dce71…` :2159 `if (UnrealConfig.bHookLoadMap)`
→ :2161 `Finding GameEngine::LoadMap` → the first exec-text walk.

Prepared variant: `settings/UE4SS-settings.ini.hookloadmap-false`
(sha256 `a4b524a602b2d2f1075d3d217089e85539be8e1c7f75e8175035991df77cd899`)
— ONE-line delta, line 63 `true`→`false`, nothing else moved (no inline
comments added; the parser corrupts inline-`;` values). Full law + expected
shape: `settings/SETTINGS-VARIANT.md`. Console handling (for the
controller/user): RETR-verify `b980cea7…` first, STOR `.next` + RNTO
(atomic), `.bak`, byte-exact readback `a4b524a6…`, restore law per
worker-regression-ledger.

---

## 2. The complete unit list with shas

### 2.1 What CHANGES vs the 12fcc41f working set

| # | unit | file (under this worker dir unless noted) | sha256 | base | evidence owner |
|---|---|---|---|---|---|
| 1 | **bridge + game-identity tags** | `staging/gameid-on-iocontrols-bridge/runtime_start_bridge_r14_diag_spanpub.gameid-497348b8.cpp` | `41dd0c153b902391fd4448c86bf93db6eca1b6de3d88c777e4ab0663d4ec6e11` | live bridge `497348b8…` | worker-gameid-unit (host 9/9 + 21/21 + round-trip) |
| 2 | **input-tag Handler TU** (G-AP1) | `staging/postpool-combined-luamod/Handler.cpp` | `16fcfe49436270760492cc56034e013fca5a66543ebcd449bf8e013cda00c3af` | admitted `f7539c60…` | worker-input-tag (host 96/96) |
| 3 | **input-tag PS5PadSource TU** | `staging/postpool-combined-luamod/PS5PadSource.cpp` | `d2f14de14d6a4697b2709fbe7c7298dc795b9b90553f981fd0be986892575865` | admitted `24692ec7…` | worker-input-tag |
| 3b | carry header (NOT a change) | `staging/postpool-combined-luamod/PS5PadSource.hpp` | `2d63c094bf8d3718c53e14087bbf304dcabd8d8ebcc4674513d8f7bce27bda33` | identical to coexist staging | — |
| 4 | **in-session service** (4 NEW units) | `worker-insession-refusal/staging/insession_cmd2_core.c` / `.h` / `insession_cmd2_service.cpp` / `.h` | `0fe3def3…` / `d98f99dd…` / `db8c74b5…` / `efe052c1…` | new files | worker-insession-refusal (host PASS 2026-10-06T04:27:48Z, 94 checks, 3/3 stable) |
| 4b | in-session wiring diffs (composition-time) | `worker-insession-refusal/staging/diffs/runtime_module_entry-insession2-hook.diff` (target `runtime_module_entry.c` `c97055f4…`), `runtime_owner-insession2-refused.diff` (target `runtime_owner.cpp` `54c56c49…`) | prose-form staged diffs | pinned targets | worker-insession-refusal |

Unit 1 derivation (full record: `staging/gameid-on-iocontrols-bridge/RE-DERIVATION.md`):
worker-gameid-unit's diff was written against `abeaa06a…`; the live bridge
has moved twice since. Reproduction control re-verified this session
(`abeaa06a…` + diff = `012fb485…` EXACTLY), then the same diff applied to
`497348b8…`: **exit 0, zero rejects, zero fuzz**; every inserted region
byte-identical to the host-tested form; call sites verified in place
(`bridge_gameid_emit()` :2014 after `supplier->capture()`, before the
lease-gap gate; `bridge_gameid_status_line(mod_root)` :2139 before
`MRS_OK`). Honest bound: the composition-level gates (UND delta = ∅
expectation, 196-strong import audit, fresh object measure, determinism
twin) are still OWED at compose time on `41dd0c15…`.

### 2.2 The "post-pool combined LuaMod" (dispatch item 1) — what it means

`staging/postpool-combined-luamod/` is the LuaMod-side source set of this
candidate: **LuaMod.cpp stays `c92c92ff…` byte-identical** (the deliberate
NOT-facf993a decision; the merged r3-ladder+gate5 file is blocked by the
R3Marshal cross-TU import issue — EDITIONS edit-20 records the three
packaging gates it trips; worker-r3-import-fix is diagnosing). "Combined"
is at CANDIDATE level: the input-tag changes are the two separate pinned
TUs above (its admission note §6: "Luamod is untouched by this unit: NO
luamod comparator regen is triggered"), and `diagnostic_event` needs NO new
import (in-link definition in pinned `diagnostic.c` `f5e96c41…`; the TUs
add only `extern "C"` declarations — the identical pattern the admitted
coexist PS5PadSource.cpp already uses; EXPECTED added UND = ∅). Details:
`staging/postpool-combined-luamod/MANIFEST.md`.

Consequence carried honestly: with `c92c92ff…`, the GATE-4
EngineTick-first ladder stays armed for any future walk-permitted world
(first `ExecuteInGameThread` throws "EngineTick callback registration
failed"); the marshal-first re-order rides in facf993a. For THIS boot
class it is moot — GATE-1 refuses before any LuaMod code runs.

### 2.3 What is UNCHANGED from 12fcc41f (full pins in
`manifest/MAX-EVIDENCE-MANIFEST.json`)

LuaMod `c92c92ff…` (comparators `8343578b…`/`c92c92ff…` stay pinned);
capture-first initializer `782dce71…`; SeedValidation.hpp `c2e6e663…`
(three-copy rule: `launch11-combined/units/` + `worker-luamod-rebase/
staging/initializer-l10-r3/` + `launch10-build/final-units/`);
SeedTable.h `59effe6e…`; wpd1 program `75abf2ab…`; walkguard ASMHelper
`694af016…` (fold-0); diagnostic.c `f5e96c41…`; live_module_facts.hpp
`917f13a7…` / geometryfix.cpp `440a5786…`; package_prx.py `e6fbe6c5…`;
pad rows NativePadInput `03c9f6d0…` + PS5PadNative `501d77af…`; settings
ini `b980cea7…` (HookLoadMap=true stays for THIS candidate).

### 2.4 In-session service reconciliation (REQUIRED at composition)

- `pool_probe` CAN be wired now: registration hook
  `insession_cmd2_register_pool_probe_target(SeedTable::kBaseObserved +
  SeedTable::kFNamePoolLabel, (uint32_t)rc_seed::kFNameBlockCapacityBytes)`
  — SeedTable.h verified to carry `kBaseObserved=0x400000`,
  `kFNamePoolLabel=0x9365ec0`; place where SeedTable.h + SeedValidation.hpp
  are visible.
- `io_probe` CANNOT be wired as documented: the service expects per-control
  `int (*)(char *tags_out, unsigned cap)` functions (`bridge_io_control_a..d`);
  the real bridge publishes ONLY the aggregate
  `extern "C" void bridge_io_controls(void)` (verified :1165). Integrate
  EITHER with io slots NULL (`io_probe` replies "controls not wired" — the
  service is complete and linkable per the admission note) OR after a NEW
  controller-owned bridge revision publishes four wrappers. Never
  force-fit the aggregate.
- v2 supersedes worker-insession-diag's event-loop hook — never apply both.

---

## 3. Pin cascade (controller-owned, all-or-nothing; consolidated from
worker-input-tag §6, worker-gameid-unit §7, worker-next-candidate
comparator_cascade_checklist, COMPARATOR-REGEN-PROCEDURE)

1. **S0 preconditions:** launch-1 evidence pulled (pool validated, GATE-1
   stop captured); all staging hashes frozen (this manifest); in-session
   io_probe decision made (NULL slots vs new bridge revision).
2. **S1 bridge_cli:** `EDITIONS.json` `bridge_cli.file` repoint to the
   `41dd0c15…` staging path (stays in the regen `--file` list) +
   `bridge_cli.sha256` → `41dd0c15…`; refresh the free-text provenance note
   (IO-controls + gameid; name the delta and old→new shas).
3. **S2 pad_coexist admissions:** `admissions.pad_coexist.files` — Handler
   `f7539c60…` → `16fcfe49…`, PS5PadSource `24692ec7…` → `d2f14de1…`; add a
   `pad_input_tag` admission record in the same shape (sha fields must
   EQUAL measurement, never prediction). NO header pins move.
4. **S3 insession units:** register the four new units in the composed TU
   set (core.c compiles like diagnostic.c; service.cpp includes NO UE4SS
   header); apply the two wiring diffs at their pinned targets; place the
   pool_probe registration hook. The L11 PAD PLACEHOLDER blocks take the
   new pad unit paths/shas when the controller resolves the pad fold.
5. **S4 SeedValidation/three-copy + stage headers:** UNTOUCHED this round
   (`c2e6e663…` everywhere; runner stage-header pins stay).
6. **S5 undefined-import + audit gates:** EXPECTED added UND = ∅ for BOTH
   the gameid hunks and the input-tag TUs — must be VERIFIED on the actual
   combined objects at compose time; per-build import audit must come out
   at exactly 196 strong; drift is a stop. `.bss` +256 KiB (kGameidChunk)
   inside the bridge budget class.
7. **S6 comparators:** luamod untouched (`8343578b…`/`c92c92ff…` stay); NO
   luamod regen. bridge.o / input-handler.o / native-pad-PS5PadSource.o /
   combined object have no pinned comparators → fresh-measure both passes;
   determinism twin must agree.
8. **S7 replace-receipts + change manifest:** NO manual edits —
   `regen_receipts --full` re-measures; changed_units entries added for
   bridge (gameid), Handler, PS5PadSource, the four insession units;
   finalize_manifest.py re-fills PENDING at pass time; any full-sha
   declaration must EQUAL measurement or the tool refuses.
9. **S8 pre-flight order (procedure §5):** `launch11_one_pass.py verify`
   (HOLD-PASS) → admission_gate preconditions → only then `--full`:
   regen_receipts → preflight_and_verify → pass 1 → post_build gates →
   pass 2 determinism twin == pass 1.
10. **S9 packaging + console:** package + FSELF; console staging per the
    RUNBOOKs (files-first, readback-verify, `.bak` + restore law); settings
    ini stays `b980cea7…` for the candidate boot; the `a4b524a6…` variant
    is a separate optional probe (§5).

Never valid: reusing `8343578b…`/`761b610a…` as any new-source comparator;
pinning a hoped-for object sha; editing the runner to skip a gate; applying
worker-insession-diag's v1 hook alongside v2.

---

## 4. Expected evidence after the pool validates — candidate boot, step by step

All shapes from pinned sources (initializer `782dce71…` line numbers; bridge
`497348b8…`/`41dd0c15…`; ASM `694af016…`; pump-chain CHAIN). Absence of a
line is itself evidence under the honest-no-fabrication law.

**Step 0 — module_start (bridge, always-on):**
- spanpub publication walk (`bridge_kve_*`, frozen-snapshot publish), then
  `bridge_io_controls()` → `io_ctrl_a/b/c/d` families (B canary verified
  ⇒ D copyin of the window-E candidate, 64 bytes; capture slot filled —
  already proven in-game session 1084).
- NEW (gameid): `gameid_eboot_size_low/high`, `gameid_eboot_mtime_low/high`,
  `gameid_stat_fold`, then — per the unit's own honesty design and the
  19-session `/app0` evidence — EXPECTED `gameid_stat_ec=2` +
  `gameid_unavailable=1` + `gameid_ver_ec` and NO sha/version/gamedata row
  (identity PENDING, truthfully; if a mount fix ever makes /app0 real the
  unit starts emitting with zero changes; pinned comparison words for that
  day: decrypted `3b6cfad4 7dd17fea 17d429bc e80ae374`, toplevel
  `afe0a2a5 fc9e1c17 b215bdbb 3de3c80e`).
- NEW (in-session): `insession_cmd2_service_start()` before the wiring;
  dispatch HELD until settle.

**Step 1 — pool validation (the capture-first payoff):**
- `io_ctrl_d` capture from Control D's window-E candidate is consumed via
  `bridge_write_copyin_capture` — the seeds-phase RE-probe is GONE
  (`pool_copyin_probe` re-fire ABSENT at seeds time; the capture-first
  hunk :1063-1070 "Check the capture FIRST").
- `[Seeds] slot 0 g_uobject_array bound at …` (:881)
- `[Seeds] slot 3 gmalloc bound at … -> …` (:932)
- `[Seeds] FName pool validated at … (…; admitted via kernel-write-copyin-read
  (listing-gap owned-read probe; all entry bytes sink-fed); 'None' anchor ok;
  'None' id 0x…, 'ByteProperty' id 0x…)` (:1257) — or the published-span
  variant (:1263); the provenance wording is itself the admission evidence.
- `[Seeds] slot 2 FName ctor bound at … behaviorally verified …` (:1290)
- `[Seeds] slot 4 static_construct_object_internal bound at …` (:1307)
- `[Seeds] Required-slot seed table armed (slots 0/2/3/4); the ps_scan arm
  and both Lua override scans are skipped` (:1380)

**Step 2 — engine init, data-side (READS OK class; igf4-proved):**
required-object wait lines, `Constructed [n / m]: …`, then
`Object CDO` / `Struct CDO` / `Finding 'GameEngine'` / `GameEngine CDO/
Instance` / `GameModeBase`/`GameMode` / `Actor CDO` / `GameViewportClient
CDO/Instance` (:2098-:2157).

**Step 3 — GATE-1 (the expected stop, unchanged):**
```
Finding GameEngine::LoadMap                 (:2161)
[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW
RX verdict (LAUNCH10-MATRIX D2)…            (W1, ASM:234)
state N failed 1 [WalkRxGuard] refused: guard_off_no_rx_verdict (walk text
read not permitted; no read attempted; detail=0)   (W2)
```
then the fail-closed unwind (:DisarmLayoutGate → rethrow → native
shutdown). No event loop, no mods, no pump, no UE4SS pad open.

**Step 4 — what the refusal leaves behind (the NEW evidence this candidate
exists for):**
- in-session service: alive after the refusal (that is its designed case);
  refused-mode triage line via `note_initializer_refused` on the async init
  thread catch; `pool_probe` command re-runs the copyin probe in-session
  (fresh attempt per command; intake `.consumed` + sdiag triad per the
  unit's staging discipline). `io_probe` replies per the §2.4 decision.
- pad input tags (`pad_first_state_read`, `pad_key_edge`): EXPECTED ABSENT
  on this boot class — the UpdateThread that drives the pad never starts
  past GATE-1. Their absence is the correct shape here; the in-session
  refused-mode line is what disambiguates "boot refused" from "input dead".
  The unit's first LIVE evidence window opens on the first boot where
  `setup_unreal()` returns (post M-A).

**Step 5 — the M-E variant boot (optional, zero rebuild, §1):** identical
through Step 2, then NO `Finding GameEngine::LoadMap` line at all, then
`Finding GameEngine::Tick` (:2170) → the SAME W1/W2 pair (the resolve at
:2171 is the next walk). Proves the gate is exactly the flag-gated resolve
family and names the next hop (then InitGameState :2294, ProcessEvent
:2339 — both enabled in `b980cea7…`). Does NOT pass; full law and the
availability-predicate hazard: `settings/SETTINGS-VARIANT.md`.

**Morning sequence (controller/user):**
1. PS5 on → stage `12fcc41f` + clean sink → user launches Townfall once →
   evidence pull (Step 0-3 shapes = pool validated + GATE-1 captured).
2. Compose the §2 candidate (cascade §3) → package → stage → second launch.
3. Optional: M-E ini swap (`a4b524a6…`) → third launch → §4 Step 5 shape →
   restore `b980cea7…` (byte-exact readback).
4. In parallel: worker-r3-import-fix verdict decides the facf993a slot;
   T2 decides the M-A fold flip. Neither blocks this candidate.

---

## 5. Honest bounds

- Proposal-only staging; no console, engine, provider or SDK execution; no
  fresh game/console evidence; no composition or packaging run; no quality
  scores; no AAA/parity/completion claims.
- Every changed unit's evidence is its source worker's HOST-level evidence;
  console sink semantics, real open/read/write under the game sandbox, and
  every boot outcome remain LIVE-only.
- The gameid re-derivation onto `497348b8…` is mechanical and byte-verified
  on the inserted regions; the composition-level gates are still owed on
  `41dd0c15…` (never assume the ∅-UND expectation).
- All log shapes are source-derived PREDICTIONS; GATE-1's exact in-game
  line has never been captured (no boot has reached it yet) — that capture
  is precisely what launches 1-2 are for.
- GATE-1 stays ARMED in this candidate by design; this candidate is the
  maximum-evidence instrumentation step, not a gate-pass step.
