# REPORT — worker-lease-coverage-fix (2026-10-05)

Honest summary, limits, and what the controller must verify before rebuild.

## What was delivered (this dir)

| file | what |
|---|---|
| `runtime_start_bridge_r14.cpp` | Full patched bridge: r13 + minimal lease-coverage diff (L1-L5). sha256 `03a9acb1d1b4df67…` (full value in fixtures/PASS.json). r13 untouched. |
| `ANALYSIS.md` | Throw-site mechanics, the exact lease ranges (base-containing records only — NOT the whole extent), ranked root cause, consumer audit, design rationale, DIFF summary. |
| `r13_to_r14.diff` | Unified diff (441 lines, mostly comments + 5 new static helpers + 3 wiring points). |
| `CLAIM.json` | C1-C10 provenance: every source fact carries file:line. |
| `fixtures/` | `test_lease_coverage.cpp` (production-TU harness), `run_tests.sh`, `PASS.json` (70 checks / 0 failed / exit 0), `run_all.log`, binaries+objects from the run. |

## Root cause, one paragraph

The launch-8 `code=2 "Owned read range includes unregistered bytes"` throw
(OwnedMemory.cpp:122) is a **two-snapshot race**, not an alignment bug: r13
registers readable spans from a fresh pre-capture sysctl while the lease ranges
derive from the capture's own frozen snapshot (anchor_mappings = records
containing base 0x400000, filtered prot&1). The only record containing the
base is the LOAD0 text record; exec-only at registration but readable at
capture → the lease walks unregistered text and throws at its first byte
(0x400000). The shipped r13 passed registration in launch-9 (spans=6/reg=6)
and its terminal failure was the honest code=1 geometry refusal (empty
derivation when the base record is exec-only at capture). Cap (6 spans) and
prot-bit mismatches are eliminated as launch-9 causes. r14 removes the race by
construction (registration from the capture's frozen array inside the
scan_image_base wrapper), registers true protection (R|X, never W; exec/gap
pages never), gates the anchor reads on the registry, and refuses any
uncoverable derivable lease range LOUDLY before any lease exists
(MRS_ERR_CONVERSION + bounded gap report through the existing channels).

## Proof summary

- Launch-8 throw reproduced in the OLD build through the REAL production ReadLease
  (`drift`, `capgate` scenarios) with the exact detail text.
- NEW build: same drifted kernel state → lease passes; gap case → loud
  pre-lease refusal naming `[0x400000 +0x787c000)` and the nearest span.
- Coverage ⊇ walk-set proven for: single RX text PT_LOAD, R rodata (unaligned
  end, r11 partial-page kept), RW data, observed RW fragment, NOREAD tail
  (never registered, leases into it throw), >128 fragments, non-aligned extent
  end, whole-extent consumer read attempt (refused).
- 70/70 checks green, runner exit 0 (fixtures/PASS.json).

## Limits (honest)

1. The T1→T2 protection flip is the only self-consistent explanation of the
   observed counters + throw under the shipped code, but the logs cannot
   distinguish a kernel-side prot flip from a record replacement. r14 is
   correct under either mechanism because it eliminates the second observation.
2. The fixture's adapter-derivation replica mirrors
   lmf_to_native_loader_facts.cpp:77-109 + live_module_facts.cpp:449-457
   semantics; the REAL adapter/snapshot/OwnedMappedImageCaller units were NOT
   recompiled (no engine rebuilds in scope). The chain was exercised through
   recording mocks + the real PLH service, same recipe as host_test_r13.
3. Host `-Werror` cannot gate this TU (pre-existing `dllimport` warnings come
   from the overlay headers on Apple clang); r14's own code compiles with
   `-Wall -Wextra` at **zero attributed warnings**, and the full suite compiles
   the TU cleanly. The authoritative gate remains the controller's prospero
   `-Werror` pass + import-set comparison.
4. The exact extent constant (0xbc602c8 shipped everywhere vs the 0xfc602c8
   figure in one relay/comment) is flagged in CLAIM C9; r14 does not depend on
   which is true because both sides share the same constant from the wrapper.
5. No console/network access; all in-game numbers quoted from
   outputs/2026-10-04-zcode-controller/launch9-evidence/ and the controller
   relays.

## What the controller must verify before rebuild

1. **Compile gate**: run the standard dry-build argv (prospero clang++,
   `-std=c++20 -Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector
   -fvisibility=hidden -ffreestanding -fno-builtin`, LLVM-21) on
   `runtime_start_bridge_r14.cpp` against a canonical vintage overlay
   (`worker-bridge-r13-polish/staging/lineage-overlay-2-r13` worked here on
   host), plus the import-set comparison vs r13's object — r14 adds NO new
   undefined symbols (only pre-linked `PLH::OwnedMemory::ScopedRegion`,
   `diagnostic_event`; printf family deliberately NOT used).
2. **Bump the pins**: comp6/launch-10 build drivers' `BRIDGE_DEFAULT` → r14
   path + measured sha (fixtures/PASS.json `new_sha256`); keep the
   `runtime_start_bridge.h` pin (header unchanged).
3. **Re-run the host suite** after any rebase:
   `fixtures/run_tests.sh` (exit 0 required; ~1 min).
4. **Consumer follow-up (separate worker, composed units — NOT in this TU)**:
   the staged UnrealInitializer seed path reads game memory via direct
   volatile reads (UnrealInitializer.cpp:678-688, used at :700-706, :715-722,
   :775-776, :798) outside the lease discipline; code-slot seeds (FName ctor
   0x63d590, static_construct 0x890aa0) sit in exec-only LOAD0 pages that must
   never be registered or read. Route those reads through a registry-gated
   check with the same bounded refusal reporting (pattern:
   `bridge_report_lease_refusal` + `bridge_anchor_owned_covered`).
5. **Log-reader note**: new diagnostic tags (all bounded, existing channel):
   `bridge_lease_cap`, `bridge_lease_order`, `bridge_lease_gap`,
   `bridge_lease_gap_req`, `bridge_lease_ref_lo/hi/len`,
   `bridge_lease_near_lo/hi/len`, `bridge_anchor_refuse`, `anchor_owned_ref`.
   `bridge_lease_reg_spans`/`bridge_lease_reg` keep their meaning (now
   frozen-snapshot basis; reg is emitted as g_image_region_count).
6. **Expected behavior change in-game**: when the base record is exec-only at
   capture, the start still refuses code=1 with the geometry text (unchanged);
   if a derivable range ever misses registration, the start now refuses
   code=1 with `lease coverage gap: …` INSTEAD of a deep code=2 PLH throw.
   No success path changes for the observed launch-9 geometry.
