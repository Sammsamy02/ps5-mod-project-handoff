# Final Report: #1, #3, #13 (ZCode, 2026-10-12 through 2026-10-13)

## EXECUTIVE SUMMARY

Three capabilities pushed from concept to concrete deliverables. One has
console evidence. All have compiled source. All are honestly labeled.

| Capability | Status | Console evidence | Next step |
|---|---|---|---|
| #1 kernel reads | Probe ran on console, 3/4 PASS; LSTAR root-caused + fixed (v1 built) | ✅ CHECK+RDMSR+READ_DR | Run v1 probe next authorized window; 3-restart law |
| #3 code patching | PS5-compiled, host-tested 98/98 | ❌ (probe loaded but produced no output due to compile issue) | Deploy KD's proper build |
| #13 DLL mods | PE analyzer + DLL sample + loader design + upstream-path analysis | ❌ | KF's E2E proof (in flight) |

## #1 KERNEL READS

### Console-proven (this session's log)
- kstuff kekcall reachable from standalone no-game probe (CHECK=PASS)
- Real kernel register data read (READ_DR: DR0-7, real values)
- Real MSR reads (TSC advancing, EFER=0x11d01)
- Clean load, execution, log, exit — no crash

### Console failure — ROOT-CAUSED AND FIXED (2026-10-13, offline)
- LSTAR "failure" was a probe bug, NOT a kstuff fault: v0's classifier
  tested `(int64_t)rc < 0` first, and a successful LSTAR read returns a
  canonical kernel address (bit 63 set = negative int64) — 100% of
  successful reads were misclassified. kstuff passes MSR addresses to
  rdmsr() unfiltered (no whitelist). Diagnosis: probe/LSTAR-DIAGNOSIS.md.
- Fix implemented and built: probe v1 (raw RAX + separate CF flag,
  classify on the flag, raw rc logged on failure). Verified in the
  compiled binary by disassembly — no sign test remains. v0 source/ELF/
  console evidence preserved byte-exact; v1 has a distinct probe ID and
  log path. Receipt: probe/LSTAR-FIX-V1.md.

### Pending
- v1 console run (next authorized window) — expected LSTAR PASS
- Arbitrary kernel-VA reads (KREAD sub-op, Phase-B-gated, user-OK-gated)
- 3-restart validation law (only 1 boot so far)

### Key files
- Probe v1 (current): cap01-03/probe/cap01-kread-probe-v1.elf (sha 2b89f99a...)
- Probe v1 source: cap01-03/probe/KernelReadProbe-v1.c
- Probe v1 receipt: cap01-03/probe/LSTAR-FIX-V1.md
- LSTAR diagnosis: cap01-03/probe/LSTAR-DIAGNOSIS.md
- Probe v0 (provenance): cap01-03/probe/cap01-kread-probe-v0.elf (sha 12ae76a0...)
- v0 source (preserved): cap01-03/probe/KernelReadProbe.c
- Console log: cap01-03/probe/EVIDENCE-CONSOLE-20261013.log
- Design: cap01-03/PROVIDER-ROUTE-VERDICT-V2.md
- Single answer: cap01-03/KERNEL-READ-ONE-ANSWER.md
- Test card: cap01-03/probe/TEST-CARD.md

## #3 CODE PATCHING

