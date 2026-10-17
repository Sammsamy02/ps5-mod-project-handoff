# Task 1 fix1 report: whole source-file closure

Status: fix1 implemented and verified, ready for root's scoped independent re-review. The full 15-test crypto suite passed, fresh host/native dependency archives built, both unchanged caches passed deterministic byte comparison, and all 15,819 historical generated files remained unchanged. This report does not confer approval; Task2 remains gated on root's fresh review.

Worktree: `/WORKSPACE/.codex/worktrees/translation-core/what`.

## Finding, minimal fix, and ownership

The reviewer demonstrated that `sources/unexpected.c` could be added to an otherwise valid host cache and authorized by adding only its hash to unsigned `manifest.file_closure`. The pinned Mbed and framework subtree checks missed their common parent's sibling file, and recompiling only the 81 pinned crypto units did not consume it. The new regression independently reproduced acceptance before production changes.

Fix1 replaces those two subtree checks with one exact `sources/` file-byte closure check against the union of freshly verified pinned archive members, prefixed by `mbedtls/` and `framework/`. It covers all 948 Mbed files and 1,091 framework files, rejects an unsigned manifest's added source bytes, and retains existing link/special-file rejection. Per root's explicit ruling, harmless empty-directory metadata remains tolerated; this fix does not broaden global tree semantics. Source bytes, consumer configuration, compiler flags, API/CLI contracts, and compiled crypto source list are unchanged.

Only two product files changed:

- `crypto_build.py`: the two subtree checks become one parent-directory check (716 total lines).
- `tests/test_crypto_build.py`: one real forged-manifest regression plus native output relocation to preserve initial evidence (379 total lines).

`crypto_config.h` is unchanged (47 lines). No staging, Git/index mutation, subagent, console, external network, account, deployment, or existing SDK/source-cache mutation was performed by the worker.

The original report was preserved byte-for-byte as `task-1-initial-report.md`, SHA256 `78a056dae2ec6e78ba974c34efeac1e8e9c2d7ba36d24f67ab836953b94190e5`. It retains the historical implementation and original RED/GREEN chronology, with the reviewed parent-source-closure gap now superseded by fix1. Original caches/descriptors/receipts under `build/crypto/` and `task1-evidence/` are historical and should not be consumed as current. All fix1 logs, descriptors, current-recipe artifacts, resources, and receipts are under `build/crypto/task1-fix1-evidence/`.

## Genuine RED/GREEN and exact commands

The new regression uses the real host builder/library. It adds `sources/unexpected.c`, updates only that file's manifest closure hash, leaves input/config/member/archive/semantic identity unchanged, and expects reuse rejection. Its temporary source and manifest changes are restored on every returned outcome.

Exact RED command:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_crypto_build.py -v -k whole_sources > outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/whole-sources-red.txt 2>&1
```

Actual RED output (`ValueError not raised`, not a setup failure):

```text
Ran 1 test in 23.155s

FAILED (failures=1)
```

After the minimal fix, the identical focused command redirected to `whole-sources-green.txt` returned exit 0:

```text
Ran 1 test in 11.397s

OK
```

Exact final focused suite:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_crypto_build.py -v > outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/green.txt 2>&1
```

Actual final output, exit 0:

```text
Ran 15 tests in 181.749s

OK
```

All prior crypto boundary/config/tamper/native/real-library tests also passed, including coordinated object/archive/manifest rewrites. No unrelated unchanged host/core/entropy suite was rerun per root's prior focused-verification ruling. `PYTHONDONTWRITEBYTECODE=1` avoids rewriting historical Python bytecode during this fix.

