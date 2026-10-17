# One-launch readiness checklist — PREPARED, NOT STAGED

Candidate: complete menu-path package; SHA-256 885f43c69b5c23fd4db422905297268902d2183770ca8d735bf068d3a1d9084c; 31,149,719 bytes; SELF 4f153d1d.

Current blockers: upload qualification remains false; console services unreachable in the saved check. Do not launch the old runtime as if it were this test. No restart is requested.

## Before a user launch

1. Resolve the outstanding promotion gates without treating rejected review, local packaging or this checklist as acceptance. Keep the frozen PRX unchanged.
2. Recheck live console state read-only: Townfall closed, FTP available, current pad/settings/load list preserved, Ploader identity and kstuff state. Process presence alone does not confirm unpaused kstuff.
3. Under the standing eligible-PRX-only approval, freshly download the live /data/plugins/modbridge_supplier_composition.prx with `-Q "SITE SELF"`; record its full hash, size and SELF magic, and retain it as rollback. Last known l18 hash is 444e3ff5d65a76dec046b856f76b44274bcc5f917865603bd77ec37f652c4c4a, but this is historical until re-read.
4. Upload only the frozen owned runtime PRX; decrypt-off readback must equal the full candidate SHA and SELF magic. Change no config, loadlist, autoload, game files or other payload.
5. Start a NEW klog file only for the eligible test. Existing capture helper uses exclusive creation. The command can be prepared as `python3 /tmp/l12-harvest/klog_capture.py --out-dir <this session directory> --window-seconds 1800`. Confirm connected/recording before the user launches.
6. Reload only the existing Ploader if needed within standing approval. Confirm loader lock PID matches a live process and satisfy the current kstuff-unpaused condition; disclose unknown state. No repeated dispatch or automatic game launch.

## The user's single launch

Load a playable save. Release both sticks. Wait for runtime/mod startup, not merely Ploader OK. Press L3+R3 once, release, and wait for queue drain. Two queued toggles can open and close the original menu. If visible, try navigation and close/reopen in that same launch. Preserve the actual menu and spawn observations separately. Leave the game as-is and report the result; no second launch before harvest.

## Harvest ALL boundaries in that launch, read-only

- Match runtime build/startup markers, original mod hash and log epoch to this launch.
- Typed imports=16 and native_thread_import_slot_ok=1; pool source/ranges/elapsed; FName live/disk prologue and ByteProperty round-trip; build/layout facts; any Stopping mod.
- Current PS5Pad.ini; pad borrow result/class/first sample; actual L3+R3-to-F7 event.
- Carrier arm/scan/native callback; native thread-name acceptance or exact refusal; queued callback drain.
- Original UI creation errors or Menu created; native viewport/visibility and user-visible menu. Menu created alone does not prove visibility.
- Real spawn separately; crash registers/module+offset if present; autotune.ini; cleanup/retirement records.

Report console-proven / Mac-only / blocked and the earliest concrete failed boundary. Save all evidence before proposing one next fix. The parameter-bounds guard IS in this build; collect any declared-frame/bounds refusal before interpreting a missing widget. Rollback would upload only the exact freshly saved owned PRX under an appropriate authorization; no game executable restoration or deletion is part of this test.
