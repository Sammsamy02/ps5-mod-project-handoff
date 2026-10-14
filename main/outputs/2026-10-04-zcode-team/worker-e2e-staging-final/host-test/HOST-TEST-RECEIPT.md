# HOST-TEST-RECEIPT — config-file entry design, host emulation

## A. staged console cmd files vs bounded grammar

| file | verdict |
|---|---|
| stage0-records.json | OK |
| stage1-register.json | OK |
| stage2a-prepare-archive.json | OK |
| stage2b-1-register.json | OK |
| stage2b-2-register.json | OK |
| stage2b-3-prepare.json | OK |
| stage3-install.json | OK |
| stage4-activated.json | OK |
| stage0-records.json | OK |
| stage5-mods-started.json | OK |
| stage0-records.json | OK |
| stage6-1-remove.json | OK |
| stage6-2-remove.json | OK |
| stage6-3-remove.json | OK |
| stage0-records.json | OK |
| recover.json | OK |

## B. host replay through the real binary (synth argv)

| D0/stage0 | exit 0 | status lines now 2 | OK |
| stage1-owner | exit 0 | status lines now 4 | OK |
| stage2a | exit 0 | status lines now 8 | OK |
| stage2b-1 | exit 0 | status lines now 10 | OK |
| stage2b-2 | exit 0 | status lines now 12 | OK |
| stage2b-3 | exit 0 | status lines now 14 | OK |
| stage3 | exit 0 | status lines now 17 | OK |
| stage4 | exit 0 | status lines now 19 | OK |
| records-after-4 | exit 0 | status lines now 21 | OK |
| stage5 | exit 0 | status lines now 23 | OK |
| records-after-5 | exit 0 | status lines now 25 | OK |
| stage6-1 | exit 0 | status lines now 28 | OK |
| stage6-2 | exit 0 | status lines now 31 | OK |
| stage6-3 | exit 0 | status lines now 33 | OK |
| closing-records | exit 0 | status lines now 35 | OK |

Transcript lines: 35 (corrected ledger: 35). Verbatim match: True.

## C. argv twins (hand-written CLI form), byte-compare


Twin transcript byte-identical to synth-argv transcript: True.

## B2. recover probe (interrupted-transaction verb shape)

exit 0, lines ['{"event":"begin","cmd":"recover","title":"","stage":"recovery"}', '{"event":"end","exit":0}'], matches begin/end recovery shape: True

## D. refusal matrix (schema sec 6)

| case | expected | got |
|---|---|---|
| unknown-top-key | REFUSE | REFUSE: unknown top-level key 'extra' (sec 6) | OK |
| unknown-option-key | REFUSE | REFUSE: unknown options key 'colour' (sec 6) | OK |
| deny-verb-make-bundle | REFUSE | REFUSE: deny-listed verb 'make-bundle' (sec 2) | OK |
| deny-option-crash_at | REFUSE | REFUSE: deny-listed option 'crash_at' (sec 2) | OK |
| bad-schema | REFUSE | REFUSE: schema must be exactly 1, got 2 (sec 2) | OK |
| mods-with-remove | REFUSE | REFUSE: mods together with options.remove refuses (sec 3) | OK |
| mods-on-records | REFUSE | REFUSE: mods present with verb 'records' refuses (sec 2) | OK |
| unicode-escape | REFUSE | REFUSE: \uXXXX escape refuses (sec 5) | OK |
| oversize | REFUSE | REFUSE: file 4300 B > 4096 refuses before parsing (sec 5) | OK |
| no_auto_bundle-false | REFUSE | REFUSE: only bool true is allowed (sec 5) | OK |
| title-on-recover | REFUSE | REFUSE: options.title illegal for verb 'recover' (eager refusal, sec 3) | OK |
| relative-state | REFUSE | REFUSE: state_root.state must be absolute (sec 2) | OK |
| unknown-verb | REFUSE | REFUSE: verb 'transmogrify' not in allow-list (sec 2) | OK |
| absent-file | REFUSE | REFUSE: file absent (sec 1: usage() + EX_BAD, nothing executes) | OK |

14/14 refusal cases refused.

## Identity

- host binary: /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-d1-chain-relink/composition/build/host/modinst_composed
- host binary sha256: 8dffe3cfba5fb211601b3430ca7d990faff4bb53ac6c2117be9f1840a01c8534
- console payload (NOT used on host): sha256 e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436, 681528 B
- movspeed fixture sha256: 3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad
- mm-skin.mfb sha256: 7b9888b9c647fc19ecd1631c17981ec320f69472260646b6b8c000e5cd9b70af
- mm-lua.mfb sha256: eefc774a589d412d827c486e6d08a4f3eaee5adb4b87f8df6f9a70bc7bd639ec
- full transcript sha256: 31a4764a4ae13afac76db4e50e0905789a6b07294acb5943ebd2affb3b5ebeed

## Scope honesty

This is a HOST-SIDE EMULATION of the entry rule (validator + argv synthesis)
feeding the REAL host binary parser/dispatch. The console entry itself does not
exist in any built payload yet (design only, E2E-CMD-SCHEMA.md). Console results
remain [TARGET] until the controller run with the re-pinned config-entry revision.


OVERALL: PASS

