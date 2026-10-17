# Task 2 handoff: bounded real-library startup DRBG checkpoint

Status: DONE. Implementation and the worker's required local verification are complete; all owned source changes remain unstaged. Root alone stages/commits and obtains fresh Task 2 and whole-phase review before any device action. No worker commit, deployment, device/manager/FTP/Nexus/account/game call, external fetch, or subagent was made.

Worktree: `/WORKSPACE/.codex/worktrees/translation-core/what`; supplied base: `65ad0bd7b662276b2a8f7e6cbd6019a78dc3becc`.

Authority: Task 2 brief and global constraints under `.superpowers/sdd/2026-09-30-native-drbg-plan/`, authoritative `outputs/2026-09-30-native-drbg-next-design.md`, and the root-approved current Task 1 fix1 report/descriptors. Initial Task 1 evidence is historical. This report claims local/host and emitted native verification, not PS5 execution or review approval.

## Implemented behavior

- `drbg.h` contains metadata only. Mbed contexts and bytes live in the private workspace defined by `drbg_internal.h`; its native branch includes the descriptor's generated `crypto_effective.h`. No workspace/array/pointer is retained in host_info.
- `mb_entropy_read32` is the shared strict adapter. It reserves one of four calls before invoking the sole sysctl callsite with MIB `{1,37}`, namelen 2, initial capacity 32, and null/zero new data. It clears errno immediately before and captures rc/errno/length immediately afterward, before reading the clock. Only rc 0 / errno 0 / length 32 succeeds. Every failure clears all 32 staging bytes; the compatibility observer uses a fresh budget and also clears successful raw bytes.
- The checkpoint shares one budget across its raw observation and registered source. It never uses the compatibility observer that resets the budget. Callback entries are counted independently from actual OS calls. Valid output capacities 32..128 receive exactly 32 bytes on success; failures clear the full valid capacity, including tail bytes, and latch. Invalid/null/unbounded storage is rejected before the OS call. Budget denial never invokes the OS, and keeps the last actual rc/errno/length/status separate from `budget_exhausted` policy metadata.
- Native startup observes raw once, then initializes one entropy/CTR-DRBG flow, adds one strong source with threshold 32, seeds once with the literal `modbridge-owned-drbg-checkpoint-v1`, default entropy 48 and nonce 0, and generates 32 bytes once only after successful seeding. The pinned SHA512 accumulator needs 64 strong bytes, so successful startup performs raw 1 + seed source polls 2 + generation source calls 0 = 3 actual API calls. Raw failure skips every Mbed operation. No fallback, retry, production reseed, extra extraction, or retained RNG was introduced.
- All returned paths free initialized Mbed contexts and then perform non-elidable writes across the full still-live owned workspace. Owned raw/source staging and generation output are covered. The fixture observer is guarded by `MB_DRBG_TESTING`, compares before cleanup only, and is absent from all production profiles. Production never inspects, prints, hashes, or persists generation/entropy bytes.
- Normal desktop startup links no Mbed archive and performs no target entropy/crypto operations. Raw and DRBG report unavailable/desktop_fixture, null operation values, and zero actual calls.
- Host startup caches only scalar raw/DRBG metadata. HTTP adds only `observations.drbg`, with the exact 17-field allowlist, nullable operation/completion/timing fields, scope `owned_startup_checkpoint`, request size 32, limit 4, and `hard_deadline_enforced:false`. The bounded `source_failure` object separately carries reason/policy and actual rc/errno/length. Reports never trigger another checkpoint. DRBG formatting is bounded at 1536 bytes, failure metadata at 320, and the whole body remains below 8192.
- Existing route/framing/Host/Origin/shutdown gates, CSP/page script, exact integer response text, cached core 7, firmware/game `not_queried`, and `installation_supported:false` are retained and tested.
- Native `build.py` requires explicit `--mbedtls-archive` and `--framework-archive`, calls current `prepare_crypto` before identity, and requires actual crypto identity for native profiles. All 20 compiled/builder/config inputs participate in SOURCE_INPUTS. All native consuming C units receive descriptor include/definitions/native options. Prospero wrapper SDK header priority and reviewed C++ flags remain intact. Only the crypto archive is linked. Native commands/macros/include closure/link map/imports/attribution are retained in manifests; embedded semantic build identity stays separate from final ELF SHA256.
- Fresh output roots under `build/drbg/` prevent overwriting Task 1 or historical raw-entropy artifacts. Mbed/framework LICENSE files are copied from the verified descriptor cache and hashed. SDK release provenance accurately records that this pinned SDK archive omits a standalone license file; root owns packaging of separately sourced historical notices. No public-distribution/license-compliance conclusion is made.

