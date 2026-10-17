# #7 step 2 — runtime mount without replacing game files

Mac-only reviewed source. Use the current 15-file stage identified in BUILD-INTEGRATION.md and REVIEW-CLOSURE.md; cumulative integration and the exact build gates still precede the console test. This does NOT use the step1 installer. No console action has been performed by this stage.

Integrator prepares one launch, with the user's authorization for each applicable console action:

1. Establish a fresh pristine game baseline. The Oct13 queue reports repair followed by step1 install, so its final state was INSTALLED. Restore that plan and verify all recorded game Paks files before this test; do not assume the old Oct8 state. No original mod trio may remain under the game's Paks tree. Preserve rollback/backups.
2. Copy the unchanged selected mod into configured native-root/pakmods, including its matching sidecars. Record source/deployed hashes. Code discovers this directory automatically; no title/mod name is compiled into logic. Keep the directory and engine delegates/allocator stable during this launch.
3. Use the existing component reviews, compose the exact current patch with other approved components, run selected-source gates, build/verify the exact PRX once, and record the known-good rollback. Do not reuse historical R11 as the new build's verdict.
4. Read back game-directory hashes and unchanged mod hashes before and after launch. Arm the existing log capture; one integrator contacts the console. Launch once and reach the playable scene.

PASS needs ALL: source/gate/build provenance; `[PAKMOUNT] status=0 call=1` with nonzero `ret`, increased synchronized count and returned identity uniquely highest at the post-call snapshot; the user sees the replacement content; every original game-file hash is unchanged. Engine mount message from existing output, where emitted, corroborates the path. Later asynchronous mounts can change precedence: the marker proves only its reported synchronized snapshot.

Leon may still be checkered gray/white because its PC material shaders are a separate #8/#12 issue; that does not make successful #7 content mounting a shader-compatibility pass. Darkness/brightness must not be attributed to this mechanism if an old config pak remains installed.

Pre-call refusal (`call=0`) means this attempt did not issue MountPak. Missing profile signatures, wrong instance/receiver/adjustment, unknown wrapper, invalid request, unreadable snapshot or priority overflow is a stop, not permission to bypass admission. After any call, null return/exception/count or precedence mismatch yields `MountedUnverified`; the engine may already have changed mount state. Do not call that vanilla, unmounted, safe to retry or automatically roll it back. Engine fatal errors/OS faults are not caught by C++ catches. Record evidence and stop on crash, changed original files or unverified side effects.

Rollback of the new runtime build and removal of externally supplied mod files are integrator/user actions; this component adds no unmount or console repair path. No kernel, detour, code-protection or game-executable changes are involved.