Fresh host command:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/ps5-mod-bridge/native-host/crypto_build.py --mode host --output-root outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts --mbedtls-archive .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz --framework-archive .superpowers/native-dependency-spike/mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz > outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/host-green-descriptor.json 2> outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/host-build-stderr.txt
```

Result: exit 0, empty stderr, complete current-recipe descriptor. Repeated the identical command with only stdout/stderr filenames changed to `host-reuse-descriptor.json` / `host-reuse-stderr.txt`: exit 0, empty stderr, descriptor identical, verified 82-command byte-comparison receipt.

The full-suite native positive test built a fresh current-recipe archive and saved `native-green-descriptor.json`. Exact independent native reuse CLI:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/ps5-mod-bridge/native-host/crypto_build.py --mode native --output-root outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts --mbedtls-archive .superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz --framework-archive .superpowers/native-dependency-spike/mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz --sdk outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk --llvm-bin outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin > outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/native-reuse-descriptor.json 2> outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/native-reuse-stderr.txt
```

Result: exit 0, empty stderr, descriptor identical to the fresh build, verified 82-command byte-comparison receipt.

Exact retained audit command:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/audit-current.py > outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/audit-output.txt 2>&1
```

Result: exit 0, `status: verified`, 81 members, native include origins exactly `llvm_resource`, `overlay`, `sdk`, `source`, and both reused descriptors identical. `configuration-audit.json` additionally confirms the full 2,039-file source union, current product hashes, original-report preservation, and all 15,819 pre-fix generated files unchanged. `historical-files-before.json` and `historical-files-after.json` retain each historical filename/hash/size; every recorded pair matches.

`git diff --check` also returned exit 0 with empty output. Self-review inspected the actual small product diff and final evidence; no production change followed final suite verification.

## Current authoritative identities

The recipe change updates configuration/dependency identity and selects fresh cache directories. The compiled archives themselves remain byte-identical to initial Task1 because this change affects cache validation only.

```text
config_id                 0983001ade634835e0755be7c0a0d7d82b7fb966ffe53e1bcbc19a1b3e77cae2
build recipe SHA256       583b64e6d65c9d70a5efbe3ecdb0cbff9e7474cc14311c2160e86723180b7739
owned overlay SHA256      4ddccf0ec7363b13fd0ef9a9dea4ec7636a7b5e8381f64cbbe014bc7217118b5
effective header SHA256   10eae7d0c9f355ba79127737a212e4834b086fa8cba78625d639961ea52bfca1
test source SHA256        6e76350db77424ed46382a42355d5fef05e282676d7350e6f2daa909a7fed8cc
```

| Current build | Host | Native |
|---|---|---|
| Cache under task1-fix1-evidence/artifacts | `host-a4768e3a50d32394616f27f8` | `native-038ce86825317d4d74716405` |
| Archive SHA256 | `9c49ece79fdd81a847844c607fffade75305cbc172a8d4a184aebe2ec2899bbd` | `ae9e0cf32fdfd2fb8281b3b3518c076ba709186c98387a95912c16beb8b2bb53` |
| Archive bytes | `726208` | `1172402` |
| Manifest SHA256 | `f197eeb8feff6c3f980dcf31abb52949cbe223202b3dc21ce42157c4175128c9` | `a7e4e10e2fa50f053ea44217e3023b0c2db9a4e77609a3449ee66f55e181bea9` |
| Canonical compact sorted identity SHA256 | `c42c69526ae5e766ab0fd34c7b7d3b506bd7dd6097b8ba822d7a19e84752dcbc` | `e7e3813ea60816a4506eb3be19631e7aa492f131ef7fddfa932fbccdc9c011d8` |
| Fresh actual commands | `88` | `94` |
| Identified file closure | `2211` | `2214` |
| Reuse commands | `82` | `82` |

The authoritative descriptors are `task1-fix1-evidence/host-green-descriptor.json` and `task1-fix1-evidence/native-green-descriptor.json`, with identical `*-reuse-descriptor.json` results. Their absolute archive/manifest/include paths identify the new current caches. Successful current reuse receipts are:

```text
artifacts/reuse-evidence/host-a4768e3a50d32394616f27f8-vq_moth7.json
artifacts/reuse-evidence/native-038ce86825317d4d74716405-4ka2ii0v.json
```

Each receipt retains 81 actual recompilation commands plus the actual archive command, status `verified`, and comparison `byte-for-byte every object and complete archive; no metadata normalization`. Rebuilds use a separate transient owned directory; no cached manifest/source/config/object/archive/evidence is overwritten before comparison. Generated objects, every member payload, member order/target identity, the complete archive, and per-TU dependency closure are checked; coordinated output/manifest claims cannot authenticate themselves. Transient directories are deleted, while receipts and historical caches are preserved.

Each current cache's `manifest.json` retains the full source/config/header/tool/object/member/archive closure. `evidence/commands.json` contains complete actual argv/stdout/stderr (88 host, 94 native commands); `*.o.d` and normalized `include_closure` cover all 81 TUs. Actual `effective-macros.txt`, `entropy.i`, `entropy_poll.i`, `ctr_drbg.i`, archive ELF headers and defined/undefined symbol evidence remain identified. Every native member is ELF64 little-endian x86_64 relocatable (`e_machine=62`, ET_REL), ordered exactly like the pinned 81-source crypto recipe; every payload equals its object. Host members are local arm64 MachO64. No TLS/X.509 archive is built.

## Unchanged API, consumer, configuration, and provenance contracts

```python
prepare_crypto(mode, output_root, source_archive, framework_archive, *, sdk=None, llvm_bin=None) -> dict
```

Returns `archive`, `include_dirs`, `compile_definitions`, `config_id`, stable JSON `identity`, `native_compile_options`, and `evidence_manifest`. Production archive paths remain explicit, with native SDK/LLVM paths also explicit. There is no hidden research-directory fallback or fetch. Absolute operational paths occur only in descriptor/evidence; semantic identity has no workspace paths/timestamps.

Every Mbed-consuming translation unit uses returned `include_dirs`, identical `MBEDTLS_USER_CONFIG_FILE="crypto_config.h"`, and includes generated `crypto_effective.h`. The effective header includes entropy/CTR public headers before derived assertions; it and its bytes/hash remain part of config identity/cache closure. Library TUs force-include unmodified upstream `library/common.h` first to establish private access, then the generated effective header. Metadata-only `drbg.h` need not include Mbed. The application consumer requires no library-private setting.

Host `native_compile_options` remains `[]`. Native Mbed-consuming C units append returned verified `-nostdinc -resource-dir <owned lib/clang/21> -isystem <owned lib/clang/21/include>` options to the existing verified prospero SDK C-header closure. Semantic normalized resource options and every selected resource-file identity participate in dependency identity. C++ include behavior remains outside this task.

Actual effective evidence and assertions retain both NO switches, legacy SHA512/AES modules, SHA512 accumulator/MD, block64, gather128, AES256 key32, default entropy48, nonce0, PR-off0, reseed10000. Derived `MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO` and `MBEDTLS_MD_SHA512_VIA_PSA` are absent. Default legacy `MBEDTLS_PSA_CRYPTO_C` remains present elsewhere. Alternate entropy/CTR/AES/SHA512, hardware/NV/null sources, forced SHA256, AES128-only, threading, and alternate/PSA CTR paths remain rejected. Actual entropy init registers no defaults; entropy_poll defines/references no platform poll. Actual entropy extraction needs at least 64 strong bytes, so the host threshold32/fixed32 fixture takes two polls; seed defaults entropy48/nonce0 and source callback failure maps to `-52`. The real host fixture's pre-registration request still returns NO_SOURCES with zero reader/source/prohibited callbacks, and its fail-fast platform poll stays uncalled; fixture bytes are not printed.

Pinned inputs remain verbatim:

| Input | Commit / archive SHA256 |
|---|---|
| Mbed TLS 3.6.7 | `068ff080b369adfac81509f9b57b2afabaf82dc5` / `ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937` |
| Framework | `dde0c4a0e448a0552f18817dcea633bb851fd288` / `3b0d4864fa877dc9165b90ff3d5b68505c6176c0b22d317d99fe8c89a993e9fd` |
| SDK v0.43 | `d9c9519116944a7f1c22d262012c53b80b3520b7` / `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb` |
| LLVM 21.1.8 macOS ARM64 package | `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669` |

All source/framework bytes remain unmodified exact copies, including both full LICENSE files and chosen upstream Apache-2.0 attribution from dual Apache-2.0 OR GPL-2.0-or-later, framework gitlink/archive provenance, and zero vendor mutations. Original SDK files match full pinned sdk.zip closure. Pinned clang/llvm-config/readelf/nm hashes and all 299 selected regular LLVM ar/resource/notice files remain in identity; llvm-ar SHA256 is `c2640fc0a747413fe27818531d4a22f979d91b7e299723a68225917c42574b50`. The original report retains the unchanged individual tool pin table. Selected LLVM bytes come exclusively from the full hash-verified LLVM archive; irrelevant links are validated inside the package and ignored under root's prior ruling, while selected tools/headers must be regular bytes. Source/framework extraction still validates all members before writes, rejects links/escapes/normalized duplicates/special entries, and bounds raw hidden PAX records, expansion, counts and sizes.

Actual native commands retain target `x86_64-sie-ps5`, SDK sysroot/isysroot, no-stack-protector/no-PLT/emulated-TLS, no default includes, explicit target SDK and verified LLVM resource headers, C11/Os/no-common, prefix-map determinism, and identical overlay/force-includes. The direct clang ABI macros still match pinned prospero wrapper: x86_64/ELF/FreeBSD9/LP64/pointer-long-size_t8/little-endian/SSE2. Full operational argv and probe outputs are in the current command evidence; full pinned source/header correspondence is independently checked before reuse.

## Prerequisites and remaining limits

The new regression and host integration tests require real managed Mbed/framework archives and usable host `cc`/`ar`; synthetic extraction tests create their own archives. Full-suite native integration additionally requires these managed inputs:

```text
.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz
.superpowers/native-dependency-spike/mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz
outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/
outputs/ps5-mod-bridge/native-probe/build/deps/sdk.zip
outputs/ps5-mod-bridge/native-probe/build/deps/llvm.tar.xz
outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/
```

Python 3.9+ and a macOS ARM64 environment capable of executing the pinned LLVM package are required for this full suite. Missing/corrupt integration inputs **fail** relevant tests with actual missing-file/archive/tool errors, without skip/fetch. A fresh checkout lacking these managed inputs cannot claim the full suite is green. Production's explicit archive/native path interface remains correct; test constants are not a production fallback. Root must supply/document those prerequisites for fresh-checkout verification.

Known upstream cleanup bypasses are unchanged: entropy_gather_internal's digest/update-error direct return bypasses its 128-byte local wipe; CTR reseed's first entropy failure returns before its final 384-byte-seed wipe. Under this selected entropy callback, no partial seed is copied on failure and its initial zero seed remains zero; nonce length is zero, so the later nonce-failure material path is not used. Caller cleanup cannot erase expired library-local storage. No vendor repair, comprehensive erasure, or new emitted-machine cleanup claim is made.

Full LLVM verification costs about 70 seconds per new Python process; deterministic reuse adds a full development rebuild of 81 objects. The final suite took 181.749 seconds. Finished PS5 runtime is unaffected; receipt retention remains development metadata. Host fixture tool/header evidence depends on the installed local toolchain; native uses the pinned verified tools/SDK/resources.

This fix establishes no PS5 execution, entropy-quality result, TLS/CA/login/download support, retained RNG policy, game/install support, or deadline guarantee. `installation_supported:false` and later integration/device gates remain unchanged. Root owns exact-path staging/commits and independent re-review; Task2 must not consume the current descriptor until that review approves the actual fix.
