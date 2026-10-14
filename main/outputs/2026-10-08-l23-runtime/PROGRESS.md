# l23 current progress

## Verified result — Mac-only

- l23 runtime and PRX twins match. Each runtime pass rebuilt all 125 dependent units; 249 unchanged units were retained in the complete 391-input link.
- PRX: 31,324,743 bytes; SHA256 `0da199eb954529bbe8fdc563a8c58d76853d560feba0230ad2fd4374b3f0d4e1`; SELF magic `4f153d1d`; SDK `0x11000000`.
- Both packages passed all 20 static gates. The exact 207 final import identities and raw runtime import set are unchanged.
- All 93 existing profile facts are unchanged. Map KeyProp/ValueProp and Set ElementProp remain absent/conditional. Array Inner is startup-required.
- Included: optional nested discovery, collect-and-continue type scan, validated pointer getters and genuine game-thread queue self-test with one 30-second timeout report. VMT fallback remains documentation only.
- Full closure found two owned-array assignment consumers missed by isolated tests. The preserved first build was not packaged. Corrected typescan-r2 adds bounded owned writes and validates Next before traversal. Ten regression runs, six target unit compiles and independent review passed before the successful twin builds.

## Track B

The generic-header route now produces actual compiler measurements without seeking any Sony PS5 platform extension. Current table: 459 measured of 711 keys; calibration 84 matches, six mismatches and three UHT-dependent unresolved entries. New values: 314 platform-independent candidates, 55 platform-dependent candidates requiring image coverage. Four container measurements match the saved image but remain deferred to l24. No new value was promoted into l23.

## Unproved / next step

No console contact, upload or launch occurred. Queue delivery, menu and spawn are not console-proven. The complete local review packet is being finalized with Track B receipts; upload remains pending the user's explicit OK. Epic source stays local and uncommitted.

Current build receipts: ../2026-10-08-l23-package/FINAL-RECEIPT.json and BUILD-REPORT.md.

## Staging 2026-10-08T17:19:17.196990+00:00
# l23 staging result

## Console file transfer proven
Fresh decrypt-off l22 backup preserved in before/rollback-live.decrypt-off.prx (eb32990807c942730c7c404482b0b57a74bd2f9ed218e10f35f0e0f899f80dec). l23 uploaded once to the approved runtime path; decrypt-off readback matched SHA256 0da199eb954529bbe8fdc563a8c58d76853d560feba0230ad2fd4374b3f0d4e1, 31,324,743 bytes, SELF 4f153d1d. All 13 tracked comparisons unchanged apart from the authorized runtime directory row. Townfall absent in both pre-upload and pre-arm process snapshots.

## Arming incomplete
New klog file connected at 17:17:54 UTC. Existing Ploader requested exactly once; HTTP returned OK. Subsequent process query timed out twice and FTP lock read timed out. This does not prove a crash or successful arming. Last capture contains a Shell UI missing-heartbeat line and process memory stats, without a confirmed panic. kstuff PID87 existed before arming; unpaused state not exposed. Local capture remains available, but continuing live delivery cannot be asserted. No remote game launch performed; no config changes.

## Next
Hold user launch until readiness established; ask what console shows and whether it responds. Do not resend blindly. Runtime l23 execution/menu/spawn remain untested. l24 held until l23 harvest review.

Evidence: UPLOAD-RECEIPT.json, PREUPLOAD-MANIFEST.json, UNCHANGED-FILES.json, PLOADER-ARMING.json, process/FTP stderr files, PRELAUNCH-STATUS.json, and new klog in this directory.

Session: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76

Readiness 2026-10-08T17:37:46.206704+00:00: fresh klog receiving; PID102 eboot and Townfall sandbox still present. Ploader lock97 matches live process. No new payload dispatch. Waiting full game close; l24 held.

2026-10-08T17:38:38.350935+00:00: Townfall exit confirmed; Ploader97 lock matches running process, kstuff87 running, fresh klog receiving. Ready for one user launch; no additional payload dispatched.

2026-10-08T17:45:40.405285+00:00
# l23 harvest + storage review

Console-proven: l23 ran in PID127. Preflight passed all50 required keys; FName/pool checks passed. Type scan then refused `[PropertyPointer] unreadable field or unknown property FField class refused`; no TypeScan-complete, Lua-mod start, self-test, input or menu success. Game remained alive. See CONSOLE-REPORT.md.

Read-only storage survey: /data/homebrew558.64GiB logical game files/images; saved media14.76GiB; downloads7.23GiB; plugins0.31GiB. No large crash dump found in scanned paths. These are logical sizes, not allocated blocks; cause of recent free-space decline remains unproved. See storage/STORAGE-REPORT.md.

No console changes during harvest or storage survey. l24 not started. Proposed next runtime work is a local review of property-pointer validation/readability and class recognition during type-system bootstrap; no code changes yet.

Packet: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/L23-HARVEST-AND-STORAGE.zip
