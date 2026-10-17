# Handoff: PS5 Mod Bridge capabilities 1-12 (for ChatGPT / Codex)

Written 2026-10-12 by Claude (terminal). The goal is for you to do the **Mac-side prep** for capabilities 1-12
while Claude finishes the Townfall runtime. The user is not a programmer, so keep your reports short and plain.
Repo: the worktree `~/.codex/worktrees/translation-core/what/`. All paths below are relative to it.

## 1. The project in five lines

- Goal: unmodified PC mods run on a jailbroken PS5 exactly as downloaded.
- Support is per engine (~95% of an engine's games), never per game or per mod. Every platform difference
  lives in a generic runtime or config.
- Test game: Silent Hill Townfall (Unreal Engine 5.6.1, PPSA33286). The Lua mod runtime (a UE4SS port) works
  there: 7 mods run, and a self-test passes 161 of 168.
- Order (user decision): (1) Claude finishes Townfall. That is in progress, **not your job**. (2) Capabilities 1-12,
  one launch each, each proven on Townfall. (3) Stellar Blade (UE4) as a fully blind test, with zero Stellar Blade
  specific code or hand-written values. **No Stellar Blade prep yet.**
- `CLAUDE.md` in the repo root holds the binding rules. Read it first. Its rules apply to you too.

## 2. Rules for you (hard)

1. **Never touch the console.** No FTP (port 2121), no pldmgr (8084), no klog (3232), no payloads, no launches.
   Claude is the only agent on the console. Never write the console's IP address into anything.
2. **Never edit Claude's live files:**
   - `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (the runtime source)
   - `outputs/2026-10-08-l25-prep/harvest/` (gates and build tools)
   - `outputs/2026-10-08-l25-prep/round10-staging/` (in-flight work)
   - `outputs/2026-10-08-l26-frozen/` (shipped builds)
   - `CLAUDE.md`
3. **No builds.** Do not run `harvest/run_labelled_build.sh` or `build_variants.py`. Only one PRX build may run at a
   time, and the Mac has 16 GB and has run out of memory before. Small Mac-side tools and tests are fine. Keep
   heavy jobs to one at a time.
4. **Work only in your own folder:** `outputs/2026-10-12-caps-prep-chatgpt/` (create it), one subfolder per capability.
5. **Runtime rules** (CLAUDE.md section 3):
   - Generic code only.
   - No detours or patching of game code on PS5. Its code pages are execute-only, so hooks are data-only
     (vtable or function-pointer swaps).
   - Every shipped value needs two independent sources. A missing value fails closed: the feature turns off and
     one log line says why.
   - Mod files are never edited.
   - Epic source may be read locally but never copied into outputs or uploaded.
   - Never seek Sony SDK or platform-extension material.
6. **Kernel work (#1, #3 instruction patching) is design-only for you.** Anything on the console needs 3 clean
   passes across restarts. Never touch the quarantined kstuffSharp / InstallerApp module.
7. **Label every claim.** "console-proven" means you cite a real console log line. "Mac-only" means tests or
   reading on the Mac. "design-only" means docs only. Never upgrade a label without a receipt.
8. **Deliverable per capability:** a short `STATUS.md` (what it does, current label, the next concrete step that
   would upgrade it, and what the console launch would need to show), plus any tool and tests. Write any runtime
   code change as a **staged patch script** (exact-once string replacements against the candidate tree, refusing
   on mismatch) for Claude to merge. Never apply it yourself.

## 3. What already exists, per capability

The main status table is `outputs/2026-10-08-l25-prep/CLAUDE-CAPABILITY-STATUS.md`, dated 2026-10-10. Re-verify it
before you rely on it. Everything below is from files in the repo.

| # | Capability | Status now | Where the work is | Next step |
|---|---|---|---|---|
| 1 | Kernel reads | **design-only.** In-process reads work, but that is just the runtime. | `PHASEB-PLAN.md` (read ladder R0-R4), `FZ-PHASEB-KSTUFF-LITE.md`, `kstuff-lite-v1.11-VERIFIED.md`, `capability-prep-20261012/PHASEB-DOC-STATUS.md`; the R0 first-rung spec is in the status table | R0 test plan: read the firmware version with a plain sysctl, expect `0x1360`. A 3-pass plan on paper only. |
| 2 | Scanner (engine/version detection) | **Mac-tested** (18/18 tests). Townfall is correctly detected as UE 5.6, and Astro Bot is correctly refused. | `phasea-tools/`, `PHASEA-PLANS.md` section #2 | Generalize to UE4 using public samples; that matters for the Stellar Blade blind test. Do not touch Stellar Blade files yet. |
| 3 | Code patching | **Split.** Pointer/table hooks (function-pointer swaps) are console-proven and used live. Instruction patching is design-only (Phase B). | status table "#3 hook-kind inventory"; `PHASEB-PLAN.md` | Inventory which PC mod hook kinds the data-only routes cover; design the Phase B data-page patch ladder (design only). |
| 4 | Early loading | **Partly proven.** The runtime is resident during game init, but it is not yet proven against engine milestones (before the first LoadMap). | `PHASEC-EARLY-LOADING-DESIGN.md` section 5 | Define the milestone proof: which log lines would show the runtime is live before the first map load. Claude's current launch adds map-load hook markers you can use. |
| 5 | Debugger | **Mac-tested**, narrowed to crash-log symbolization (18/18, 97.1% of .text mapped). Not a live debugger. | `phasea-tools/symbolize_crash.py`, `PHASEA-PLANS.md` section #5 | Make it a one-command tool over a klog file with a plain report; test on the preserved crash klogs. |
| 6 | Controller | **Done.** Kernel route declined by design. Userspace PS5 pad (PS5Pad.ini key mapping) is console-proven. | status table #6 row, `PHASEB-PLAN.md` section 3.3 | Nothing, unless a mod needs keys beyond the pad mapping. |
| 7 | File overlay (pak / IoStore mods) | **Console-proven for ONE mod (Leon outfit) in ONE game**, but by replacing a game index file. The goal now is **no game file changed**. | `.superpowers/native-observations/leon-index-overlay-trial-2026-10-01/result.json`; `pak-overlay/overlay_builder.py` (32/32 tests); `PAK-RUNTIME-MOUNT-DESIGN.md`; `PAK-PIN-PASS.md` + `pak-pin-pass-pins.json`; `capability-prep-20261012/CAP7-NO-FILE-CHANGE-OVERLAY.md` | **First priority.** Turn the runtime-mount design into a staged, data-only mount plan. Each pin needs two sources. Also list the checklist for "several mods at once", "uninstall proven" and "a UE4 game". |
| 8 | Asset conversion (textures/meshes) | **design / components.** The texture pipeline is **paused by policy**: Leon's PC textures ran unconverted. The active work is a material fallback (point mods at the game's own shaders). | `PHASEA-PLANS.md` section #8, `shader-fallback/`, `agent-GM-shaderfallback.md` | Finish the material-fallback analyzer (derived overlay rows only, originals untouched). Resume textures only if a real mod fails without conversion. |
| 9 | UE4SS C++ mods | **design-only.** Path (a): recompile from published source and link into the runtime. Path (b): a Windows DLL shim, judged hard because of calling conventions. | `CAP9-CPPMODS-DESIGN.md` (566 lines), `agent-HD-cap9.md` | Pick 2-3 real open-source UE4SS C++ mods; check license and source availability; write the recompile plan and the API surface each one needs. |
| 10 | C# mods | **Excluded for now** (user decision; moves to the Unity phase). | `CAP10-CSHARP-DESIGN.md` | Nothing. |
| 11 | UE4SS GUI windows | **design + Mac-tested Lua toolkit.** Widget building from Lua is console-proven in menu sessions (the item spawner's menu is live). The OSD toolkit passes 5/5 host scenarios. | `CAP11-OSD-DESIGN.md`, `osd-toolkit/` (toolkit.lua, host-test.lua, HOST-RUN-LOG.txt), `agent-HL-osdtoolkit.md` | Map what PC UE4SS GUI mods actually call (ImGui-style windows) to the UMG widget shim. Plan the one-launch proof. |
| 12 | Shader translator | **design + first Mac step.** The PS5 global shader cache container decodes and rebuilds **byte-identical** (Mac-only; microcode kept as raw bytes). | `CAP12-SHADER-DESIGN.md`, `cap12-m1a-20261012/REPORT.md`, `capability-prep-20261012/CAP12-M1-FEASIBILITY.md`, `cap12-tools/` | M1b: understand the shader code blobs inside the container. Licensing law: no Sony shader tools, no fabricated shader maps. |

**Suggested order for you:** #7 first, then #8 and #12 (the user wants paks first), then #9, #11, #2 (UE4
generalization), #4 and #5. Design only for #1/#3.

## 4. What Claude's current launch will give you

Claude is finishing one launch that carries: Lua errors unwinding as C++ (B7); map-load, InitGameState, BeginPlay and
EndPlay hooks through data-only routes; 15 engine fields verified from two sources (field gate tightened to zero
unproven reads); and a by-name object lookup. Before that came stutter fixes (object index, time-paced pump work).
Useful for you:

- **#4:** map-load hook markers in the log.
- **#11:** frame-paced callbacks.
- **#9:** a stricter field gate any C++ mod port must also pass: `harvest/field_gate.py`. Read it; don't edit it.

## 5. How to hand work back

Put a `HANDBACK.md` in `outputs/2026-10-12-caps-prep-chatgpt/` with one line per capability:
- label (console-proven / Mac-only / design-only)
- what changed
- files
- staged patches, if any, with their exact target files
- what the console launch must show

Claude merges staged patches and runs all launches. Nothing goes to the console without the user's OK for that step.
