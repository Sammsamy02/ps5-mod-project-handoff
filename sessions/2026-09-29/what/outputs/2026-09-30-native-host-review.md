# Independent ordinary SDK native-host review

Reviewed `0b34b1b0940ebd9c882d09378cd4f1b60beeeb76..70fe22f7b39035102398aabc79f949493ebaeb8e`, its complete review package, native-host design and plan. Review covered application C, builder, tests, retained manifests/imports/readelf output and the unchanged cached SDK wrapper. No product files, index, commits or console/device state were changed. Independent scratch fixtures were confined to ignored `native-host/build/review` or temporary directories underneath `build`.

## Finding

**P2 — Reject program headers outside the section-derived transmission prefix.** `outputs/ps5-mod-bridge/native-host/build.py:119` calculates the loader prefix from the section table and file-backed section ends; line 120 checks PT_LOAD file ends against it. Lines 104–105 only bound the program-header table against the full file. The program-header table can therefore lie outside the transmitted prefix while the verifier accepts the artifact. A loader needs that table to interpret its loads. Reject a candidate when its ELF header or full program/section table exceeds the actual section-derived send extent; do not silently extend the reported prefix beyond the launcher's contract.

Independent reproduction: read the retained loopback ELF; append an exact copy of its four 56-byte program headers; replace the ELF `e_phoff` at offset 32 with the old file length; pass the temporary file to `elf_layout`.

```text
MALFORMED ACCEPTED: program_table_end 112568 loader_prefix 112344 file_size 112568
```

The current two reviewed artifacts are not affected: their program table ends at 288, section table ends at 112344 and actual prefix is 112344. This is a verifier gap requiring a regression and scoped re-review before treating future builder results as sufficient launch validation. No other actionable findings were found in the bounded application scope.

## Independent verification

```text
$ make -C outputs/ps5-mod-bridge/native-host test
Ran 15 tests in 3.553s
OK
```

The actual desktop C server was compiled with C11, `-Wall -Wextra -Werror`. The suite exercised fixed routes, actual imported observations, exact Host/framing checks, Origin/custom-header shutdown gates, fragmented/abandoned/broken/oversized peers, deadlines, real subprocess exit, port reuse and bind failure preserving an existing listener. Builder tests checked invalid explicit profiles and complete archive validation before writing.

Additional independent requests against that actual desktop subprocess checked duplicate case-varied shutdown control headers, Transfer-Encoding identity, `Origin: null`, an origin with a trailing slash, bare LF/CR inside a header value, and same-buffer pipelining. All seven returned bounded JSON rejection without terminating the host. A separate one-second process lifetime with an active incomplete peer exited normally with status 0 after 1.003 seconds.

```text
$ python3 outputs/ps5-mod-bridge/native-host/build/review/independent_http_review.py
7 independent malformed shutdown/control/origin/pipeline fixtures rejected with bounded JSON; process survives
Active incomplete peer lifetime clamp: subprocess exit 0 after 1.003 s
```

Static review confirms fixed 4096-byte request storage, absolute connection deadline, the documented final 100ms response reserve, checked short writes and retry handling, nonblocking bounded drain, native `_exit` after owned socket cleanup, and no process ownership inference/injection/kill path. Host validation applies to all routes. Shutdown accepts no body or Transfer-Encoding and requires exact bound authority, explicit zero length, custom control header and matching Origin when present. The origin gate is not arbitrary LAN-client authentication. Default is loopback; the separate explicit-address development profile expires after 600 monotonic seconds. Reports retain `installation_supported:false`, unavailable firmware/game facts and distinct desktop/native origins.

The source adds no kernel/exploit/privilege operations or SDK changes. The existing SDK CRT's loader-supplied privileged initialization and ordinary RWE startup load are acknowledged, not represented as a freestanding or W^X artifact.

## Pins and artifact evidence

Independently recomputed all **11** entries in `checksums.json`: every entry matched. There are **12** files in the artifact directory because `checksums.json` itself is intentionally outside that hash map. The preliminary reviewer message's count of 12 checksummed entries was a reporting-label error; actual verification iterated the 11 recorded entries.

