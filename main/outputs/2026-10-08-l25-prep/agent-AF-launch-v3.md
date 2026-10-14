# Agent AF — launch order v3 (B first) documentation update

Date: 2026-10-08. Mac-only, read-only research + documentation edits. No
console contact, no source/engine files touched, no model-facing state
changed. Deliverables: this report, the appended LAUNCH-PLAN.md section,
and usage-doc text edits inside `harvest/prep_modded_launch.py` (strings
only — zero logic change, verified below).

## 1. Files changed

| File | Change |
|---|---|
| `2026-10-08-l25-prep/LAUNCH-PLAN.md` | Appended section `## Launch order v3 — B first (Claude review 2)` (subsections v3.0–v3.5). Appended at the end of the file, after the Interpretation-matrix-v2 listener placeholder; nothing above it was edited. |
| `2026-10-08-l25-prep/harvest/prep_modded_launch.py` | Usage-doc text only: module docstring (USAGE block + new LAUNCH ORDER v3 paragraph), `VARIANT_DESC` A/B order wording, `variant_expectations_md("B")` (v3 order + in-session spawn-test branch), `user_sequence_md("B")` (new step 5b: harvest → D-pad + one spawn → harvest again), argparse `--variant` help. No gate, check, command, constant-behavior, or exit-code change. `python3 -m py_compile` PASS; `--help` renders; both text functions re-rendered and eyeballed. |

## 2. Claude's revised directive — transcribed faithfully (LAUNCH-PLAN v3.0)

- BEFORE any launch: Mac-only items 1-4 must land (offset verification,
  GMalloc audit, container audit, extended A logging) — reference the agent
  reports as they exist.
