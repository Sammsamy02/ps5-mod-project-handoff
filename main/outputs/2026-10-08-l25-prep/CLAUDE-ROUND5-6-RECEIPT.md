# CLAUDE ROUND 5+6 RECEIPT — one receipt covering both review rounds

Agent GR (ZCode), 2026-10-10. DOCS ONLY: this receipt, the WAKE-RUNBOOK
§12 append, and the GR report. Read-only everywhere else. Every status below
is **landed Mac-only / in flight / queued-at-AC-power / queued-console** per
the safeguard law: Mac-side artifacts are not console proof; each row cites
its artifact; nothing is overclaimed. The console has had ZERO contact this
round (rest mode); no builds were run (battery law); no PRX exists yet for
round-4/round-5 — every build is queued at AC power (§12 staging order).

Position recap: frozen known-good = PRX `efbfc656` (menu-up build,
FROZEN-KNOWN-GOOD-20261010.md); resident round-3 = `56a9b1a0`; round-4
contents LOCKED = round-3 tree + FR parity revert + FW routed sites/literal
markers, FX patches OUT, shim OUT (WAKE-RUNBOOK §10.3).

---

## 1. ROUND-5 directive — what was ordered, who delivered, status, console proof

Numbering note (honest): the verbatim numbered round-5 directive is not
preserved on disk as a standalone document. Item numbers below are the ones
landed reports themselves quote (FR "PRIORITY-1"; GA "round-5 item 4"; GB
"6a"; GC "6b"; FW/FL/FM "Claude item 9"); the remaining workstreams are
mapped by their dispatch names from CLAUDE-ROUND5-ADDENDA.md's work mapping
and WAKE-RUNBOOK §10. Every row cites its artifact either way.

