# REPORT — worker-preflight — console staging pre-flight (local)

Date: 2026-10-04. Full evidence: `PREFLIGHT-CHECKLIST.md` (same directory). Basis: `worker-staging-pkg-c4/MASTER-STAGING-PACKAGE-V2.md` §A, recomputed locally (`stat -f%z` + `shasum -a 256` on the actual files) — no network, no console contact.

## Result: GO conditional on Gate A0 — 6/7 items clean PASS, 1 item PASS-with-discrepancy, 1 open console prerequisite

1. **9 staged files — PASS (9/9).** Every local source exists; every sha256 and byte size equals the manifest: PRX `de372302…`/30778567; loadlist `49ab7a3a…`/446; settings ini `9f3fa127…`/779; layout pair `f291d552…`/588 + `9c20f7c9…`/186; PS5Pad `1d919fbf…`/2960; main.lua `f586043a…`/21415; enabled.txt `e3b0c442…`/0; UEHelpers `5a8f6609…`/9962. PRX first bytes `4f153d1d` (FSELF) as stated; composition-4 lineage receipt: 33 checks, 0 fail (re-parsed).
2. **ploader.ini — PASS per V2 §C; DISCREPANCY flagged.** All records agree the only required edit is `/data/plugins/modbridge_supplier_composition.prx = 2` under the existing `[PPSA33286]` header (mode 2 LATE), nothing else changed. The briefing's "3 entries: aligned_entry=2, ingame_facts=0, supplier_composition=2" is in NO local record — history staged `modbridge_ingame_facts.prx = 2`, never `= 0`. Newest console readback (2026-10-03 12:45) has NEITHER the aligned nor the supplier line (probe-era entries only), so the controller must be ready to ADD the line at Step 0/4 and cannot assume the frozen aligned-entry line is still present. Console bytes remain unprovable host-side until the Step 0 RETR.
3. **Activation root structure — PASS.** loadlist.txt: correct 5 header rows + exactly 7 file rows, roles `config`/`lua_mod`; per-row sizes equal actual file bytes; total 35890 B; TAB-separated, LF, trailing newline. Byte-identical to `worker-loadlist-final/final-package/loadlist.txt`; rehearsal T1–T6 prove `mfa_find`/all-or-nothing (`MFA_E_SIZE` rc −8, `MFA_E_NO_LOADLIST` rc −2, `MFA_E_TITLE` rc −4).
4. **Layout pair — PASS.** Both files carry exactly the promoted identity: `EngineRevision=5.6.1-0+++UE5`, `BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`, EngineMajor 5 / Minor 6, `NonShipping=0`, `Stats=0`, `prospero-x86_64-sysv`, PointerSize 8; member file adds FUObjectArray (10 members, 0xB8) + TUObjectArray (7 members, 0x20).
5. **Settings PS5 gates — PASS.** `ConsoleEnabled=0`, `GuiConsoleEnabled=0`, `GuiConsoleVisible=0`, `CrashDump.EnableDumping=0`, plus cache/scan keys; no `[Overrides]` section (correctly absent).
6. **Prerequisites — CONDITIONAL + PASS.** `/data/modbridge` console existence/write route: NOT proven by any local record (R-G3-01 "FTP STOR into console dirs unproven — ADVANCED"; installer stage-41 auto-create note) — Gate A0 write-probe is the decider, or the installer route with the same bytes/hashes. Runtime finding the activation root: PASS (exact `mfa_config::root` string `/data/modbridge/roots/PPSA33286` documented and rehearsal-proven).
7. **PREFLIGHT-CHECKLIST.md** produced with per-item PASS/FAIL and exact evidence.

## Action items for the controller (nothing blocks local staging)

- Ignore the "ingame_facts=0" 3-entry briefing shape; execute V2 §C verbatim (add the supplier `= 2` line only if absent; backup + readback).
- Expect Gate A0 to possibly fail (write route historically unproven): no-roundtrip → STOP per §0.
- Re-verify all 9 hashes on-console after STOR (Step 3) exactly as §B mandates; PRX readback must start `7f 45 4c 46` (Gate A1).
