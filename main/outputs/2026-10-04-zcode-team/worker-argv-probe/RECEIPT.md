# RECEIPT — worker-argv-probe (team 2026-10-04)

G1 ARGV probe payload `ARGV_ECHO.elf` — built, locally gated, deterministic.
**Never executed** (not on host — macOS arm64 cannot run prospero ELFs — and
never on console: this worker made no console or network contact; dispatch
and collection belong to the controller per `PROBE-PLAN.md`).

## Identity

| Field | Value |
|---|---|
| Payload | `build-1eb64d8964c6/ARGV_ECHO.elf` |
| build_id | `argv-1eb64d8964c6` |
| sha256 | `53075bd7cadbebbdfc11df03e08df740a57b81148a270c68652cc5dc81104e6f` |
| bytes | 111,016 |
| source | `src/main.c` (single file) |
| sink | `/data/modbridge-argv-probe.jsonl` (append, one bounded JSONL record per dispatch) |
| record | `{"marker":"modbridge.argvprobe","schema":1,"build_id":"argv-…","argc":N,"argv":[…],"argv_capped":b,"str_trunc":b}` |
| bounds | ≤64 argv entries, ≤64 B per string (escape-then-cap, flags set, never silent), argc recorded verbatim from `main()` |
| determinism | two independent clean compiles → byte-identical (`53075bd7…` both); no timestamps/randomness in the record |

## Format decision: ELF, not PRX

The task said "follow what prior argument-free payloads used for the
`/loadpayload` route". CONSOLE-LOG items 2–4, 8–9 show that route's payload
class: standalone SDK ELFs (`NEXT_DLL_THREADS.elf` 240,960 B build 6d295f56,
`TEST_THIS_DLL.elf` f72faea5, `INITONCE_DLL_TEST.elf` 241,784 B build
1a6b37b7 — all uploaded, readback-verified, dispatched once, PASSED). These
are built by the ps5-payload-sdk `prospero-clang` driver exactly like the
SDK's own `samples/hello_argv` (whose Makefile even demonstrates the
`?args=` deploy grammar). The v2 probe lineage's 7-verbatim-NID import
contract is a PRX/ploader (in-game module) artifact; importing those NID
identities into a standalone `/loadpayload` ELF is NOT what any
console-proven payload on this route did, so it was not done here. What v2
contributes and this build follows: entry record first (the JSONL write is
the payload's only action), bounded /data JSONL sink, receipted commands,
loud gates, two-clean-build determinism.

## Import proof (build-1eb64d8964c6/import-proof.txt, llvm-readelf)

Dynamic section: exactly 3 DT_NEEDED — `libkernel_web.sprx`,
`libSceLibcInternal.sprx`, `libSceNet.sprx` (the SDK default hosted link —
the same link class as the three console-proven diagnostics; libSceNet comes
from the SDK libc linkage, not from our code: no network symbols are
imported). `.dynsym` undefined: exactly 5 FUNC imports — `fopen`, `fprintf`,
`fputc`, `fwrite`, `fclose` (stdio only, matching the source; printf-family
inlined to fputc/fwrite). Forbidden set (`dlopen/dlsym/dlclose` family,
`sceKernelGetModule*` family): ABSENT — gate passed.

## Gates

- `check_argv_artifact.py`: PASSED (ELF magic, x86-64, EXEC/DYN, size sane,
  marker + sink strings embedded, twin-build byte identity, receipt sha
  match).
- Forbidden-symbol gate: PASSED.
- Determinism: PASSED after `-Wl,--build-id=none` (finding: lld's default
  GNU build-id hashes the OUTPUT PATH, so two clean builds into different
  directories differ in exactly those 16 bytes; build-id disabled — payload
  identity is the embedded `argv-…` build_id string).
- Known cosmetic: llvm-readelf warns `invalid PT_DYNAMIC size (0x1018)` on
  this SDK-linked ELF (same link class as the console-proven ELFs); recorded
  honestly, no claim of change.

## Bounds honored

≤25 tool calls (15 used); no nested subagents; no network, no console, no
game launch; writes confined to
`outputs/2026-10-04-zcode-team/worker-argv-probe/`; no dispatch performed —
`PROBE-PLAN.md` reserves all console actions for the controller, dispatch
bounded to ≤3 total (2 bare + ≤1 UI-args variant), never replaying a
collected request.

## Files

- `CLAIM.json` (filed first), `src/main.c`, `build_argv_echo.py`,
  `check_argv_artifact.py`
- `build-1eb64d8964c6/`: `ARGV_ECHO.elf`, `a/` + `determinism/` twin builds,
  `import-proof.txt`, `build-receipt.json`
- `PROBE-PLAN.md` (controller steps), `REPORT.md` (this worker's report)
