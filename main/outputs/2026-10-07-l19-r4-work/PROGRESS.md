# Persistent PS5 progress — l22 console harvest, 2026-10-08

Console-proven for ONE launch, PID196 / runtime epoch68114406: exact approved PRX eb32990807c942730c7c404482b0b57a74bd2f9ed218e10f35f0e0f899f80dec ran. Profile digest matches;401 checks passed. Exact63 preflight:49 required present,2 conditional present,10 conditional missing,2 runtime deferred,missing-required0. Pool admission2630ms/shared60000ms; FName ByteProperty native round-trip passed, binding2688ms. Slot4 static cache admitted, not a native SCO call proof.

Current blocker: type-system initialization stopped on missing FMapProperty::KeyProp. Runtime status failed; UE4SS stopped TownfallItemSpawner at10:57:32 before Lua started. No [l22] progress/error markers, keybinds, controller borrow/state/edge, queued work or widgets observed. Early pad deferred error80920004 persists but is not this launch's first blocker. Game remained alive; no actual crash;autotune empty. Original mod bytes unchanged.

Mac-only source trace: TypeChecker UE>=5.6 MapProperty calls KeyProp thenValueProp. Those were absent from bounded63 join. Adjacent Set ElementProp also absent; Array Inner present but join incorrectly calls it only spawn-conditional. Do not invent offsets or claim635-key closure/all17nativeallocator calls.

Read-only harvest completed, with zero uploads/reloads/launches/config edits/deletions during harvest. All captures are NEW files. Report: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l22-package-r2/console-test-20261008T155200Z-634c29/harvest-20261008T155939Z-0a44ac/CONSOLE-REPORT.md
Claude ZIP: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l22-package-r2/console-test-20261008T155200Z-634c29/harvest-20261008T155939Z-0a44ac/l22-console-review.zip (25 manifest files verified).

Next: user/Claude review; proposed bounded type-initialization profile/preflight correction after independent measurements. No fix applied or approved by this record. User may close game; no relaunch until a new approved test. l22 local twin-build/package receipts remain under outputs/2026-10-08-l22-package-r2. Klog session13375 remains the owned recording tap; snapshot is saved.
