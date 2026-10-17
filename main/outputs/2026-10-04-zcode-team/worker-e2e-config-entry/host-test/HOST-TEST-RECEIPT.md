# HOST-TEST-RECEIPT — config-entry revision, compiled-in entry, host validation

# Identity

- patched binary: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-e2e-config-entry/host-test/build/modinst_composed_cfgentry
- patched binary sha256: 4659c24c4a4d829dc13a59813e7b137bf4d0efeaf0bca45b21e064b589eb2aa1
- patched .o sha256: 51b881715d9a43f7c29886622f5d74becee5b38f02a93198f02549688c52ec40
- owned source sha256: 43ea66c1c5dcf52bfb9158bae5f782438911b61f699eeb1f8dc29c6b702dcda0
- pristine base sha256: 45c04d084215cb5da520ff1a5bf70219f5ce0e86baf55bda4da706dd8dda044d
- linked import census: identical to pristine D1 binary (nm -u diff empty; no plain strtol anywhere)

## A. bare dispatch (argc<2, compile-time slot absent on host)

OK    bare dispatch exit 2 (got 2)
OK    bare dispatch refusal line on stdout
OK    bare dispatch prints usage (today's shape)
OK    bare dispatch writes no status lines
OK    bare dispatch creates no state dir

## B. stage 0..6 replay via compiled-in entry (--e2e-cmd)

OK    D0/stage0: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage1-owner: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage2a: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage2b-1: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage2b-2: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage2b-3: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage3: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage4: exit 0 + consumed-rename (exit 0, consumed=True)
OK    records-after-4: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage5: exit 0 + consumed-rename (exit 0, consumed=True)
OK    records-after-5: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage6-1: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage6-2: exit 0 + consumed-rename (exit 0, consumed=True)
OK    stage6-3: exit 0 + consumed-rename (exit 0, consumed=True)
OK    closing-records: exit 0 + consumed-rename (exit 0, consumed=True)
OK    transcript == 35-line corrected ledger (35 lines)
OK    transcript sha256 == 31a4764a4ae13afa... (got 31a4764a4ae13afa...)

## C. legacy-argv twins (hand-written CLI form), same patched binary

OK    legacy-argv transcript byte-identical to entry transcript
OK    legacy-argv transcript byte-identical to staging-final twin (PRISTINE D1 binary) -> legacy argv unaffected

## C2. recover probe via compiled-in entry

OK    recover: exit 0, begin/end recovery shape, consumed (exit 0, 2 lines)

## D. stale re-dispatch (consumed slot)

OK    stale re-dispatch exit 2 + absent line (exit 2)
OK    stale re-dispatch changed zero state

## E. argv>=2 without --e2e-cmd never consults the entry

OK    legacy records dispatch exit 0 (got 0)
OK    leftover valid cmd file NOT consumed by legacy dispatch

## F. refusal matrix (14 cases, schema sec 6) via compiled-in entry

| case | expected | exit | stdout reason | report line |
|---|---|---|---|---|
| unknown-top-key | exit 6 | 6 | yes | no report line | OK |
| unknown-option-key | exit 6 | 6 | yes | no report line | OK |
| deny-verb-make-bundle | exit 6 | 6 | yes | no report line | OK |
| deny-option-crash_at | exit 6 | 6 | yes | no report line | OK |
| bad-schema | exit 6 | 6 | yes | no report line | OK |
| mods-with-remove | exit 6 | 6 | yes | no report line | OK |
| mods-on-records | exit 6 | 6 | yes | no report line | OK |
| unicode-escape | exit 6 | 6 | yes | no report line | OK |
| oversize | exit 6 | 6 | yes | no report line | OK |
| no_auto_bundle-false | exit 6 | 6 | yes | no report line | OK |
| title-on-recover | exit 6 | 6 | yes | no report line | OK |
| relative-state | exit 6 | 6 | yes | no report line | OK |
| unknown-verb | exit 6 | 6 | yes | no report line | OK |
| absent-file | exit 2 | 2 | yes | no report line | OK |
OK    14/14 refusal cases behave as designed

## G. `--e2e-cmd` without a value stays legacy grammar

OK    valueless --e2e-cmd -> unknown flag usage + exit 2 (exit 2)
OK    valueless --e2e-cmd wrote no status lines

## Scope honesty

All results are HOST-side, exercising the ACTUAL compiled-in entry of the patched binary through the --e2e-cmd override slot (the designed host harness entry point). The compile-time slot /data/modbridge/intake/e2e-cmd.json itself is only reachable on console; bare-dispatch behavior with that slot absent is proven identical to today's usage()+EX_BAD. Console STOR/readback/consumed behavior, the fresh tuple IDs (g1-cmd-entry-*), suites re-run (bridge 75 / F-R2 87 / fault 186 + new config-entry cases), the new build receipt and the Step P re-pin remain the build worker's and controller's work; console results remain [TARGET] until that run.

OVERALL: PASS

