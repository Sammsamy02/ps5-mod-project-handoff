# CHATGPT HANDOFF — capabilities #1, #3, #13 (2026-10-13)

Written by ZCode (root session) for a fresh ChatGPT worker. Self-contained:
everything needed is in this file or at the paths it cites. The user's scope
clarification of 2026-10-13 (quoted in §1) is the LATEST word and supersedes
earlier wording anywhere else.

Your work area: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/`
(create `cg-*` subdirectories for your outputs; NEVER write into existing
agents' directories — especially `cap13/e2e/`, which has an agent KF still
running in it). Absolute prefixes used below:

- `WT`  = `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013`
- `R5`  = `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep`
- `WHAT` = `/WORKSPACE/.codex/worktrees/translation-core/what`

## 0. Hard rules (binding — read first)

1. **PS5 UNAVAILABLE** until the user explicitly says otherwise. All work is
   Mac-only. No console contact of any kind (not even reads), no build for
   console deployment, no launch planning beyond test cards.
2. Console actions (when the PS5 returns) need the user's explicit OK for
   THAT action. The 3-restart law applies to kernel-work claims. Phase B
   (new kernel-resident code) is gated on the Townfall Item Spawner spawning
   first, per-step user OK.
3. **Never modify kstuff, never load new kernel code** without Phase B
   approval. Quarantined forever: kstuffSharp/InstallerApp kernel module.
4. **Mod files are never edited.** Original DLL files are preserved
   byte-exact. Ordinary in-memory relocation and import binding are allowed;
   no per-mod rewrites, no hand-entered per-game loader addresses.
5. **Generic runtime only**: no mod- or game-specific names, offsets, or
   special cases anywhere in runtime code. Engine differences live in
   adapters; game-specific values come from the existing profile generator.
6. **Honest labeling**: every claim is "console-proven" (citing a specific
   session log) or "Mac-only". Never overstate. Missing evidence = unrated.
7. Never replay a failed attempt unchanged; new revisions get distinct IDs.
8. No Sony SDK material; Epic source stays local (never committed/uploaded).
9. Do not duplicate web fetches; share sources. Do not touch: the integrator's
   candidate tree (`WHAT/outputs/2026-10-08-l26-runtime/typescan-r4/`), frozen
   builds, mod files, or `cap13/e2e/` (KF is live there).
10. Full context of the standing rules: `WHAT/CLAUDE.md` (worktree law) and
    `/WORKSPACE/Projects/ai-game-studio/AGENTS.md` (console + PS5 rules).
    Where two rules differ, the stricter applies.

## 1. Scope (user, 2026-10-13 — LATEST, supersedes earlier wording)

**Goal**: unchanged DLL gameplay mods that hook or patch the PS5 version of
their intended game. **Automatic loading alone is insufficient: the mod's
intended behavior must work.**

Excluded: PC graphics injectors, online mods, mods for other game versions.
Sound support deferred. Shared core reusable across engines; engine
interfaces via adapters; game-specific values via the existing profile
generator.

Responsibilities:
- **#1**: establish the documented memory-read capability and its evidence.
- **#3**: establish executable-memory patching, hooks, and complete
  restoration.
- **#13**: integrate DLL loading, framework/ABI compatibility, and automatic
  target resolution so existing DLL mods can exercise those capabilities.

**Automatic retargeting — three DISTINCT steps** (do not conflate):
1. Resolving a named target.
2. Finding the corresponding function across PC/PS5 builds.
3. Making the mod's call, hook, or replacement instructions compatible with
   the PS5 target.
Resolving an address alone does NOT establish step 3. Verify names and
patterns; never assume they carry across builds.

**#3/#1 relationship**: do NOT assume every #3 operation requires a new #1
kernel modification. Establish what the EXISTING permissions route supports
(task C3 below).

**Reporting format**: report loading, target resolution, and working patch
behavior SEPARATELY. Phase B prerequisite for console patching is preserved.

## 2. Current state (labeled, with evidence)

### #1 memory reads
- **Console-proven (register reads only)** — session 2026-10-13, probe v0:
  `WT/caps-team-work/cap01-03/probe/EVIDENCE-CONSOLE-20261013.log`. CHECK=PASS
  (kstuff resident + kekcall-eligible), RDMSR EFER=0x11d01 + TSC advancing,
  READ_DR real debug-register values, clean load/log/exit. These use
  EXISTING kstuff sub-ops (1/3/-1) — zero new kernel code.
- **LSTAR "FAIL" root-caused as the probe's own bug**: v0's classifier tested
  `(int64_t)rc < 0` first; a successful LSTAR read returns a canonical kernel
  address (bit 63 set = negative int64), so 100% of successes were
  misclassified. kstuff passes MSR addresses to rdmsr() unfiltered (no
  whitelist). Diagnosis: `WT/caps-team-work/cap01-03/probe/LSTAR-DIAGNOSIS.md`.
- **v1 fix BUILT** (2026-10-13): `cap01-kread-probe-v1.elf` sha
  `2b89f99a9cd3f72f96dea8149854dbf47dec026d135df1b21a8f5238c88380c4`
  (121,352 B, prospero clang, `-Wall -Werror -O2 -g`, clean). kekcall returns
  raw RAX + separate CF flag; classification branches on the flag first;
  every failure branch logs raw rc. Disassembly-verified (no sign test
  remains) — receipt `probe/LSTAR-FIX-V1.md`. v0 source/ELF/evidence
  preserved byte-exact; distinct probe ID + console log path.
- **Host regression test DONE** (14/14 PASS):
  `probe/classifier_v1_regression.c` — high-bit successes (the v0 bug class),
  CF errors, errno-in-RAX errors, zero success, 0xFFF/0x1000 boundary.
- **Arbitrary kernel-VA reads: NOT established.** The only named provider is
  a NEW kekcall KREAD sub-op = Phase B = gated (see
  `cap01-03/KERNEL-READ-ONE-ANSWER.md`, `cap01-03/PROVIDER-ROUTE-VERDICT-V2.md`).
  Byepervisor Reborn = HOLD (`byepervisor/ASSESSMENT.md`).
- 3-restart law: 1 clean boot so far (v0). v1 runs count fresh.

### #3 code patching
- rc-checked 9-step patch/restore protocol implemented:
  `WT/caps-team-work/cap03/src/CodePatch.{hpp,cpp}` + `PatchDiagnostic.cpp`.
- **Real prospero-clang++ build DONE** (KD 2026-10-13):
  `cap03/build/cap03-diagnostic.elf`, 521,192 B, sha
  `35500cb7b901c283a8b06eae956e8c00cd0092279569ccc886dad3adaee550ae`,
  zero errors/warnings; host test 98/98 AFTER two real bug fixes (unpatch
  left pages RWX; verdict-7c retry dead). Authoritative package record:
  `cap03/PACKAGE-MANIFEST.md` (SHAs, verbatim flags, source-revision pins).
- **Console state**: an EARLIER different compile (`cap03-diag-v0.elf`,
  129,344 B, `-nostdlib`) loaded via pldmgr but produced no output — cause
  UNRESOLVED, and it is NOT the packaged payload. No console run of the real
  payload. Target behavior UNVERIFIED (U1-U6 open in `cap03/BUILD-RECEIPT.md`
  §7). Single console procedure: `cap03/SINGLE-TEST-CARD.md` (user OK +
  Phase B gate; one run per boot, no game running).
- **Mechanism caveat for task C3**: read `cap03/DESIGN.md` for the exact
  mprotect/permission-skip mechanism before making claims about what existing
  kstuff behavior supports.

### #13 DLL mods
- **Analyzer**: `cap13/TOOL/pe_analyzer.py` (31/31 host tests). **Sample**:
  23 DLLs in `cap13/SAMPLE/` — IMPORTANT: these are #9-ABI FIXTURE DLLs (our
  own Apple-clang builds against `mbm_mod_entry_v1`), NOT upstream mods.
- **Taxonomy (KH, 2026-10-13 — authoritative)**:
  `cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md`. Key facts:
  - NO genuine unchanged upstream UE4SS C++ DLL mod exists on this Mac. The
    only unchanged upstream PEs are the framework itself
    (`WHAT/.superpowers/native-dependency-spike/windows-image-original-inputs-1/UE4SS.dll`,
    sha256 `94c2ebd3…`, 4256 exports — a loader STRESS input, not a mod; it
    does not export `start_mod`) and its `dwmapi.dll` proxy. All real
    downloaded Townfall mods are Lua/pak/config. Zero upstream DLLs have been
    tested through any part of the path; claims stay "dependency coverage".
  - The closest mod-shaped artifact is KB's reconstruction
    (`cap13/REPORT/evidence/simplest-mod-census/cme_upstream_shape.obj`,
    never linked/run); its 13 UE4SS imports verified string-for-string
    against the real framework export table.
  - **Decisive ABI gap (source-located)**: the target mod's core call
    `?StaticConstructObject@UObjectGlobals@Unreal@RC@@…` (real export,
    ordinal 3562, RVA 0x1224524) is ALREADY IMPLEMENTED in the port runtime
    (`WHAT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp:146-197`,
    consumed at `src/LuaMod.cpp:2949`) but `mbm_host_v1`
    (`cap13/BUILD/src/sdk/mbm/NativeModAbi.h:203-240`) exposes no construct
    row; the staged workaround
    `call_function(controller,"StaticConstructObject",…)` in
    `cap13/BUILD/src/port_cheat_manager_enabler.cpp:44` deterministically
    returns `MBM_E_NOT_FOUND` (reflection-only resolution,
    `NativeModEngineServices.inc:352-356`).
- **Designs (staged, unbuilt)**: native loader
  `cap13/DESIGN/native-dll-loader.md` + `cap13/native-loader/` (PE mapper,
  Win API shim, Win64 bridge asm — UNVERIFIED, DllMain invoker, UE4SS compat
  sketch); managed route `cap13/DESIGN/managed-dll-route.md` +
  `managed-loader/`. 9 blockers in `cap13/REPORT/blocker-list.md`.
- **KF in flight** (agent KF, `cap13/e2e/` — DO NOT TOUCH while it runs):
  Mac E2E proof, status unknown at handoff time; its completion notice will
  update `cap13/e2e/` contents.

## 3. Task list (ordered; each task says done-ness and deliverable)

### C1 (#1) — return-contract verification (KG directive, OPEN)
"Verify the real return contract before adopting carry-flag classification."
The v1 classifier treats CF as authoritative. Verify from the pinned kstuff
source that the error path actually leaves RFLAGS.CF set in the resumed user
thread (kstuff's own client `ps5-kstuff/debug-reader.c:36` assumes so, but
the probe's v0 header noted `uelf/main.c:77-81` writes regs[RAX]=err without
proving flag state). Source: `WHAT/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip`
(sha `b4fcf169…`; a possibly-current extraction exists at
`/tmp/kb-src/kstuff-lite-1.11/` — /tmp is volatile, re-extract from the zip).
Trace the intercept → sub-op failure → trap-frame patch → resume path
(`ps5-kstuff/uelf/main.c:65-84` and the syscall-exit route). Deliverable: a
short verdict doc (does CF carry on error? always / never / depends) + any
classifier consequence. If CF turns out unreliable, the errno-in-RAX
fallback (small-value) branch is already the safety net — state whether it
suffices. The host regression test half of this directive is DONE
(§2#1 above).

### C2 (#13) — StaticConstructObject gap precision (KH directive, PARTLY DONE)
"Define the StaticConstructObject ABI gap precisely and explain whether
fixing it enables the unchanged upstream interface." KH's report already
defines the gap + the one-row fix (`construct_object(ctx, mod, class_object,
outer_object, mbm_object* out)` appended at `mbm_host_v1` table end per its
only-grow rule, `struct_size` bump, honest state-change admission — NOT
claimable as DATA_ONLY). What remains for you: the **enables-what analysis** —
with the row added, walk the reconstruction's full flow (ClientRestart hook →
class/property reads → construct → write-back) and state exactly which steps
work, which still need the explicitly-refused bridge features (aggregate-8
returns, MSVC `std::function` by-value — see
`cap13/native-loader/src/win64_bridge.h` "PER-SIGNATURE HONESTY"), and which
need layout pinning (`FStaticConstructObjectParameters` direct field writes
per SDK version — §3.3 caveat of KH's report). Deliverable: a one-page
enabled/blocked matrix per mod flow step.

### C3 (#3/#1) — existing-permissions-route analysis (user directive, OPEN)
"Do not assume every #3 operation requires a new #1 kernel modification;
establish what the existing permissions route supports." Read
`cap03/DESIGN.md` + `cap03/src/CodePatch.cpp` and enumerate every #3
operation (permission query, patch, verify, unpatch, restore-verify, hook
install/remove). For each: which EXISTING kstuff-lite v1.11 resident
behavior / sub-op covers it (see the dispatch table
`ps5-kstuff/uelf/kekcall.c:19-107` in the pinned zip), and which genuinely
need NEW kernel-resident code (= Phase B). Deliverable: an ops × route
matrix with a bottom line: "what can #3 do today on the existing route,
Mac-argued" — this feeds the console test card when the PS5 returns.

### C4 (#13) — KF completion check + E2E honesty (KF directive)
When KF lands (its directory stops changing / its notice arrives): read
`cap13/e2e/` and classify what the E2E actually exercised — #9-ABI fixtures
vs genuine upstream UE4SS DLLs (per the §2 taxonomy; be blunt). Then define
the NEXT ACCEPTANCE TARGET: "an unchanged upstream DLL with observable
behavior." Honest options, pick and justify:
  (a) `UE4SS.dll` framework itself through the Mac PE loader (legitimate
      stress: 29-DLL import chain, 4256 exports, DllMain→CRT observable;
      but it is the framework, not a gameplay mod — say so);
  (b) a genuinely downloaded upstream C++ mod binary (requires the user to
      supply one — upstream's C++ example mods cannot be compiled on this
      Mac: no MSVC/Windows SDK);
  (c) neither is available → state that the acceptance target is BLOCKED on
      artifact acquisition, and propose exactly what to ask the user for.
Deliverable: E2E classification + acceptance-target decision doc.

### C5 (#13) — retargeting design (user's 3-step distinction, OPEN)
Design the automatic-retargeting pipeline honoring the three DISTINCT steps
from §1 (named-target resolution / cross-build function correspondence /
instruction compatibility). Ground it in what exists: the port's
signature-scan + slot-binding machinery (e.g.
`typescan-r4/candidate/src/UObjectGlobals.cpp:28-33` binds a
signature-scanned engine address), the profile generator (game-specific
values), and KH's import census (which upstream symbols mods actually use).
State for each step what is verified today vs assumed. No code yet — design
doc with the verification obligations named.

### C6 (all) — reporting
Any report you write: separate LOADING vs TARGET RESOLUTION vs WORKING PATCH
BEHAVIOR sections; label every claim Mac-only/console-proven; cite paths +
SHAs.

## 4. Artifact index (quick map)

| What | Path (under WT/caps-team-work unless prefixed) |
|---|---|
| Probe v1 ELF + source + receipt | `cap01-03/probe/cap01-kread-probe-v1.elf` / `KernelReadProbe-v1.c` / `LSTAR-FIX-V1.md` |
| Probe v0 (provenance; never rerun) | `cap01-03/probe/cap01-kread-probe-v0.elf` + `KernelReadProbe.c` (preserved) |
| v0 console evidence | `cap01-03/probe/EVIDENCE-CONSOLE-20261013.log` |
| Classifier regression test | `cap01-03/probe/classifier_v1_regression.c` (14/14 PASS) |
| LSTAR diagnosis | `cap01-03/probe/LSTAR-DIAGNOSIS.md` |
| #1 provider verdict / one-answer | `cap01-03/PROVIDER-ROUTE-VERDICT-V2.md` / `KERNEL-READ-ONE-ANSWER.md` |
| #3 payload + manifest + card | `cap03/build/cap03-diagnostic.elf` / `cap03/PACKAGE-MANIFEST.md` / `cap03/SINGLE-TEST-CARD.md` |
| #3 design + safety | `cap03/DESIGN.md` / `cap03/SAFETY.md` / `cap03/BUILD-RECEIPT.md` |
| #13 taxonomy + gap analysis | `cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md` |
| #13 analyzer + fixtures + sample | `cap13/TOOL/pe_analyzer.py` / `cap13/FIXTURES/` / `cap13/SAMPLE/` |
| #13 designs + staged sources | `cap13/DESIGN/native-dll-loader.md` + `native-loader/` ; `managed-dll-route.md` + `managed-loader/` |
| #13 blockers / feasibility | `cap13/REPORT/blocker-list.md` / `simplest-mod-feasibility.md` |
| KF live E2E (READ-ONLY while running) | `cap13/e2e/` |
| Full status / handover / final report | `WT/STATUS.md` / `WT/INTEGRATOR-HANDOVER.md` / `WT/FINAL-REPORT.md` |
| Console session record | `CONSOLE-SESSION-20261013.md` |
| kstuff pinned source | `WHAT/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip` |
| Port runtime tree (READ-ONLY) | `WHAT/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` |

## 5. What NOT to do (blocked list)

- No console contact; PS5 unavailable (§0.1). No exception for "quick reads".
- No kstuff modification, no new kernel code, no Byepervisor builds/loads.
- No edits to: probe v0 files (provenance), mod files, the integrator's
  candidate tree, frozen builds, `cap13/e2e/` while KF runs, round5 staging.
- No rebuild of cap03 from any source other than PACKAGE-MANIFEST.md's exact
  recipe; a differing sha means a different build — do not mix.
- No rerun of `cap03-diag-v0.elf` (unresolved, different compile); no replay
  of any failed attempt unchanged.
- No claims of "upstream DLL compatibility" beyond "dependency coverage"
  until an unchanged upstream DLL actually executes with observable behavior
  (see C4).
- Do not treat the staged `port_cheat_manager_enabler.cpp` workaround as a
  fix — it deterministically fails (§2 #13).
- Never upload/execute anything on console without per-action user OK; when
  in doubt, stop and ask the user.

## 6. One-paragraph orientation

#1 has console-proven register reads and a fixed v1 probe waiting for the
next authorized window (return-contract verification C1 is the open Mac
task). #3 has a properly built, host-tested 521 KB payload and needs the
existing-route analysis (C3) plus one console run when allowed. #13's honest
frontier is Mac-side: no upstream DLL exists here to run, the decisive
`mbm_host_v1` `construct_object` row is specified but unimplemented, KF's
E2E is finishing on fixtures, and the user's bar is now behavior-works-not
just-loads — your C2/C4/C5 deliverables define what "works" requires before
any console minute is spent.
