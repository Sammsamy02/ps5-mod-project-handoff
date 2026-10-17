# Claude progress update — combined Townfall runtime, pending console test

This is a factual progress packet, not a request to repeat a rejected review or deploy anything. The goal remains a standalone PS5 Nexus installer that adapts formats and runtime APIs automatically, keeps game/build facts in profiles, and runs original PC Lua mods byte-for-byte unchanged. One Townfall success would establish a first case; universal support is not yet demonstrated.

## Latest native evidence

The last harvested console runtime is l18. Pool admission passed all 13 checks in 2297 ms, then the authenticated on-disk FName-prefix route and the real output-pointer/UTF-16 constructor round-trip returned ByteProperty (id 3, number 0). Startup then refused unresolved build-configuration and stats axes and stopped TownfallItemSpawner. No menu or spawn was established. This is one native observation, not three-reboot completion or full API conformance.

## New work complete on the Mac

The combined runtime selects the authenticated Townfall build/layout profile, dependent consumer rebuilds, static native callback thunks with immutable registration identity, a native thread-name adapter, controller-handle borrow retry/lifetime fixes, and retirement/stop guards. Callback publication uses data-pointer swaps rather than new executable detours. Constructor/entry retirement uses sequential consistency and retirement is checked again after the initializer joins.

Two actual full runtime links match: 374 selected units, 391 link inputs, 119,191,728 bytes, SHA-256 abcf511b262cd495367859df28c1ffb27ca2405023ec3bb06ffa656be71ada02. Two separately packaged PRXs also match byte-for-byte:

- SHA-256: 6085a1c43418c8a2e1008e1ba915f7f33207d0764ff6f94a9bf35a29136e33c7
- Size: 31,148,615 bytes; SELF magic: 4f153d1d; SDK: 0x11000000.
- Each passes the preserved 20 static package checks and all 207 exact NID/module/library/version import comparisons. The sole new system getter is scePthreadGetthreadid; the owned import-slot readiness helper is locally defined.
- Both preserve 238 constructor and 2 destructor entries. Packaging preservation does not prove native constructor behavior.

The packaging controller initially failed on extended ELF section numbering (206,055 sections) and legacy/repeated constructor sections. Original failed receipts are preserved. A separate corrected qualifier reproduces that old failure and validates the original checks plus the complete constructor census; seven focused parser controls passed. A prior incorrect symlink staging anchor is also preserved and corrected through an explicit pinned overlay directory. No original failure was silently relabeled.

## Additional separate work — NOT in this PRX

The selected original-menu marshalling trace found no proven deterministic first-menu fault. It did identify a generic zeroed 512-byte parameter buffer without a size/extent preflight. An isolated candidate adds rejection of oversized ParamsSize and invalid parameter offsets/dimensions/extents before marshalling or ProcessEvent. Eighteen plain and eighteen sanitizer controls and the selected whole-TU SDK compile passed. This candidate remains unlinked; it does not fix unproved native nontrivial parameter initialization/destruction, alignment or widget schemas.

The original Nexus mod is TownfallItemSpawner V1.0, Nexus mod 95/file 218. Its main.lua SHA-256 remains f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658, 21,415 bytes. No mod files, PS5Pad.ini or console settings were changed. The user owns Townfall on PS5 only, so no same-game PC reference run exists.

## Honest remaining boundaries

- Native controller borrowing/first input sample/translated F7 is unproved for this combined candidate.
- Recurring carrier execution, authentic game-thread delivery and native UMG/viewport/visibility/spawn are unproved.
- The per-mod Lua borrower/uninstall lifetime issue is separate from whole-runtime code-residence guards.
- The latest on-console PS5Pad.ini has not been reacquired; an older settings copy is not mapping proof.
- The independent cleanup review was automatically rejected for possible cybersecurity risk. It was not retried or rerouted, acceptance remains false, and the standing all-gates upload condition is not met. This packet is not a substitute for that review.
- General Windows DLL compatibility is partial; earlier owned API-family execution does not demonstrate arbitrary downloaded DLLs or other engines.

## Current readiness check

Read-only process and FTP requests to ps5-host-not-configured.invalid ports 8084 and 2121 failed with curl code 7. No current running-game, Ploader, kstuff-toggle, live PRX, mapping or rollback evidence can be asserted. Zero uploads, payload loads, game launches, configuration edits or deletions were performed. Klog has not been started for a nonexistent eligible launch.

The exact candidate remains frozen and on HOLD. Once promotion conditions and console availability are established, use one PRX-only staging transaction, decrypt-off fresh live backup/readback, NEW klog, existing Ploader readiness checks, and ONE user launch. Harvest every boundary together before another launch. Do not repeat the accepted full builds, alter the downloaded mod, silently guess native schemas or treat Ploader OK as visible-menu success.

Included receipts and launch checklist give the exact evidence; original workspace paths are references, not cloud-accessible locations.
