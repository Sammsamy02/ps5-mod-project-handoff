# Updated complete menu-path candidate — Mac-only

All selected profile, callback, native-thread, controller-borrow and shutdown fixes are in this candidate, now together with the reflected UFunction parameter-bounds preflight. The original Item Spawner release remains unchanged. This package has not run on the PS5; the visible menu and item spawn remain unproved.

## Exact artifact

`pass-1/modbridge_supplier_composition.prx` and pass-2 are byte-identical: **31,149,719 bytes**, SHA-256 **885f43c69b5c23fd4db422905297268902d2183770ca8d735bf068d3a1d9084c**, SELF magic **4f153d1d**, SDK **0x11000000**. These supersede the 6085a1c4 combined candidate for a future test; neither has been uploaded. The last observed console runtime remains l18, not this build.

## What changed in this integration

Only the `lua-luauobject` runtime translation unit changed. The preflight checks the declared parameter size and every parameter/out/return extent before any pusher or ProcessEvent. It avoids signed multiplication overflow. It refuses malformed/oversized frames without adding a new engine-call ABI or editing a mod. It does not provide nontrivial parameter initialization/destruction, alignment validation or unknown widget layouts, and is not proof of a first-menu fault.

The actual source-extracted control body was reproduced in a new directory: the old implementation incorrectly accepts 9 invalid metadata cases and faults under ASan in 2 cases. The selected candidate passes **18 plain + 18 fatal ASan/UBSan** controls. Both fresh SDK compiles used the admitted recipe; emitted dependencies match the old include graph after normalizing only the source and object paths. All 373 other selected unit source/object pairs and 17 fixed inputs remained pinned. Both complete **374-unit / 391-input** runtime links are byte-identical: 119,193,344 bytes, SHA cefdc383a373462c746f1327dfeb093e169971226fb9b4c84bccdba82a6924b6. Their raw undefined-symbol sets are unchanged.

Both bootstrap/package passes completed with **20/20 static checks**, **207 exact import identities**, **238 init / 2 fini entries**. All 207 import identities are identical to the previous combined candidate. The extended ELF/legacy-array checker correction was selected directly in this new build controller, so there is no known parser exception in these receipts. Historical failures remain untouched in the previous package directory. Four import-checker and seven ELF parser controls passed.

## Still required from the console

Earlier l18 proved pool admission and the ByteProperty FName round-trip, then refused build/layout facts. The profile remedy is locally selected. Authentic pad borrowing/input/F7, recurring callback carrier, accepted game-thread delivery, native original menu/viewport/visibility and spawn remain to be measured together in ONE launch. The known per-mod callback borrower/uninstall lifetime gap is not fixed by this parameter guard or the whole-runtime residence guard; hot uninstall is not a test-success claim.

The latest read-only HTTP8084/FTP2121 checks failed with curl7. No current loader/game/kstuff state, fresh live PRX rollback or current PS5Pad.ini is verified. Zero uploads, payload loads, game launches, config edits or deletions. Klog has not been started for a test that cannot yet be staged.

The independent lifecycle cleanup review remains unfinished after its request was automatically rejected for possible cybersecurity risk. That was a review-request rejection, not a console upload rejection; it was not retried or rerouted. Local tests do not count as independent acceptance. The previously recorded promotion hold is retained, and the standing conditional upload is not yet qualified. No new gate, review attempt or restart was introduced by this integration.

Next: restore existing console services, resolve the recorded promotion condition, then follow the unchanged PRX-only backup/readback/NEW-klog/one-user-launch procedure and harvest all boundaries before another launch. The downloaded Lua release remains immutable.