## Genuine TDD and independent reference evidence

Before product implementation, deterministic C/Python tests and the separate direct-public-API reference harness were written. Exact initial RED command:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_drbg.py -v > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/red.txt 2>&1
```

It ran 8 tests in 0.585s and failed 96 subcases with `bounded startup checkpoint implementation is missing`; the independently compiled upstream KAT already passed. Missing adapters/workspace were the expected reason for failure, not a simulated RED. The native identity prerequisite also failed before its product change:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_build.py -v -k requires_the_actual > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/identity-red.txt 2>&1
```

Result: 1 test, `ValueError not raised`. After implementation, the first full default-shape case passed (`focused-first.txt`, 1 test/0.796s), then the full focused command passed:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_drbg.py -v > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/green-focused.txt 2>&1
```

Result at that stage: 9 tests/1.348s, OK. An additional clock case is included in the final 10-case suite below. Initial RED and intervening evidence are preserved; final evidence supersedes their verification scope without rewriting them.

`tests/drbg_reference.c` uses only direct public Mbed APIs, never the owned adapter/source/checkpoint. It verifies the first pinned AES256/no-PR/no-add vector at `tests/suites/test_suite_ctr_drbg.data:1..3`: entropy 48, custom 16, nonce read 0, two random32 calls, expected second generation. This published KAT is separate from the production-shaped first32 regression fixture.

The one-time reference freeze used the approved Task 1 host archive/config, deterministic two32 polls (`0x40+i`, then `0x80+i`), exact production personalization, one default seed, and one first generation32. Its bytes were frozen into the fixture's constant; current tests never derive expected bytes dynamically from the function under test. The private `owned-reference.bin` is fixture-only, and neither its contents nor their hash were printed. Exact preparation run argv is retained in `reference-provenance.json`; `reference-run.json` reports only KAT/freeze success and two source calls. Source/config/archive provenance is closed there and in the current receipt. This establishes owned integration regression plus an independent library KAT, not entropy quality.

## Final tests and exact commands

All commands below ran from the worktree. Final affected discovery was run once after product stabilization, excluding unchanged Task 1 `test_crypto_build.py` by explicit root preservation/verification ruling. No test ROOT monkeypatch/symlink machinery was introduced. Task 1's separately approved current 15-test result remains authoritative; the worker does not claim it reran whole discovery.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 - <<'RUN' > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/final-affected-suite.txt 2>&1
from pathlib import Path
import unittest
root=Path('outputs/ps5-mod-bridge/native-host/tests')
patterns=sorted(p.name for p in root.glob('test_*.py') if p.name!='test_crypto_build.py')
print('Affected suite patterns:',', '.join(patterns),flush=True)
print('Excluded unchanged Task1 test_crypto_build.py per root preservation/verification ruling',flush=True)
suite=unittest.TestSuite(unittest.defaultTestLoader.discover(str(root),pattern=p) for p in patterns)
result=unittest.TextTestRunner(verbosity=2).run(suite)
raise SystemExit(not result.wasSuccessful())
RUN
```

Result: 48 tests in 24.839s, OK (build 8, core 6, DRBG 10, entropy 8, host 16). All seven core observations and actual desktop lifecycle/gates passed.

```sh
PYTHONDONTWRITEBYTECODE=1 MB_DRBG_SANITIZE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_drbg.py -v > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/sanitize-drbg-final.txt 2>&1
PYTHONDONTWRITEBYTECODE=1 MB_ENTROPY_SANITIZE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_entropy.py -v > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/sanitize-entropy-final.txt 2>&1
```

Results: DRBG 10 tests/19.190s OK; entropy 8 tests/2.065s OK, no ASan/UBSan diagnostics. `instrument_crypto_fixture` replays the approved host recipe in a fresh fixture tree with ASan/UBSan on all 81 real Mbed C units, and the adapters/caller are instrumented too. This is not an unsanitized library linked to a sanitized wrapper. Final sanitizer crypto manifest: `sanitizer-builds/host-2abml23w/manifest.json`, SHA256 `880b79d48085c6f282b814e6e86e562a7dcfdd4906ba702fa793a41536922c85`; instrumented archive SHA256 `be64fef668eec212c3e0b1cb3920183087308c4bcf1a44d74d07a6541af42689`. Manifest contains actual compile/archive argv and source/object/config hashes; prior sanitized evidence also remains.

