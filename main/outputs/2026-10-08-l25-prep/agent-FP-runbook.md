# agent FP — docs/state pass for the MENU-UP wave (round 3)

2026-10-10 early UTC. Docs only — no code, no console, no other files
touched. Deliverables (all under `outputs/2026-10-08-l25-prep/`):

1. **STATE-L26-WAVE.md §9 appended** (earlier sections untouched): the
   MENU-UP milestone with verbatim log lines ([BUILD] l26-20261010T043709Z
   snapshot 24025929; [ST] wcslen-16units PASS; [ST] wstring-rt PASS;
   [LUA-FN] PlayerController id=0x31AAD / ItemDatabaseSubsystem id=0x39B47;
   24 spawnable pickups Pages=3; DRY-1..6 PASS; menu visible in game), the
   ItemId blocker (00:55:47 main.lua:317), the smoking-gun refusal
   ([FN] reason='embedded' len=4 at=1 @ 00:54:35), the reverse-family
   zero-fire (fstring-rev/ftext-rev = 0 lines; forward families ×10 ok=1),
   Claude's round-3 directives, the FJ/FK/FL/FM/FN/FO/FP agent map, and the
   console state (REST MODE; PRX efbfc656 resident; zzdiag v10; settings
   original cache-off; ploader/klogsrv expectations).
2. **WAKE-RUNBOOK-20261010.md created**: numbered wake sequence —
   (1) daemon restore (klogsrv restart via
   `loadpayload:%2Fdata%2Fpldmgr%2Fpayloads%2Fklogsrv-ps5.elf`, Z2 ≤120 s
   "Timeout, shutting down." wait, never double-dispatch);
   (2) zzdiag-v11 deploy via `harvest/lua_fast_deploy.sh` (game closed, ~30 s,
   no PRX change — probe rides resident efbfc656, instrument-first);
   (3) user launch #1 probe, harvest `[PRB-*]`;
   (4) root decision — deploy overnight routed build via
   `prep_modded_launch.py --variant A --prx-path <new>` ONLY if
   `verify_build.sh --manifest` PASS in the receipts, else fix the named hop;
   (5) user launch #2 — byte-identical ORIGINAL mod (FN identity doc) →
   MENU+SPAWN acceptance; (6) 12-mod ladder per FO schema. Plus the reading
   rules (grep -a; UE4SS.log at /data/plugins/UE4SS.log; klog taps
   /tmp/l25-variant-A/; FindAllOf real-nil vs wrapper contract; size-exact
   loadlist; decrypt-off readback; one-launch/clean-exit laws).
3. **CLAUDE-ROUND3-RECEIPT.md created**: Q1→FJ (zzdiag v11 probe), Q2→FK
   (refusal first-10 + call-site tag + hex units), Q3→FL/FM (conversion-site
   inventory + route all reverse sites through the bridge), post-spawn FN
   (freeze + original-mod identity) and FO (12-mod ladder), FP (docs). Status
   honestly marked "in flight (offline)" where artifacts had not landed on
   disk at write time (nothing on disk was newer than
   CLAUDE-MENU-UP-SPAWN-BLOCKED.md); wake-time execution steps cross-linked
   to the runbook.

Evidence cross-checks done: all quoted milestone/blocker lines grepped
verbatim from `evidence/claude2-menu-session-20261010.log`; fstring-rev/
ftext-rev count re-verified 0; DRY 6/6 at 00:55:14 confirmed; command syntax
verified against the actual scripts (`lua_fast_deploy.sh` usage/behavior,
`wakeup_prep.sh` checks-but-does-not-start, `prep_modded_launch.py`
--variant/--prx-path flags, `verify_build.sh --manifest` usage,
`harvest.py --label`, Z2 120 s law from agent-Z2).

— agent FP (ZCode), 2026-10-10.
