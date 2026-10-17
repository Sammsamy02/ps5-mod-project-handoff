# ELF package layout verifier

`package_binding.py` is read-only on artifacts and writes bounded metadata only. Use `--bare-original`, `--bare-sdk`, `--packaged`, `--out` (new file inside this directory). A future actual producer additionally needs `--producer` and its independently selected `--producer-sha`. Omit both for retained layout-only checks; never synthesize missing producer rows.

Twelve focused synthetic tests pass. TDD-RED.log preserves ten missing-utility failures; TDD-RED-FILE-OVERLAP.log and TDD-RED-EXEC-ONLY.log preserve two later regression failures. TDD-GREEN.log records the final12-test run. Initial retained replay refused an overly strict PF_R requirement; a source-header inspection identified genuine flags1 and0, and the tested repair accepts0..7 while refusing unknown bits. No unchanged unrelated suite was rerun.

RETAINED-LAYOUT.json binds the current l26-package-A/pass-1 artifacts: bare-original33a7c6283a5dd25f58fd37e7873dc4efbe22ecd492265494063669b077cc4afa; bare-sdkcf48356fbf948321398e3e5c04497929d70cd3623a1e250de079e8549e6ba73d; packageddc2d570cc5f6b551538093b2e4a85367fd8ce9255e446f9c19496a4071bf9d6b. These differ from historical symbol-input audit artifact identities; evidence must stay attached to this exact current triple. Initial stat bounds were checked before reads, then full source/artifact SHA and identity were rechecked. REPLAY-HASH-CHECK.json records those comparisons.

The executable PT_LOAD VMA0/flags1/filesz=memsz19497319/content SHA06fc92e59a184df053dad2bf6410f3a48c31177aebc99bbf63d4e3017fe738b7 is preserved original→SDK. The exact retained default wrapper reproduces all31987799 packaged bytes. This is ELF_LAYOUT_BOUND_ONLY; producer INPUT_REQUIRED, zero real symbols, runtime base convention and capture/build pairing false. A layout relation alone does not make crash addresses resolvable.

Next owner action is normal-build application/intentional rebase of the source-only producer, obtaining its genuine byte-bound receipt and authenticated source/assembly build record, then verifying that actual receipt with this binder. Separately establish runtime module-base/ELF-VMA convention and capture/build pairing before a crash sidecar adapter. No compiler/build/install, original writes, console, downloads or executable output occurred. Heavy binary slot released after the single repaired retained-artifact replay.

Replay synthetic suite only when warranted by changes:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/tmp" python3 test_package_binding.py
```

Independent review should inspect source pins/default profile, absence of SELF parsing, final stable source rechecks, declared-range vs authentic FUNC distinction, and the preserved failure/repair logs. Preserve all previous audit receipts; this folder does not update historical claims.