The real-library matrix covers no-sources/reset with zero prohibited callbacks; separate extraction requesting128 and two32 polls; exactly one seed entropy request48; success and frozen output; raw skip; first/second poll nonzero/positive/nonstandard returns, errno0 errors, rc0 with errno, lengths0/all1..31/33/SIZE_MAX, and partial writes; callback capacities32/64/128 with tail/canaries/full wipe; invalid/null/insufficient/unbounded storage and pre-exhausted budget with no OS invocation; no later generation/retry/default source after rejection; actual first MD calloc failure after a successful source poll; add-source failure; real generation followed by injected partial-write/error cleanup; still-live entire workspace/canary cleanup; invalid/backward clocks preserving API metadata; and fixture-only reseed accepting the fourth OS call then denying the fifth before invocation, preserving/latching the last actual source result and separate policy metadata. Source callback failure is -60 and the real seed maps it to -52; classification survives separately. Allocation/generation injection scope and known vendor-local limits are explicit below.

Both native profiles were built once in one Python process to reuse full pinned tool verification; product artifacts were not rebuilt to repair checker assumptions:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 - <<'RUN' > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/native-builds-first.txt 2>&1
from pathlib import Path
import sys
root=Path('outputs/ps5-mod-bridge/native-host').resolve()
sys.path.insert(0,str(root))
import build
args=['--output-root',str(root/'build/drbg/task2-evidence/artifacts'),'--mbedtls-archive',str(Path('.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz').resolve()),'--framework-archive',str(Path('.superpowers/native-dependency-spike/mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz').resolve())]
build.main(['--profile','loopback',*args])
build.main(['--profile','development_lan','--ps5-address','ps5-host-not-configured.invalid',*args])
RUN
PYTHONDONTWRITEBYTECODE=1 MB_NATIVE_BUILD_ROOT=outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/artifacts python3 outputs/ps5-mod-bridge/native-host/tests/native_code_check.py > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/native-code-green.txt 2>&1
PYTHONDONTWRITEBYTECODE=1 python3 outputs/ps5-mod-bridge/native-host/build.py --profile desktop_fixture --output-root outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/artifacts > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/desktop-build.txt 2>&1
NODE_PATH=/WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules MB_DESKTOP_BINARY=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/artifacts/desktop_fixture/modbridge-native-host-desktop /WORKSPACE/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node outputs/ps5-mod-bridge/native-host/tests/browser_fixture.cjs > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/browser.txt 2>&1
```

Results: both native builds exit0; native checker 6 tests/1.102s OK; desktop build exit0; actual browser fixture exit0 (`CSP, report, keyboard shutdown, actual desktop exit; no device execution`). It used the available installed Chrome through the existing fixture fallback and bundled Node/Puppeteer, with no install/fetch. Actual nm and fixture-byte/observer absence evidence for desktop are saved in `desktop-symbols.txt` and `desktop-observer-absence.json`.

Self-review strengthened only the identity test's required literal list of 20 inputs after the final48 run, so omitted inputs cannot authorize themselves through SOURCE_INPUTS. All product/test sources in the final receipt include this strengthening. Its focused current verification (without another package rebuild) was:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_build.py -v -k identity_covers > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/identity-self-review-green.txt 2>&1
```

Result: 1 test/0.045s OK, mutating each required source plus relevant configuration/crypto identity/profile. The final48 log precedes this test-only assertion strengthening; this focused result covers the final change. No production changes followed the final native/desktop builds.