- SDK release v0.43 commit: `d9c9519116944a7f1c22d262012c53b80b3520b7`.
- Actual SDK archive: 8,814,217 bytes; SHA-256 `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`, independently matched. Cached extraction's declared members matched the pinned archive.
- All five recorded LLVM 21.1.8 executable pins matched independently: clang, ld.lld, llvm-config, llvm-readelf and llvm-nm. Exact expected values are retained in each manifest and `build.py`.
- Loopback ELF SHA-256: `7ff2279bd6c0492bea176de09ec1b5d603c2c854c0724003d3b44796d75b173b`; source/profile build_id: `b0cf2edc26e438c690144e84b59b78f6fa18cd2d215287694d93c24eead6e721`.
- Development ELF SHA-256: `c0f81706fd35f49b791761e559ab37f7605c956d3ef08d5075bbf5793d72c3ef`; source/profile build_id: `5ec5ce00254778980377773f607ef533b03a66d772a282ede68cd45b4c1f308b`.

Both artifacts are 112,344-byte ELF64 little-endian x86-64 ET_DYN files with entry 0. Actual PT_LOAD flags are 7/6/6 (RWE/RW/RW), each aligned to 16,384 bytes. Maximum load file end is 98,332; full section-table/send extent is 112,344; all current headers/tables/file-backed loads fit it. Recomputed source hashes, canonical source/profile build_id and actual ELF layout matched both retained manifests. Build identity is distinct from the external artifact digest.

The retained local CSP snapshot explicitly has `browser_checked:false`. Root separately reports a passing real Chrome CSP/report/keyboard POST shutdown/subprocess-exit fixture and broader 97 C++/96 Python tests; this reviewer did not rerun those broader/browser checks. Neither those host results nor these static ELF/import checks prove PS5 entry, imported socket/clock behavior, termination, Nexus account/network readiness, texture output or game installation. No console launch/contact occurred in this review. Resolve and re-review the verifier finding before the controller's narrow owned-artifact console checkpoint.

## Scoped re-review — `70fe22f..08fdbf7`

The original P2 is **resolved**. Reviewed only the scoped verifier regression/fix, refreshed artifacts/manifests/identity receipt and implementation-report changes. The verifier requires canonical 64-byte ELF header size, computes full program/section table ends, checks all header/table extents against the file and the unchanged section-derived send prefix, and explicitly rejects an out-of-prefix table instead of enlarging transmission. Application HTTP/lifecycle C and SDK runtime are unchanged. No new actionable findings were found in this fix range.

Independent focused verification:

```text
$ python3 -m unittest discover -s tests -p 'test_build.py' -v
Ran 4 tests in 0.012s
OK
```

Repeated the original independent mutation against the refreshed real loopback ELF: the appended table ending at 112568, outside prefix 112344, now raises `ELF header or table exceeds loader prefix`. A separate header-size mutation from 64 to 63 raises `unsupported ELF tables`. Both valid retained ELFs still pass with checked extents `(ELF header 64, program table 288, section table 112344)` inside prefix 112344 and unchanged loads/16KiB alignment.

Recomputed all 11 recorded checksums (checksums.json is not self-hashed), both retained native layouts, current source/profile identities for all three profiles and actual artifact digests. Every value matched. Current reviewed artifacts replace the historical values above:

| Profile | Source/profile build ID | External artifact SHA-256 |
| --- | --- | --- |
| Loopback | `fc98b7cad1fa318553eb51b782fcec9c090d99607b2d508aaf491977ae7ae82d` | `d37ecbf7e4f27aca72e45e0822515ae4adedb66c6c365017379767c373a048f7` |
| Development LAN | `0f011382a9e320b9eef14af9b45f7506a8b07705f98e4e12a92f333f9fdb0391` | `e15b97b8a43154325f180d33354bc0c2ef65f8ab77f6b5f9a26e6794b4aae778` |
| Desktop fixture | `a15d87ff6199a892c2c42d9c310d3e80874863306412768f4a4d47cecd5af11a` | `67ae35f910da44bcb9ce0afec8834af7679fcb1f11849fca1451d96d5eba226d` |

The implementation report records the regression RED, subsequent 16 focused tests and all three rebuilds. This scoped reviewer reran the four builder tests and independent malformed/current-artifact checks, leaving unchanged HTTP tests and the new desktop browser fixture to the controller's existing checkpoint. No product/index/device changes or console contact occurred. There is no remaining independent-review blocker to the controller's narrow ordinary owned-artifact console validation after its browser checkpoint; static/desktop evidence still does not prove native execution or installation capability.
