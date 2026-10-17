# Current PS5 standalone DLL diagnostic build — updated 2026-10-04

CURRENT: INITONCE_DLL_TEST.elf
- Build ID: 1a6b37b7a46c459cb630a458b7a736a3
- SHA-256: bc0574d662909eb60471984a693be2ffc57490f8095902ee794b3f9272992379
- Bytes: 241784
- Console path: /data/pldmgr/payloads/INITONCE_DLL_TEST/INITONCE_DLL_TEST.elf
- Status: UPLOADED, READBACK-VERIFIED, DISPATCHED ONCE, TARGET RUN PASSED
  (all expected values matched incl. init_once:{enabled:1} and 46/10 imports; see
  outputs/2026-10-04-zcode-controller/console/1a6b37b7-console-receipt.json)
- Profile: entry/clock + owned worker threads + critical sections + SRW locks +
  condition variables + address waits in their exercised forms (creator-side
  zero-timeout admission checks and wake calls; worker-side blocking path not
  proven invoked) + InitOnceBeginInitialize/InitOnceComplete through actual PE
  imports with documented-semantics assertions. Standalone SDK ELF diagnostic;
  not a PRX, not a UE4SS/game mod test.
- Superseded intermediate: c49108fe (first InitOnce composition; omitted
  --report-path so its target log went to /tmp; dispatched once with no readable
  result — corrected by this build).

PREVIOUS (history, do not overwrite):
- NEXT_DLL_THREADS.elf — build 6d295f56 — PASSED (workers/sync profile).
- TEST_THIS_DLL.elf — build f72faea5 — PASSED (entry/clock profile).

Local aliases in this directory mirror the console payloads byte-for-byte.