- LAUNCH ORDER: Variant B FIRST (it's the path to the first real look at
  mod start, the pump, the pad, and the menu). Then variant A second, only
  to confirm the mechanism (with item 4's logging).
- B test procedure: console restart first; restore ORIGINAL
  UE4SS-settings.ini byte-exact 1947 B; main menu 90 s; then load the save,
  wait 30 s, L3+R3 once. IF THE MENU OPENS: harvest, then D-pad + one spawn
  IN THE SAME SESSION, then harvest again.
- Variant C (proper fix) designed after the B result; packet + Claude
  review + user OK as always.

## 3. B-first rationale (documented in the plan)

Z11's solved chain (`agent-Z11-stack-symbolize.md` §3/§6): the CE-108255-1
crash is a deterministic init-completion crash at
`UnrealInitializer.cpp:528` — `AddUObjectDeleteListener` →
`TArray<FUObjectDeleteListener*>::Append → ResizeTo → ResizeAllocation →
FMemory::Realloc → RC::Unreal::FMalloc::Realloc` whose `call rax` is the
FIRST live call into the game's (repointed) GMalloc; inside the game
allocator a virtual dispatch at vtable slot +0x10 landed on the game's
fatal-error formatter → abort. Variant B (`MODBRIDGE_L25_SKIP_DELETE_LISTENER`,
wraps BOTH registrations per agent R §2) performs no Append → no
FMemory::Realloc → no allocator call from that path at all → predicted to
survive init and potentially reach mod start. Hence B is the path to the
first real look at mod start, the pump, the pad, and the menu; A second,
only to confirm the mechanism.

## 4. Mac-only items 1-4 grounding (as the reports exist)

1. Offset verification → `agent-Z11-stack-symbolize.md` (+0x10 dispatch
   site; sharpened suspect), `agent-Z8-listener-layout.md` (layout-proof
   method to replicate for the allocator vtable).
2. GMalloc audit → `agent-Z11-stack-symbolize.md` (repoint
   0x970add8→0x97089b8 + FMalloc vtable suspect), `agent-V-listener-abi.md`
   (slot-mismatch precedent), l21 "Virtual FMalloc::QuantizeSize
   unavailable" precedent (agent F/G reports).
3. Container audit → Claude review 2 cites an **agent-AC audit**;
   **no agent-AC report exists in `2026-10-08-l25-prep/` as of this
   writing** — documented as an open deliverable with existing inputs
   `agent-Z11-stack-symbolize.md` (generic Append→ResizeTo→FMemory→FMalloc
   chain), `agent-Z6-getallocatedsize-reachability.md` (mechanism B),
   `agent-V-listener-abi.md` §4.
4. Extended A logging → `agent-O-listener-instrumentation.md`,
   `agent-P-phase-tags.md`, `agent-Q-copyin-probe.md` (whatever review 2
   adds on top must be verified landed before STEP 0.2 hashes the twins).

## 5. Spawn test — read from the mod's ACTUAL code

Mod: TownfallItemSpawner `scripts/main.lua`, exactly **21415 B**. Provenance
copies of the console-resident file (byte-identical set):
`outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/main.lua`,
plus the l22-package-r2 before/after/harvest triple and
`2026-10-08-menu-path-audit/source-cache/main.lua`. Read in full.

- 6 `RegisterKeyBind` calls (main.lua:694-729), all wrapped in
  `ExecuteInGameThread`: F7→`toggleMenu` (:694), UP_ARROW→selection-1
  (:698), DOWN_ARROW→selection+1 (:703), RETURN→`activateSelection` (:708),
  LEFT_ARROW/RIGHT_ARROW→page turn (:713/:722). Plus a `pcall`-wrapped
  `RegisterLoadMapPreHook` state reset (:731-744).
- Pad layer: `ue4ss/PS5Pad.ini` (staged with the mod; verified in the
  l22-package-r2 console-test harvest copy) maps **L3+R3→F7, UP→UP_ARROW,
  DOWN→DOWN_ARROW, CROSS→RETURN, LEFT→LEFT_ARROW, RIGHT→RIGHT_ARROW**;
  OPTIONS deliberately unbound (game pause). Chord = conjunction match,
  one F7 per chord completion, no frame-perfect simultaneity
  (`PS5PadSource.cpp:154-171`).
- Menu rows (`buildRows` :375-422), selection starts at row 1: 1 Quantity
  (default x1 — `quantities={1,2,5,10,20}`), 2 Page, 3 Refresh pickup list,
  4-11 the eight PICKUPS slots, last row STATUS. **Operationally "D-pad +
  one spawn" = DOWN×3 → first PICKUPS row (RIGHT once if it shows `---`)
  → CROSS once.** One CROSS = one spawn at quantity x1.
- UE4SS key codes for diag `pad_key_edge` values (bounded 60/boot): F7=118,
  RETURN=13, LEFT/UP/RIGHT/DOWN_ARROW=37/38/39/40.

### Spawn-test marker list (exact strings from the mod's code)

| Marker (UE4SS.log) | Meaning | main.lua |
|---|---|---|
| `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` | mod main script ran | :746 |
| `[TownfallItemSpawner] <N> spawnable pickups found. Pages=<P>` | first-open item scan OK | :189 |
| `[TownfallItemSpawner] Scan failed: <err>` | scan failed (e.g. save not loaded) | :194 |
| `[TownfallItemSpawner] Menu created.` | first-open UI build OK | :643 |
| `[TownfallItemSpawner] UI creation failed: <err>` | UI build failed | :636 |
| `[TownfallItemSpawner] WidgetBlueprintLibrary not found.` | UMG lib missing | :502 |
| `[TownfallItemSpawner] Spawned <name> x<qty> ID=<id>` | **SPAWN PROOF** | :335 |
| `[TownfallItemSpawner] Failed at <stage>. See UE4SS.log. <err>` | spawn failed; stage ∈ {player, ground trace, load pickup class, spawn actor, configure pickup} | :342 |

Caveat documented in the plan: the F7 toggle itself and selection moves log
NOTHING — the only in-log menu-open evidence is the FIRST-open pair (scan +
`Menu created.`), so the first harvest pull must happen in the same session.
Runtime-side corroboration: `[l22] keybind callback entry key=118` then
`key=13` (LuaMod.cpp:2040), each with
`[l22] ExecuteInGameThread queued`/`drained … (callback entry)` pairs
(:5650/:4367); diag `pad_l3r3_f7_dispatched`, `pad_key_edge` value 13.
`harvest/markers.json` currently has NO `[TownfallItemSpawner]` entries —
the plan documents exactly what to add/grep (listed in LAUNCH-PLAN v3.3).

## 6. B harvest expectations + failure modes (LAUNCH-PLAN v3.4)

Expected sequence if all works: `Adding shutdown listener` (:527) → NO
`[L25][DeleteListener]` registration lines, ONLY `delete-listener
registration SKIPPED (variant B; unsafe long-term: stale object pointers
not cleared)` (both skip sites, UnrealInitializer.cpp:557/:578) →
`Starting callback garbage collector!` (:542) → `[l25] event loop started`
→ `[l25] on_program_start entered` → `[l22] Lua mod started:
TownfallItemSpawner (main script returned)` + `[TownfallItemSpawner] Loaded
v1.0. …` → `[R3] FuncPtr-marshal armed on carrier` → `[l25] marshal hook
first invocation (carrier fired)` + `r3_marshal_pumped` → pad chain
(`pad_available` → `pad_first_state_read` → `pad_key_edge`=118 →
`pad_l3r3_edge` → `pad_l3r3_f7_dispatched`) → `[l22] keybind callback entry
key=118` + queued/drained pairs → menu markers → `Spawned … ID=` → klog
zero copyin/nonsleeping/fatal hits.

B-specific failure modes documented: (1) another container growth crashing
one line later — if the true fault is the GMalloc
dispatch/repoint/vtable, the next init-time engine-container growth through
FMemory/GMalloc reproduces the identical chain; per the agent-AC audit
cited by review 2 (not yet on disk; grounded in Z11 §6 + Z6 mechanism B);
the [l25] phase ladder localizes the last-reached tag; (2) mod start
blocked elsewhere — the variant-independent A-STALL rung ladder applies
unchanged past :528; (3) menu opens but spawn fails — still a valid B
result; the `Failed at <stage>` line names the stage; no retry-spam.

## 7. Prep script note

`--variant B --prx-path <B.prx>` mechanically unchanged (LAUNCH-PLAN
v3.5): the order change is documentation only; the twin-build machinery
(agent R) is order-agnostic. Verified: `py_compile` OK, `--help` shows the
v3 order, B receipt renders the new step 5b and SKIPPED + spawn-test
expectations, A receipt shape unchanged.

## 8. Honest gaps

- No agent-AC report exists on disk; cited as an open Mac-only deliverable
  (item 3), with existing Z11/Z6/V results as inputs.
- Whether review 2's items 1-4 have ADDITIONAL specifications beyond the
  grounding above is not verifiable from disk; the plan states they must
  land as reports before the launch gate closes.
- The mod's menu/spawn markers are not yet in `harvest/markers.json`; the
  plan documents the exact add list. Adding them is a markers.json edit,
  deliberately NOT done here (out of task scope: edit only LAUNCH-PLAN.md
  and the prep script's usage docs).