| # | Ordered | Delivered by / artifact | Status | Console test that will prove it |
|---|---|---|---|---|
| 1 | **PRIORITY-1 parity revert**: FName/FText/FString property reads return UPSTREAM userdata (FM plain-string pushers reverted); bridged `:ToString`/`__tostring` kept | FR — `agent-FR-parity-revert.md`; edits at `typescan-r4/candidate/src/LuaUObject.cpp:1910/:1969/:2027` (post-edit sha `aa10e4ea…`, 2794 lines); host test `host-tests/prop-parity-test.lua` (8/8 PASS incl. the "plain-string shape FAILS" regression guard); one `-fsyntax-only` rc=0 | **landed Mac-only** (source + host proof; no build) | Round-4 launch: `type(pickup.PickupInventoryItem.ItemId)` prints `userdata`; `:ToString()` and `tostring()` print the same id; `[LFB-M] fname-rev … ok=1` fires on both (FR §5 grep list) |
| 2 | **API parity table** (port vs upstream registration surface — data of record for the ladder) | FS — `API-PARITY-TABLE.md` (header carries the FS identity + scope; global/type member-name sets proven IDENTICAL port vs upstream) | **landed Mac-only** (analysis; no separate agent-FS report file exists — see gaps §4.1) | No test of its own; its claims get exercised row-by-row by GB's self-test on console (row 6a) |
| 3 | **Per-mod static scan** of the ladder pool: inventories, predicted first failures, aggregate fix queue | FT — `agent-FT-static-scan.md` + `mod-ladder/STATIC-SCAN.md` (9 corrections to FO's manifest; 24-item gap queue; crash-law sweep clean; 13th real mod found: stamina) | **landed Mac-only** | Predictions confirmed/refuted by the ladder-v2 rung markers on console (row 6b); each "predicted first failure" names the probe that settles it |
| 4 | **io/path shim** (round-5 item 4): logical->physical path resolver + io/os write confinement for PC-shaped mod paths | GA — `agent-GA-path-shim.md`; NEW `typescan-r4/candidate/src/LuaPathShim.cpp` (851 ln) + `candidate/include/LuaPathShim.hpp` (91 ln); ONE LuaMod.cpp insertion (now :7842-7862 after root's Rule-10 fix, §3.1); host tests `host-tests/lua-path-shim-test.lua` 38/38 PASS; 13-subject matrix verdicts identical to FU baseline | **landed Mac-only; EXCLUDED from round-4 by the build-scope lock** (§12 step 0); queued for the round-5 build | Round-5 build launch: `[SHIM] path-resolver installed mod=… root=…` in UE4SS.log; MiniMap settings io read/write through the resolver; `[SHIM] write-rejected` on out-of-mod writes |
| 5 | **Host Lua harness** for the 12-mod pool (calibrated on the console-known-good mod) | FU — `agent-FU-host-harness.md` + `host-lua-harness/` (stub, runner, run-all, 26 logs, RESULTS.md); calibration law upheld (console-copy spawner LOADS-CLEAN) | **landed Mac-only** | Same as row 3 — Lua-level greens are explicitly NOT console proof (FU's own caveat); native marshal questions stay with the P-probes/ladder |
| 6a | **UE4SS self-test** (round-5 item 6a): game-agnostic 167-call parity mod replacing the ladder's first phase | GB — `agent-GB-selftest.md` + `ueselftest/` (generator, 12-file mod, EXPECTED-MARKERS, CALL-SHEET, CONSOLE-TEST-PLAN, validate.lua; host 167/167; crash-resume drill proven; stub extension neutral 27/27) | **landed Mac-only** (Lua-only deploy, no PRX change; fits the round-4/5 launch window) | Ladder-window launch: press **L1+R1** (F12) after the spawn test -> `[ST2] SUMMARY <N> of 167 PASS`; PASS bar = ≥95% AND 0 FAIL in the four critical families; 3 passes across restarts; opportunistic crash-resume drill. Needs the 1736 B `PS5Pad-round4-proposed.ini` + loadlist row re-pin (§4.7) |
| 6b | **Ladder v2** (round-5 item 6b): one-launch-per-TYPE check; standard check = profile -> self-test -> ~3 mods + 3-passes-across-restarts | GC — `agent-GC-ladder-v2.md` + `mod-ladder/LADDER-V2.md` (supersedes MANIFEST/VERDICT-SCHEMA first lines; picks: CheatManagerEnabler, ConsoleEnabler, MiniMap, Stamina + resident spawner; interference verdict GO; zero new pad rows) | **landed Mac-only** | One launch + up to 2 restarts, byte-identical staging, per-type exact marker strings (LADDER-V2 §2), `harvest.py --label ladder2-combo-r1..r3`; watch-item: double-ClientRestart hook unhooking detected by the 3-restart law |
| 7 | **Cleanup round** (directives a+b+c): route remaining conversion sites; whole-literal diagnostic markers; release diag define | FW — `agent-FW-cleanup.md`; edits in `typescan-r4` LuaFNameBridge.hpp/.cpp, LuaUnrealString.hpp (FString `__tostring` closes FR's gap), LuaUObject.hpp ProcessConsoleExec route; `MODBRIDGE_RELEASE_DIAG_OFF` (per-call markers compiled out, refusals kept); `harvest/markers-declared.json` now 15 literal-strict entries (18 with shim, §3.1); 6× `-fsyntax-only` rc=0 | **landed Mac-only** | Round-4 build gate: `verify_build.sh --manifest markers-declared.json` PASS against the literal-strict markers; on console `fstring-fwd` fires on ProcessConsoleExec and `tostring(fstring_prop)` returns the string |
| 8 | **Parked P1/P2 patch sets** (fault-ledger desaturation; variant-B dead-by-default + dead-R3 removal) | FX — `agent-FX-parked.md` + `parked/P1-fault-ledger/` + `parked/P2-variant-b-removal/` (proposal patches, /tmp apply-clean 0 rejects, live tree verified untouched; NEVER-APPLY-WITHOUT flags in NOTES) | **landed Mac-only as PROPOSALS; builds queued-at-AC-power** (§12 steps 4-5) | P1 labelled build: one-shot `[ST] copyin_fault_ledger_st=15`/`_st_pass=1`; P2: default runs write no B pair-receipts; final gated build console-tested inside the ladder-v2 launch (no separate launches; bisect via intermediate gated builds) |
| 9 | **Build speed**: concurrent twin builds + LuaMod split plan | FY — `agent-FY-build-speed.md`; `build_variants.py` concurrent twins (4 passes, `L26_BUILD_JOBS`, deterministic gate merge; plan-mode byte-identical; unit tests PASS); `FY-LUAMOD-SPLIT-PLAN.md` (plan only) | **landed Mac-only; execute unproven** — serial-equivalence proof queued | §12 step 2: during the console launch window, build the same snapshot with the pre-FY serial driver (`/tmp/build_variants.py.pre-FY.bak`); PRX shas must MATCH, else stop using concurrent builds until explained (§10.4 law) |
| 10 | **Next-track research**: kstuff-lite interface doc + second-game profile dry-run | FZ — `agent-FZ-next-tracks.md`, `FZ-PHASEB-KSTUFF-LITE.md`, `FZ-GAME2-PROFILE-DRYRUN.md`, `FZ-GAME2/` (Astro Bot REFUSED 0/244 — machinery validated on a never-profiled binary) | **landed Mac-only** (research) | No console test this round; Phase B read rungs R0-R4 are the eventual console tests behind Claude review; the open blocker (lite source) is now closed by root action §3.2 + GP |
| — | **Item 9 policy** (Helpers codec retained for non-Lua consumers) | Honored by FL/FM/FW in their edits (class-(c) sites keep Helpers; cited in FW §2, FL inventory, FM report) | **landed Mac-only** (policy compliance, verified in the round's edits) | n/a (policy; visible as retained `#else` host branches post round-4 build) |

### Round-5 ADDENDA work (CLAUDE-ROUND5-ADDENDA.md A1-A5 mapping)

| Addenda item | Delivered by / artifact | Status | Console test that will prove it |
|---|---|---|---|
| A1/A5 first-launch automation + 95% coverage architecture (+ Rule-10 release audit) | GI — `agent-GI-firstlaunch.md`, `FIRST-LAUNCH-AUTOMATION.md`, `phasea-tools/release_audit.py` + run1/run2 receipts, `tables/ue5.6-…layout-table.example.json` | **landed Mac-only** (design + audit; audit exit 2 with blind-test game CLEAN — see §3.1/§4.6) | Dogfood law: generator passes TOWNFALL on console with the pinned profile removed BEFORE any second game; then the Blade first-launch flow is the proof |
| A3a Stellar Blade prep (per-version tables UE4-5.6 + generator, blind-test law) | GJ — `agent-GJ-blade-prep.md` + `blade-prep/` (per-version-table.json — 1 measured 5.6 row + 6 seed bands; run-blade-profile.py: Townfall ACCEPTED byte-exact vs SeedTable receipts, Astro REFUSED; BLIND-TEST-KIT, TEST-PLAN, PREREQ-CHECKLIST) | **landed Mac-only; dump queued-console** (round-6 item 2, below) | After the user's-own-copy decrypt-off dump: `run-blade-profile.py` ACCEPTS from the table alone; blind-test 5-mod kit under the 3-pass law; failure => engine/version-level fix only |
| A3b Capabilities B->C->A staged Mac-only | GD — `agent-GD-phasebc.md`, `PHASEB-PLAN.md` (kernel bridge plan; 6 readiness gaps; R0-R4 read ladder; L4 prohibited), `PHASEC-EARLY-LOADING-DESIGN.md` (#4 SATISFIED-ALREADY, receipted from the frozen session timeline); GF — `agent-GF-phasea.md`, `PHASEA-PLANS.md` + `phasea-tools/` (18/18; #2 ready-to-build, #5/#7 needs-X, #8 texture ready / mesh blocked) | **landed Mac-only** | PhaseC: 3-pass observe-only receipts (module_start_entered t_ms vs eboot monitor bound, existing instrumentation, no new build); PhaseB rungs + PhaseA #2/#7 console tests all behind Claude review + user OK |
| A5 mod manager replaces manual installs | GG — `agent-GG-modmanager.md` + `mod-manager/` (modmanager.py 1343 ln; 49/49 offline; --apply refuses while console is down by design) | **landed Mac-only** | DESIGN.md §7 3-pass console acceptance: preflight -> upload -> readback-cmp receipts + rollback drill, console on |
| GH other-engine research (priority context only) | GH — `agent-GH-engines.md` + `OTHER-ENGINES-RESEARCH.md` (5 frameworks, MUST-needs per capability, ranking: REFramework first) | **landed Mac-only** (research; execution follows A3 (a)/(b) first) | None this round; first REFramework step is Mac-local (RE eboot dry-run), later read-only self-test PRX behind review |

## 2. ROUND-6 directive (WAKE-RUNBOOK §11) — status

| Item | Ordered | Delivered by / artifact | Status | Console test that will prove it |
|---|---|---|---|---|
| 1 | **FX build order**: P1 then P2 as separate fully-gated labelled builds on AC power; console-test the FINAL build inside the ladder-v2 launch; bisect via intermediates | FX (row 8 above) + §12 staging order (this receipt's companion append) | **patches landed Mac-only; builds queued-at-AC-power** (§12 steps 4-5) | Ladder-v2 launch on the final gated build; regression => bisect across round-4 / equivalence / P1 / P2 / round-5 archived builds |
| 2 | **Stellar Blade scheduling**: dump at the FIRST console session after the Townfall launch; user's own copy; decrypted copy stays on the Mac, never uploaded; then GJ dry run + container survey; fixes engine/version-level only | GJ prep (landed) + runbook §11 law | **prep landed Mac-only; dump queued-console** | Post-dump Mac dry run: `run-blade-profile.py` verdict ACCEPTED (or seed-refusal if ≠5.6 — by design, triggers the table-upgrade campaign) |
| 3 | **Pak track (capabilities #7/#8) Mac-only while power is out**: (a) generalized overlay builder, (b) no-game-file-change runtime mount design, (c) Leon glasses cause + generic PC-shader fallback, (d) LogicMods/BPModLoader through the harness + pak-side plan | (a) GK — `pak-overlay/overlay_builder.py` in progress, **report absent: IN FLIGHT**; (b) GL — `agent-GL-runtimemount.md` + `PAK-RUNTIME-MOUNT-DESIGN.md` **landed Mac-only**; (c) GM — `shader-fallback/` partial artifacts, **report absent: IN FLIGHT**; (d) GN — `agent-GN-logicmods.md` + `logicmods/` **landed Mac-only** (Stage 0 done: loader runs to terminal lines in harness, 27/27 calibration byte-identical; two REAL port gaps found: Content-dir derivation + absolute-path dofile) | GL/GN landed; GK/GM in flight (§4.5) | GL: Phase 0 observation launch (one reviewed launch, read-only: module_start timing vs Paks enumeration + which seam opens pakchunk0) then rung markers (MountedPaks entry + engine FileExists); GN: Stage 2 behind Claude review; GK/GM console proof TBD in their reports |

## 3. ROOT ACTIONS this session — on-disk verification (performed by GR, 2026-10-10)

### 3.1 Rule-10 fix in the path shim — VERIFIED

GI's release audit found `include/LuaPathShim.hpp:80` hardcoding
`kDefaultModBridgeRoot="/data/modbridge/roots/PPSA33286"`. Root removed the
title-id default. GR verified on disk:

- `typescan-r4/candidate/include/LuaPathShim.hpp`: `grep PPSA33286` → **no
  matches**. Header :77-79 now documents: "NO title-id default exists
  (Rule 10 / games, release-audit finding): the installer derives the root
  at runtime from the program's mods directory; tests override via
  `set_modbridge_root_override`."
- `typescan-r4/candidate/src/LuaMod.cpp:7842-7862`: the single [SHIM]
  insertion now derives the root at runtime —
  `std::filesystem::weakly_canonical(m_program.get_mods_directory())`,
  strips a trailing `Mods` then `ue4ss`, and passes the derived `<root>` to
  the 3-arg `LuaPathShim_install(lua, mod_name, modbridge_root)`. File is
  9,530 lines post-fix (9,509 pre-GA).
- `typescan-r4/candidate/src/LuaPathShim.cpp` `install()` (~:726-734):
  resolution order = test override > passed-in root; **if the root resolves
  empty the shim disables itself loudly** —
  `[SHIM] path-resolver disabled reason=no-root (no title-id default exists; derive from the mods directory)`
  — and leaves stock io/os untouched. Confirmed by reading the code.
- Residual (honest, known): comment-only title-id mentions remain
  (`LuaPathShim.cpp:89`, plus LuaMod.cpp:8566 / hpp:9 /
  UnrealInitializer.cpp:1961 per GI's audit) — scrub-at-release items, no
  compiled-in default.

Status: **landed Mac-only**. Console proof: the round-5 build's
`[SHIM] path-resolver installed … root=<derived>` marker (root comes from the
runtime mods directory, not a constant), plus a re-run of
`release_audit.py` runtime scope showing the behavioral violation gone
(comments still flagged until scrubbed).

### 3.2 kstuff-lite PINNED — VERIFIED

`outputs/kstuff-lite-pinned/` holds:

| file | sha256 (GR-computed) | match |
|---|---|---|
| `kstuff-lite-v1.11.elf` (1,737,080 B) | `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` | == the console pldmgr catalog checksum (`2026-10-04-zcode-team/worker-boot-protocol/probe/list_payloads.json` → `/meta/kstuff-lite_v1.11.elf/checksum`, URL = itsPLK mirror release) == the upstream mirror sha recorded in `FZ-PHASEB-KSTUFF-LITE.md` §upstream. **The resident payload IS the official release.** |
| `kstuff-lite-v1.11-source.zip` (5,922,550 B) | `b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6` | the v1.11 source zip (GP verifies the source claims — **in flight**, no report yet) |

This closes FZ's open blocker (lite source not on the Mac) at the pinning
level; Phase B's §2 byte-identical-rebuild-or-bust gate still applies before
any client code.

### 3.3 Round-4 build-scope law + AC-power staging order — documented (this receipt + runbook §12)

Claude locked **round-4 = FR + FW only**. GA's shim is round-5 content and
must be EXCLUDED from the round-4 build, then restored. The full staging
procedure (set-aside → round-4 → serial-equivalence → restore → P1 → P2 →
round-5) is now §12 of WAKE-RUNBOOK-20261010.md with the exact set-aside /
restore file list (verified against the live tree by GR). Root owns
execution; status: **documentation landed; execution queued-at-AC-power**.

### 3.4 GJ provenance finding — VERIFIED

`blade-prep/per-version-table.json` line 4:
`"generated_by": "agent-GJ 2026-10-08 (Claude round-5 addendum A3a: per-version layout tables UE4-5.6 + generator, Stellar Blade blind-test prep)"`
— the blind-test game name IS in the provenance string (GI finding
confirmed on disk). Future work item: the cell packer must whitelist
provenance fields so no game name flows into a shipped cell. No console
test (release-scope item).

## 4. HONEST GAPS

1. **FS**: no `agent-FS-*.md` report file exists; the deliverable
   `API-PARITY-TABLE.md` (header self-identifies as Agent FS, scope + method
   stated) is the data of record. FT ran its own registration census because
   the table was absent at FT's polls (it landed 12:13, FT's report is
   12:15 — ordering, not loss).
2. **Everything is Mac-only.** No builds, no console contact, no PRX for
   round-4/5 yet. FR/FW/GA are verified by single-file `-fsyntax-only` + host
   Lua tests only. Console proof begins at the §12 AC-power build + ladder
   window.
3. **Round-5 verbatim numbering**: the numbered 1-9 directive text is not on
   disk; §1's numbers are the ones quoted by landed reports, the rest are
   mapped by dispatch name (stated in §1's note).
4. **FX P1/P2 are unapplied proposals**; root must apply → `--plan` →
   `--execute` with full gates. FY's concurrent-twins execute path is
   unproven on a real build (plan-mode proven only) — hence the §12 step-2
   serial-equivalence proof.
5. **In flight at receipt time**: GK (overlay builder — `pak-overlay/overlay_builder.py`
   being written, no report), GM (Leon glasses/shader fallback — partial
   artifacts in `shader-fallback/`, no report), GP (kstuff-lite source
   verification — no report). GN and GL landed during this session. Their
   statuses must be re-read from their reports before the next review.
6. **GI's audit receipts predate the Rule-10 fix**: run1/run2 show exit 2 /
   10 violations including the now-fixed behavioral one; the audit has not
   been re-run post-fix (4 comment hits + 5 profile-table units remain in
   the runtime-scope count by design until scrub/architecture).
7. **ueselftest pad row**: deploying GB's combo needs the 1736 B
   `PS5Pad-round4-proposed.ini` + MFA loadlist row re-pin — an explicit,
   reviewed deviation from "live 3718 B ini unchanged" (ladder-v2's four
   picks themselves need zero new rows; only the self-test combo does).
8. **CHANGES.json carries the shim**: `round5_row_sync` block +
   `LuaPathShim.hpp` entries in added_files/added_header_hashes. The §12
   set-aside must reconcile these (root-owned; listed in §12) or the G2
   candidate-hash gates will fail the round-4 build.
9. **Lua-version honesty** (GN): harness runs Lua 5.5.1, the vendored
   runtime is 5.4.7 — checked equivalent for the touched semantics, but it
   is a standing caveat on all host-harness greens.
10. Gameplay/mod functionality on console remains at the frozen menu-up +
    24-pickups + DRY 6/6 state; the ItemId spawn hop is fixed on paper (FR)
    but unproven until the round-4 launch.

## 5. QUESTIONS FOR CLAUDE (only real, already-on-record decision points)

1. **P2 removal boundary** (FX §P2, flagged in the report): the
   Blueprint-override event-carrier family + `funcptr_marshal_hook` +
   `r3_marshal_lazy_arm` are console-dead today (GATE-1) but designated the
   FALLBACK pump by the Claude-approved EU FIX4 comment. Removing them is a
   resilience-policy decision — keep as fallback (current P2 does) or
   authorize the one-hunk follow-up removal?
2. **Delegate `FunctionName` pass-back semantics** (FR §2 note): FM's
   plain-string delegate `FunctionName` table values were KEPT per dispatch;
   upstream l22 built FName userdata there. If strict PRIORITY-1 pass-back
   semantics are wanted for `prop.FunctionName` too, it is a separate
   one-line-per-site root-assigned change. Which shape should ship?

— agent GR (ZCode), 2026-10-10. No console contact, no builds, no tree edits
(this receipt + the runbook §12 append + the GR report are the only writes).