Current freeze/audit and whitespace verification:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/audit-current.py > outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/audit-current.txt 2>&1
git diff --check
```

Both exit0. Audit verifies all three exact current artifact file closures, source snapshots, fresh dependency source/config/member/archive closure, and 15,819 unchanged historical generated files against Task 1's retained before-file inventory. Old raw-entropy ELF hashes match their old manifests. Task 1 current source/descriptors/caches and all old SDK/dependency/artifact bytes remain unchanged. Worker never read or altered user certificate/key bytes and preserved all three user untracked files.

## Emitted native evidence and limitations

Optimized object/ELF checks find one sysctl callsite in the common adapter, budget reservation before it, exact32 compatibility wipe, callback bounds/staging success+failure wipe/full-capacity failure loop, add/seed/generate gating, raw wipe, both context frees, and entire workspace wipe. Actual native workspace extent `0x540` (1344) matches the wrapper's owned stack and full wipe loop. Observer symbols/callsite and expected fixture bytes are absent from both native profiles. The actual pinned Mbed zeroizer loads its volatile `memset_func` and calls indirectly; no invented `explicit_bzero`/`memset_s` claim is made. Actual entropy init registers no defaults and no platform/hardware/NV poll symbols are present. Native `mbedtls_aesni_has_support`, CPUID dispatch, and AESENC instructions are emitted; host arm64 success is not target AES execution.

Both native artifacts have BIND_NOW, three LOAD alignments 16384, and retained loader-prefix/ELF/program/section/load closure evidence. Prefix equals full file size 1,973,432; maximum LOAD file end is 1,442,416; section-table end is 1,973,432. The existing ordinary SDK CRT and RWE LOAD remain; this is not a freestanding W^X claim. Needed libraries are exactly `libkernel_web.sprx`, `libSceLibcInternal.sprx`, `libSceNet.sprx`. Complete import delta versus both historical raw-entropy profiles is 11 additions and no removals: `__isthreaded`, `__udivti3`, `ferror`, `fgets`, `ftell`, `printf`, `putchar`, `puts`, `rand`, `setbuf`, `time`. Retained whole-object imports do not prove runtime fallback use. New eager import resolution still requires root's actual native runtime proof.

Two initial native-checker setup failures are preserved (`native-code-first.txt`, relative path comparison versus actual absolute argv; `native-code-needed-first.txt`, stale expected .so names versus pinned SDK's actual .sprx names). Systematic inspection of actual manifest/NEEDED evidence identified checker assumptions; only checker expectations were corrected, and the unchanged current artifacts then passed. No unexplained failure or sanitizer diagnostic remains.

Known upstream cleanup limitations are unchanged and retained explicitly: `entropy_gather_internal`'s digest/update-error direct return bypasses its128-byte local wipe (the real MD allocation failure exercises this); the first CTR reseed entropy failure returns before its final384-byte seed wipe. Under this selected callback no partial seed is copied, the initial seed remains zero on that failure, and nonce length0 avoids the later nonce failure material path. Caller cleanup cannot erase expired library-local storage. The vendor/config bytes are unmodified and no comprehensive erasure claim is made. Real MD allocation failure is tested; generation cleanup uses successful real random followed by a fixture wrapper's injected error/partial write. Arbitrary digest/AES-internal failures are not claimed covered.

The syscall is synchronous with no hard deadline, thread, timeout escape, or retry. C interfaces require valid live distinct caller storage; pointer validity beyond bounded/null/capacity checks cannot be established by C. No TLS/DNS/CA/Nexus/login/download/game/install/retained production RNG support is implied. No device readiness, execution, entropy-quality, or shutdown proof exists here. Root must retain undifferentiated startup failure if its later single dispatch yields no report.

## Prerequisites and ownership

Fresh-checkout verification needs the real managed pinned source/framework archives, approved Task 1 current host/native descriptor/cache evidence, host cc/ar, Python3.9+, and macOS ARM64 capable of running the verified LLVM tools. Missing/corrupt inputs fail rather than skip/fetch. The native SDK/LLVM defaults selected by the recorded build are verified pinned paths, not source archive fallbacks:

```text
.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz
.superpowers/native-dependency-spike/mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz
outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/
outputs/ps5-mod-bridge/native-probe/build/deps/sdk.zip
outputs/ps5-mod-bridge/native-probe/build/deps/llvm.tar.xz
outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/
outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/host-green-descriptor.json
outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/native-green-descriptor.json
```

Mbed 3.6.7 commit `068ff080b369adfac81509f9b57b2afabaf82dc5`, archiveSHA `ca6bd316bbec49ef20088f39b8755fcaec0b7e45781506de55b17cd191ae6937`; framework `dde0c4a0e448a0552f18817dcea633bb851fd288`, archiveSHA `3b0d4864fa877dc9165b90ff3d5b68505c6176c0b22d317d99fe8c89a993e9fd`; SDKv0.43 commit `d9c9519116944a7f1c22d262012c53b80b3520b7`, SDKzipSHA `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`; LLVM21.1.8 ARM64 archiveSHA `b95bdd32a33a81ee4d40363aaeb26728a26783fcef26a4d80f65457433ea4669`. Actual compiler/header/archive argv and resolved path/hash evidence are retained in profile/dependency manifests.

Owned modified files (all relative to `outputs/ps5-mod-bridge/native-host/`): `Makefile`, `README.md`, `build.py`, `entropy.c`, `entropy.h`, `host.c`, `http.c`, `http.h`, `tests/browser_fixture.cjs`, `tests/build_fixture.py`, `tests/entropy_caller.c`, `tests/native_code_check.py`, `tests/test_build.py`, `tests/test_host.py`. Owned new files: `drbg.c`, `drbg.h`, `drbg_internal.h`, `tests/drbg_caller.c`, `tests/drbg_reference.c`, `tests/test_drbg.py`. `tests/test_entropy.py` remains unchanged: its conditional fixture wiring change was unnecessary and all8 existing cases pass through the revised helper/caller. The parent explicitly added existing browser_fixture.cjs to ownership for required actual-browser verification.

Root owns the separate visible unstaged `outputs/2026-09-30-native-drbg-plan.md` amendment and SDD plan/brief maintenance. This worker report and ignored fresh evidence are handoff records, not a stage/commit instruction for all untracked files. Preserve `work/nexus-test.crt`, `work/nexus-test.key`, and `work/nexus-types-pinned.ts`. `crypto_build.py`, `crypto_config.h`, and `tests/test_crypto_build.py` were not edited.

Self-review found no unresolved correctness doubt in the requested owned flow. File boundaries follow the plan; existing long README/history was updated locally without restructuring. Explicit root rulings resolve notice packaging scope, the unchanged Task 1 test exclusion, and browser fixture ownership. Root may now independently review exact owned diffs/evidence and choose integration; no product writes continue after this report.

## Current identities and exact artifact paths

The complete machine-readable freeze is [current-artifact-receipt.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/current-artifact-receipt.json). All evidence paths above are relative to [task2-evidence](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence) unless expressly stated.

Current config ID: `0983001ade634835e0755be7c0a0d7d82b7fb966ffe53e1bcbc19a1b3e77cae2`. The fresh native dependency has81 members and2214 identified files; both profiles consume identical approved archive SHA.

- **loopback**: [modbridge-native-host-loopback.elf](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/artifacts/loopback/modbridge-native-host-loopback.elf); size `1973432`; embedded build ID `ad8b1047d82095f28783dc9bcf732f49e89f5d0d5ac340b7e9f56bb119168934`; final artifactSHA `22a91e229891df40653820754c671d27e9d9c29fc4f004d7d99cb99b95d6b2d5`; manifestSHA `fca0f44ccb18f5acbad4b1688b02f8f1d087690a640b3fe04dc58425e8097287`.
- **development_lan**: [modbridge-native-host-development-lan.elf](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/artifacts/development_lan/modbridge-native-host-development-lan.elf); size `1973432`; embedded build ID `c2fc7809782db1db36b360835d3433a0b08342df2bee36000206e53a142fe1fb`; final artifactSHA `ebf47655cb7b5834b142344633855f3bffc2ba215f431e6e7ffcc530afe9e7d9`; manifestSHA `0198a578e7581fef4c0b781b01029c2676d205bb48c758ffe5aac342f3efece7`.
- **desktop_fixture**: [modbridge-native-host-desktop](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/drbg/task2-evidence/artifacts/desktop_fixture/modbridge-native-host-desktop); size `272352`; embedded build ID `16342d854dd77d8563f6b544916f6c058a695eef4f91888462397e4ed05dee2d`; final artifactSHA `54229628fa42fbc5fdc6c56d47582831843cd39631e879cf443e4ab828c5d114`; manifestSHA `f273311d783d721191ffdf7928353ef97ef13dd1940a006eeabc7eee8ccb594b`.

| Prerequisite | SHA256 |
|---|---|
| `crypto_build.py` | `583b64e6d65c9d70a5efbe3ecdb0cbff9e7474cc14311c2160e86723180b7739` |
| `crypto_config.h` | `4ddccf0ec7363b13fd0ef9a9dea4ec7636a7b5e8381f64cbbe014bc7217118b5` |
| `host-archive` | `9c49ece79fdd81a847844c607fffade75305cbc172a8d4a184aebe2ec2899bbd` |
| `host-green-descriptor.json` | `f7780d7b7a53f2c551d93ecbb7c47d62ce7a23ec71664cc0dc1ab807926f5156` |
| `native-archive` | `ae9e0cf32fdfd2fb8281b3b3518c076ba709186c98387a95912c16beb8b2bb53` |
| `native-green-descriptor.json` | `356e2aaea2485b82b6866383a12357b81b4d2e5fca6c372e8bd127bd620861b2` |
| `tests/test_crypto_build.py` | `6e76350db77424ed46382a42355d5fef05e282676d7350e6f2daa909a7fed8cc` |

| Current source (relative to native-host; test_entropy.py unchanged) | SHA256 |
|---|---|
| `Makefile` | `d03555b8195cb8c00dc123a74a9866f10fe61cdb62ff265a6e493f019a9e348c` |
| `README.md` | `0f0df2ce91a2cf7772eadc13ad35a1540adf1fe2ee31721618a0c744c5db85c4` |
| `build.py` | `398f6bbb59f98e6e99ab3a8d87ecb4b9158e4bcc48b3c778a7af5511996383c1` |
| `drbg.c` | `2f64b11d12cd6ac2ff4b6b04e2855bf12e0cf05544ab32c48e96351405c50025` |
| `drbg.h` | `b62e49fbe06abc273dcd730e7442743e8491b34b8473573ef6bc3ea6eb273003` |
| `drbg_internal.h` | `e7839cc5a88fc8aa41e46872e162f0d2a9d9bcbbc104a1cd0dcf0ae76d5c7b74` |
| `entropy.c` | `2bad4b8ec7f041ba94baa463e706345375231e80b761a39a9979781781f9066d` |
| `entropy.h` | `da3285fc6dbb14c676dfe970c9194b30c17c9f769bb82f5d67cfd6c75dd7fce2` |
| `host.c` | `2a13ba2c084755684b8774f29abd6dda6e97b69a68f4d9c3d24de92f871b25c4` |
| `http.c` | `e5fc94d99003b569de19b40a2af4557dde5543e4c9245f0025bb364a408c9dde` |
| `http.h` | `218a7753bc32cc9c21c867746949278e016f86bf32f2a45f44213e09be1c05a3` |
| `tests/browser_fixture.cjs` | `8a43e806d3bdf8da6204210443abbae7dd03e1beb0033c49f9213502d6f22bb0` |
| `tests/build_fixture.py` | `307e70f8aeaad3e8aaae07a947807efe49ae1f5f340d3718e118b2e6fcdaf1b2` |
| `tests/drbg_caller.c` | `760417feab4f1239ce1dd7508bdb1d3818263b3d087cd3a2acaf9d3dc2423f07` |
| `tests/drbg_reference.c` | `91108742342b859aabaf5ca0fc757d0046b13329dbc78cf664615930208d4dd8` |
| `tests/entropy_caller.c` | `53afdd07ede34852ed1872988476d60e0e2263953fb43ab7b254ffbb1abc9739` |
| `tests/native_code_check.py` | `66d5aaf2b27edd46945a58c9c0201683c1bcacf2bc9e02e1d3a2e937fd1c9dc1` |
| `tests/test_build.py` | `299344af1b5e4ca9d7502f38b8ee91c803b0bde79ac78b5131b85ed6437751a9` |
| `tests/test_drbg.py` | `21cdfd5c50916fb6cd55e401758974576c1d51f24868e35081275355237fbee9` |
| `tests/test_entropy.py` | `c053d429dd2490f2e8742d6d2088aa09a5d981f8b4045fc58369f17b5f9100c7` |
| `tests/test_host.py` | `2cce30404a1fd353c4863eb24abc956c94cabdd62db9cfd1efa734d8e180146e` |

Both native copied notices: MbedSHA `9b405ef4c89342f5eae1dd828882f931747f71001cfba7d114801039b52ad09b`, frameworkSHA `11402351e38392230bb8934ba1095c0c0049a296c0f8821f76e4672dff54b490`. The receipt closes actual profile outputs and fresh dependency input/member/include provenance. It deliberately contains no production or fixture entropy/generation byte digest.
