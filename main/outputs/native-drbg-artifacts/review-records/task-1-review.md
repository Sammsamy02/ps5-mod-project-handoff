❌ Spec compliance: Issues found. Task1 does not fully enforce its required rejection of extra source bytes in the identified cache closure. The gap is at [crypto_build.py:603](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:603), against [the plan’s closure requirement:48](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-09-30-native-drbg-plan.md:48).

⚠️ Cannot verify from this diff: Task2 consumer wiring, final linked native-host closure, PS5 execution, shared runtime budget/cleanup, and preserved HTTP behavior remain later integration gates, as assigned in [the plan:62](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-09-30-native-drbg-plan.md:62). The dependency’s host tests and native ELF objects establish Task1 evidence only.

### Strengths

- Source extraction validates physical TAR records, including hidden PAX payload/count bounds, before writing; paths, links, duplicates and file/parent collisions are rejected: [crypto_build.py:116](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:116), [test_crypto_build.py:103](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/tests/test_crypto_build.py:103).
- Library and consumer use the same overlay, with both NO switches and assertions after the public headers derive their settings: [crypto_config.h:5](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_config.h:5), [crypto_config.h:27](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_config.h:27), [crypto_build.py:455](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:455).
- The real-library test verifies no-sources failure with zero callbacks, then two 32-byte source polls at capacity 128; an aborting platform-poll stub stays uncalled: [test_crypto_build.py:203](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/tests/test_crypto_build.py:203), [green.txt:10](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/crypto/task1-evidence/green.txt:10).
- Anchored recompilation compares every object and the complete archive byte for byte in a separate transient directory and preserves cached evidence: [crypto_build.py:464](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:464). Genuine coordinated-object forgery RED/GREEN is retained at [coordinated-output-red.txt:9](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/crypto/task1-evidence/coordinated-output-red.txt:9) and [coordinated-output-green.txt:1](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/crypto/task1-evidence/coordinated-output-green.txt:1).
- Native tools/resources and SDK includes are pinned, archive members are checked against compiled objects, and source/framework license provenance is retained: [crypto_build.py:316](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:316), [crypto_build.py:352](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:352), [crypto_build.py:407](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:407), [crypto_build.py:676](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:676).

### Issues

#### Critical (Must Fix)

- None found.

#### Important (Should Fix)

- **[P2, plan-mandated closure requirement] Authenticate the complete source container.** At [crypto_build.py:603](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:603), the complete-cache check accepts whatever names the unsigned `manifest.file_closure` supplies. The independently anchored checks at [crypto_build.py:604](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/crypto_build.py:604) cover only `sources/mbedtls` and `sources/framework`. Adding `sources/unexpected.c` and its hash to `file_closure` therefore passes every check and the deterministic rebuild; the returned semantic identity and archive hash remain unchanged. This violates the explicit Task1 extra-source rejection requirement, while the linked archive remains byte-identical. Check all of `cache / 'sources'` against the union of anchored Mbed and framework members, prefixed respectively with `mbedtls/` and `framework/`. Add a coordinated extra-source/manifest regression alongside [test_crypto_build.py:261](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/tests/test_crypto_build.py:261); its present forged-extra loop covers config and objects only.

#### Minor (Nice to Have)

- None found.

### Checks

- Inspected the supplied diff in transport-sized segments after the initial tool response was truncated. No changed file was separately reread, no Git commands were run, and no broader codebase crawl occurred.
- Named outside-diff check: verify the recorded configuration, target closure, and current test/reuse evidence. Read the authoritative spec’s configuration/provenance sections, initial missing-builder RED, final 14-test GREEN, coordinated-output RED/GREEN, retained audit source/result, current descriptors, actual-command records, and named reuse receipts. [green.txt:17](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/crypto/task1-evidence/green.txt:17) records all 14 passing tests; [configuration-audit.json:2](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/crypto/task1-evidence/configuration-audit.json:2) records 81 x86-64 ET_REL native members and [configuration-audit.json:63](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/crypto/task1-evidence/configuration-audit.json:63) the four verified include origins. Host/native descriptors equal their reuse descriptors; their 88/94 fresh commands and 82-command reuse receipts have zero failures and empty stderr. No suite was rerun.
- Named focused doubt: forged source-container entries can authenticate themselves through the unsigned whole-cache closure. Executed exactly the command below in `/WORKSPACE/.codex/worktrees/translation-core/what`. It copied the already-built host cache into an automatically cleaned temporary directory, added one source file and its manifest hash, and called the real builder once. `PYTHONDONTWRITEBYTECODE=1` prevented import-cache writes. Original product files and generated caches were untouched.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 - <<'PY'
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile

root = Path.cwd()
product = root / 'outputs/ps5-mod-bridge/native-host'
spec = importlib.util.spec_from_file_location('review_crypto_build', product / 'crypto_build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
descriptor = json.loads((product / 'build/crypto/task1-evidence/host-green-descriptor.json').read_text())
original_cache = Path(descriptor['evidence_manifest']).parent
with tempfile.TemporaryDirectory(prefix='task1-review-source-closure-') as temporary:
    output = Path(temporary)
    cache = output / original_cache.name
    shutil.copytree(original_cache, cache)
    extra = cache / 'sources/unexpected.c'
    extra.write_text('/* Unauthorized extra source bytes, forged into unsigned closure. */\n')
    manifest_path = cache / 'manifest.json'
    manifest = json.loads(manifest_path.read_text())
    manifest['file_closure']['sources/unexpected.c'] = hashlib.sha256(extra.read_bytes()).hexdigest()
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
    spike = root / '.superpowers/native-dependency-spike'
    try:
        result = build.prepare_crypto('host', output,
            spike / 'mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz',
            spike / 'mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz')
    except ValueError as error:
        print('PASS: forged source-container addition rejected:', str(error))
    else:
        print(json.dumps({'forged_extra_source_accepted': True,
                          'identity_unchanged': result['identity'] == descriptor['identity'],
                          'archive_hash_unchanged': result['identity']['archive_sha256'] == descriptor['identity']['archive_sha256'],
                          'extra_source_still_present': extra.is_file()}))
        raise AssertionError('Reuse accepted sources/unexpected.c authenticated only by rewritten manifest.file_closure')
PY
```

- Focused command outcome: **exit 1**, with the following output after the full real reuse comparison accepted the forged closure:

```text
{"forged_extra_source_accepted": true, "identity_unchanged": true, "archive_hash_unchanged": true, "extra_source_still_present": true}
Traceback (most recent call last):
  File "<stdin>", line 37, in <module>
AssertionError: Reuse accepted sources/unexpected.c authenticated only by rewritten manifest.file_closure
```

### Assessment

**Task quality:** Needs fixes.

**Reasoning:** The selected crypto configuration, native include/tool closure, real no-default library behavior, and generated-object authenticity checks are well supported. The reproduced whole-source-container gap violates a binding Task1 requirement and should be fixed before dependency acceptance and Task2 integration.