### Status: source-backed candidate; target behavior unverified
- Full implementation: cap03/src/CodePatch.cpp/.hpp (JJ + KD)
- PS5-compiled: build/ps5-obj/*.o (KD, prospero-clang++ correct flags)
- Linked payload: build/cap03-diagnostic.elf (521KB, KD)
- Host-tested: 98/98 PASS after 2 real bug fixes (KD)
- Bugs caught: unpatch left pages RWX (critical); retry loop broken

### Console attempt (failed — wrong compile)
- My quick compile (129KB, -nostdlib) loaded but produced no output
- KD's proper build (521KB, correct flags, bug-fixed) is the real version
- Next console window: deploy KD's build

### Key files
- Source: cap03/src/CodePatch.cpp/.hpp, PatchDiagnostic.cpp
- Build receipt: cap03/BUILD-RECEIPT.md
- Host test: cap03/build/host/run-output.txt (98/98)
- Test card: cap03/SINGLE-TEST-CARD.md
- Safety: cap03/SAFETY.md
- Design: cap03/DESIGN.md

## #13 DLL MODS

### Completed
- PE analyzer: TOOL/pe_analyzer.py (31/31 tests, cross-validated)
- DLL sample: 23 binaries built/collected, PE-analyzed (SAMPLE/)
- Dependency coverage measured: 69% need nothing beyond host services
  [NOTE: these are #9-ABI fixture DLLs, NOT real upstream mods —
  percentages don't generalize to upstream binaries]
- Simplest mod: cap13-mod-keybinds-noeh.dll (1 import DLL, 1 export)
- Feasibility: 9 blockers identified, 3 closed, 1-row ABI gap is decisive
- Native loader design: PE mapper + ABI bridge + Win API shim (JH)
- Managed route design: SDK-only v1, BepInEx structurally impossible (JI)

### Landed 2026-10-13 (agent KH) — unchanged-upstream-DLL path analysis
- Inventory truth: NO genuine upstream UE4SS C++ DLL mod exists on this
  Mac — only the framework itself (UE4SS.dll, 4256 exports, a loader
  stress input, not a mod) + our fixtures. All real downloaded Townfall
  mods are Lua/pak/config. No upstream DLL has been tested through the
  path; claims stay "dependency coverage."
- Decisive ABI gap source-located: the target mod calls
  StaticConstructObject (real export, ordinal 3562); the port runtime
  already implements it (typescan-r4 src/UObjectGlobals.cpp:146-197) but
  mbm_host_v1 has no construct row, and the staged #9 workaround
  (call_function "StaticConstructObject") deterministically returns
  MBM_E_NOT_FOUND (reflection-only resolution). One-row fix specified:
  append construct_object(...) at table end.
- Report: cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md

### In flight
- KF: genericity audit + Mac E2E proof (loading real DLL through PE loader)

### Corrections applied
- "Dependency coverage" not "measured compatibility"
- KA's DLLs labeled as fixtures, not real upstream mods
- "0% EH required" retracted (only the no-exceptions variant was tested)
- Static TLS checked separately from TLS imports

### Key files
- PE analyzer: cap13/TOOL/pe_analyzer.py
- Sample: cap13/SAMPLE/ (23 DLLs)
- Analysis: cap13/REPORT/dll-sample-analysis.md
- Feasibility: cap13/REPORT/simplest-mod-feasibility.md
- Blocker list: cap13/REPORT/blocker-list.md (9 blockers)
- Native loader: cap13/DESIGN/native-dll-loader.md + native-loader/
- Managed route: cap13/DESIGN/managed-dll-route.md + managed-loader/

## ROUND5 RUNTIME FIXES (staged, not applied)

| Fix | Patches | Source |
|---|---|---|
| Lookup speed | 6 patches (round5-staging/lookup-speed/) | IT agent |
| GROUP B APIs | 3 patches (round5-staging/group-b/) | IU agent |
| Shim write-area | 1 patch (round5-staging/shim-fix/) | IQ agent |
| Lua allocator | 3 patches (round5-staging/lua-alloc/) | IS agent |
| No-detour gate | 1 tool (harvest/no_detour_gate.py) | IR agent |

All staged in the main tree area, none applied. Re-pinning required
(all pins are stale against current tree). See INTEGRATOR-HANDOVER.md.

## DELIVERABLES INDEX

| File | Location |
|---|---|
| STATUS.md | caps-resume-20261013/STATUS.md |
| INTEGRATOR-HANDOVER.md | caps-resume-20261013/INTEGRATOR-HANDOVER.md |
| Console session record | caps-team-work/CONSOLE-SESSION-20261013.md |
| Reviewer corrections | caps-team-work/CORRECTIONS-FROM-REVIEWER.md |
| #1 single answer | caps-team-work/cap01-03/KERNEL-READ-ONE-ANSWER.md |
| #1 probe binary (current, v1) | caps-team-work/cap01-03/probe/cap01-kread-probe-v1.elf |
| #1 probe v1 fix receipt | caps-team-work/cap01-03/probe/LSTAR-FIX-V1.md |
| #1 LSTAR diagnosis | caps-team-work/cap01-03/probe/LSTAR-DIAGNOSIS.md |
| #1 console log (v0, provenance) | caps-team-work/cap01-03/probe/EVIDENCE-CONSOLE-20261013.log |
| #3 source | caps-team-work/cap03/src/ |
| #3 build receipt | caps-team-work/cap03/BUILD-RECEIPT.md |
| #3 package manifest | caps-team-work/cap03/PACKAGE-MANIFEST.md |
| #3 test card | caps-team-work/cap03/SINGLE-TEST-CARD.md |
| #13 PE analyzer | caps-team-work/cap13/TOOL/pe_analyzer.py |
| #13 sample | caps-team-work/cap13/SAMPLE/ |
| #13 analysis | caps-team-work/cap13/REPORT/dll-sample-analysis.md |
| #13 feasibility | caps-team-work/cap13/REPORT/simplest-mod-feasibility.md |
| #13 blockers | caps-team-work/cap13/REPORT/blocker-list.md |
| #13 upstream-DLL path analysis | caps-team-work/cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md |
