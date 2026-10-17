# agent-GO-capability-audit — verification of the 8-row capability table

2026-10-10 evening. AUDIT ONLY (read-only; no builds, no console, no tree
edits besides this file and `CLAUDE-CAPABILITY-STATUS.md`). Work dir:
`/WORKSPACE/.codex/worktrees/translation-core/what/`. Deliverable:
`outputs/2026-10-08-l25-prep/CLAUDE-CAPABILITY-STATUS.md` (safeguard-compliant
final table).

## Verdict per row

| # | Row | Result of verification |
|---|---|---|
| 1 | Kernel reads = design-only | **SURVIVED** (caveats added). Interface docs exist as cited (FZ-PHASEB-KSTUFF-LITE.md, PHASEB-PLAN.md §2/§3.1; R1+ all "design"). Pin verified BOTH sides by this audit: pinned `outputs/kstuff-lite-pinned/kstuff-lite-v1.11.elf` sha256 = `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` == pldmgr catalog checksum in `outputs/2026-10-04-zcode-team/worker-boot-protocol/probe/list_payloads.json` (version v1.11). GD's prefix citation (agent-GD-phasebc.md gap 1) and PHASEB-PLAN §2 P1's full-hash citation both check out. "Never issued a kekcall" verified: `-DMODBRIDGE_KB_PROBES` appears ONLY inside PHASEB-PLAN.md — zero references in any build/source/config file. Caveats noted: no pin-receipt file in the pinned dir; fetch-URL provenance unreceipted (catalog URL is the itsPLK/ps5-payloads-mirror distribution of upstream EchoStretch/kstuff); §2 P4 byte-identical rebuild not run. |
| 2 | Scanner = Mac-tested + console-proven primitives | **SURVIVED.** phasea-test-receipt.json: 18/18 (astro-refused 0/16+0/244 sha 7723d150…; townfall VERSION_CONFIDENT 5.6, 54/244, sha 3b6cfad4…). Console primitive receipts found: claude2 log L561 `[Seeds] slot 2 FName ctor bound at 0x63d590 and behaviorally verified (ByteProperty probe id == pool id 0x3)` at 00:53:06 (plus slot-1 pool admission, and the l21 REVIEW-TABLE row (b) "console-proven" verdict with the 2026-10-08 09:22:57 lines). SeedTable LEA-census located: `outputs/2026-10-08-l26-package-B/lineage/SeedTable.h` (per-slot rip-relative xref censuses: 1565 / 10284-5-2-18-2 / 1918 sites, "validated in-game"). |
| 3 | Code patching = console-proven vtable/data-level ([EU FIX4]) | **EVIDENCE CORRECTED — one component DOWNGRADED.** The VMT tick pump (EU FIX4) NEVER fired on console: claude2 log has `[EU-VMT] arming refused: non-text-ent (R3 carriers remain the pump)` (00:53:06) and `[ST] pump vmt REFUSED (ufunc line pending first tick)` (00:53:08); "pump vmt ARMED" appears in NO retained console log (only as the expected-format line in agent-EU's design doc). EU's pump is Mac host-fixture-proven + target-compiled only (EU-HOST-FIXTURE.json per agent-EU report). The ROW still stands as console-proven at data/function-pointer level on corrected evidence: `[R3] FuncPtr-marshal event carrier armed: [n: F7BD] @0x0000003000AA8BD0 (callback id 1/2)` (00:53:08, UFunction RegisterPostHook carriers) + 145 `[l22] ExecuteInGameThread queued/drained/callback returned` lines + the in-game menu (CLAUDE-MENU-UP). Instruction-level: detours explicitly compiled out (log: "EngineTick detour unavailable"; "detour registration compiled out under R3 marshal mode"). Final table keeps one line with the split status and the correction flagged. |
| 4 | Early loading = console-proven | **SURVIVED — every cited timestamp verified verbatim.** PHASEC-EARLY-LOADING-DESIGN.md §5 line 147 `#4 (early loading) = SATISFIED-ALREADY`. eboot monitor file NOT purged: preserved at `evidence/tmp-preserved-20261010/tmp/l25-variant-A/eboot-monitor-20261010T051653Z.log` line 5 `2026-10-10T05:52:31Z TRANSITION eboot.bin [] -> [1271]` (bound (05:52:16, 05:52:31] local 00:52:16–00:52:31). claude2 log: `[BUILD] id=l26-20261010T043709Z` 00:53:05; slot-1 pool + slot-2 ctor + delete-listener ops all at 00:53:06; mod Loaded v1.0 00:53:08; 24 pickups 00:54:35; DRY ladder 00:55:14. Kept PHASEC's honest caveat: pre-first-instruction injection is mechanism-proven, not timestamp-receipted. |
| 5 | Debugger = Mac-tested | **SURVIVED + independently re-verified.** Receipt 18/18 includes the klog checks (2 SIGSEGV blocks, eboot base 0x400000 law, build-id 04bb4330…, RIP in libkernel, MAP-RECONSTRUCTED 97.1%). I re-ran `symbolize_crash.py --klog <preserved klog-20261009-021938.log>` myself (read-only, stdout only): 2 blocks parsed, RIP `0x80000224c` → libkernel.sprx +0x224c with fingerprint. Did NOT run run_phasea_tests.py (it rewrites its receipt file = tree edit; prohibited). GF's "needs-X" blocker (no final-link symbols sidecar) kept. |
| 6 | Controller = declined-by-design + userspace console-proven | **SURVIVED.** PHASEB-PLAN.md §3.3 (line 251) "kernel route DECLINED" with the exact rationale (no pad/input kekcalls; IDT hooks = debug-register intercepts; PC UE4SS is userspace too); §3.3 also states userspace PS5Pad "console-proven across sessions (menu-up session used it)". L3+R3 firing receipts found: `evidence/tmp-preserved-20261010/tmp/zzdiag2-diag.jsonl` — `pad_l3r3_edge` → `pad_l3r3_edge_allow/subscribed/modifiers` → `pad_l3r3_f7_dispatched value=118` (118 = F7). F7 menu-open + keybind callbacks in claude2 log. |
| 7 | File overlay = console-proven invasive-form | **SURVIVED with one weakened citation.** result.json verified: phase `in_game_leon_appearance_confirmed_by_user`, gameplay_effect_confirmed true, "Leon appears; game runs normally", base_ucas_modified false, glasses artifact cause_confirmed false, all_rendering_verified false. I re-hashed the retained sidecar (`leon-iostore-adapter-trial-1/overlay/derived/pakchunk0-ps5_s1.ucas`) = `27c7b50a…` exactly matching the staged receipt (stage==receipt==retained). WEAKENED: the screenshot cited in result.json (`/tmp/codex-remote-attachments/…/1-28554.jpg`) is PURGED — visual evidence now rests on the JSON observation fields only. Index-replaced caveat confirmed from plan.json (original .utoc swapped with .bak; derived index + sidecar staged). |
| 8 | Asset conversion = design-only/components | **SURVIVED with phrasing tightened.** GF #8 verdict at PHASEA-PLANS.md:437 ("texture path ready-to-build … mesh path blocked-on-platform-cook research"). Leon: PHASEA-PLANS §#8 records "staged the Leon 2K PC mod unchanged"; sidecar sha equality verified by this audit (row 7). NOTE: the exact phrase "byte-identical" belongs to a different receipt (ORIGINAL-MOD-IDENTITY.md — the TownfallItemSpawner Lua mod), so the final table says "staged unchanged/unconverted (sha-verified)" rather than "shipped byte-identical". No converter exists; the Leon success proves staging (row 7), not conversion. |

## Summary

- **Survived unchanged (6):** rows 1, 2, 4, 5, 6, 8 (row 1 with pin caveats;
  row 8 with phrasing tightened; row 5 additionally re-verified by direct
  read-only tool run).
- **Corrected/downgraded in part (1):** row 3 — the EU FIX4 VMT tick pump
  component is downgraded console-proven → Mac-tested (host fixture); it was
  REFUSED on console in the only session carrying its scope. The row's overall
  data-level console-proven classification survives on corrected evidence (R3
  UFunction event-carrier arms + [l22] ExecuteInGameThread pump receipts).
- **Weakened citation, not downgraded (1):** row 7 — screenshot file purged;
  console-proven claim still stands on result.json + transaction journal +
  sha-verified retained sidecar.
- **Pin event:** both hash sides verified independently (pinned file and
  console-side pldmgr catalog); upstream attribution recorded exactly
  (itsPLK/ps5-payloads-mirror distributing EchoStretch/kstuff v1.11); P4
  source rebuild still pending, so row 1 stays design-only.

Audit trail: all quotes above were re-read from the primary files during this
session; the claude2 session log requires `grep -a` (NUL-interleaved wide
lines) — noted in the final document so future readers don't get empty grep
results.
