# GREEN — host verification of r3 data-window anchor logic (worker-anchor-host-test)

Verdict: **ALL PASS — no launch blocker found.** r3's `window_anchor_verify`
+ `ops.scan_image_base` wrapper behave correctly on every fixture, including
all refusal and edge cases, and the embedded window bytes are byte-identical
to the retained eboot.

- Bridge under test: `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r3.cpp`
  sha256 `8a8c596101c0b2cfe697a3045d1fbfd337bb8bf440b0c39e40a73e42c64ed4a8` (re-verified at test time)
- Logic extracted VERBATIM by sed (never retyped):
  - `frag_anchor.inc` = r3 lines 113-156 (DataWindowAnchor, kWindowE, kWindowA,
    kDataWindowAnchors, kCandidateBase, kImageExtentVaddr, kVmProtRead,
    window_anchor_verify) — md5 71aae990c5b7ff0a29a9997962016d0f
  - `frag_lambda.inc` = r3 lines 219-241 (ops.scan_image_base wrapper lambda)
    — md5 5ead9e8f34ee9387ec3429ddb0d59c64
  (r3 lines 217-218 — `ops = base_ops; ops.context = &base_ops;` — are
  reproduced verbatim in `build_wrapper_ops()`; base_ops is the caller's
  long-lived object exactly as in r3's start function.)
- Types: real lmf headers (`/tmp/comp5_inc/live_module_os_ops.hpp` +
  live_module_base_scan.hpp + sce_module_abi.hpp). Mocked: ops table
  (query_vmentry / base scan_image_base) + process memory. The logic TU is untouched.

## Test method (why the evidence is strong)

This macOS host denies MAP_FIXED at guest-real addresses entirely, so raw
dereferences in the verbatim logic are served by a SIGSEGV/SIGBUS handler that
decodes the faulting LDRB (the only instruction a `p[i]` compare can emit;
emits 0x39400000-family and 0x38600800-family per probe, anything else aborts)
and returns the byte from a shadow range table. Consequences:
- every read the logic performs is logged (address+value) — cases assert
  exact read counts and that no read ever leaves the span-validated window;
- any out-of-bounds or unservable fault fails the case (read budget watchdog).

## Fixture results (see RECEIPT-run.txt for the full log)

| case | setup | result |
|---|---|---|
| a | both windows in one big readable span (prot=1) | rc=0; out={idx=0, base=0x400000, extent=0xbc602c8, count=1, trunc=0}; reads=100 (64 E + 36 A), 0 OOB |
| b | one byte flipped in E (byte 17) | rc!=0 REFUSED after exactly 18 reads (stops at mismatch), no read of A, 0 OOB |
| c | E readable-covered; A's span prot=4 (exec) | rc!=0 REFUSED; reads=64 (E only) — ZERO reads attempted of A |
| d | query_vmentry returns 5 | rc!=0 REFUSED; zero reads |
| e | zero mappings (total=0) | rc!=0 REFUSED; zero reads |
| f1 | E end == span end exactly | rc=0 success; highest E read is the last byte (e_end-1), nothing beyond the span edge |
| f2 | span ends one byte short of E end | rc!=0 REFUSED; zero reads |
| g | base header scan anchors (idx=1, base=0x3000000) | rc=0 passthrough UNCHANGED; window verify NOT run (query_vmentry calls=0, zero shadow reads) |
| h | E span prot=3 (rw), A span prot=5 (r-x) | rc=0 success (bit0 readable test) |
| i (extra) | vmentry TOTAL=257 > 256 capacity | clamp path (`count = min(total,256)`) still succeeds, 100 reads |

`ALL CASES PASS (0 failures)`, exit 0.

## Embedded-bytes verification (task 3): PASS

- decrypted-eboot.bin sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`
  matches EXPECTED-VALUES-OFFLINE.json retained_capture.
- r3 source-extracted `kWindowE` (64B) == JSON candidate E expected_hex ==
  eboot bytes at file offset 0x7dd6a40; unique in the whole 194MB image
  (1 occurrence, as recorded).
- r3 source-extracted `kWindowA` (36B) == JSON candidate A expected_hex ==
  eboot bytes at file offset 0xb93b250; unique in the whole image (1 occurrence).
- Informational: r3 `kImageExtentVaddr = 0xbc602c8` equals exactly
  max(p_vaddr+p_memsz) of the recorded phdr table (idx 8 end 0xbc602c8). Consistent.

## Notes (non-blocking, for the controller)

1. The task brief's literal happy-path span start `0x8018c000` lies ABOVE
   window E's runtime address 0x400000+0x7dd2a40=0x81d2a40. With that literal
   span the anchor REFUSES with zero reads — which is the correct,
   fail-safe behavior (that refusal was itself verified). The host fixture uses
   span start 0x818c000 (genuinely below E) to realize the intended happy path.
   On console this is moot: the shipped constants produce rt=0x81d2a40/0xc05f250
   and the real VMMAP spans cover them.
2. The wrapper passes `nullptr` as ctx to `base.scan_image_base(...)` and
   `window_anchor_verify` passes nullptr to `base.query_vmentry(...)` (r3 lines
   227/232/137). The real SCE ops are ctx-independent (factory records ctx but
   query_vmentry is a sysctl-for-current-pid call per live_module_os_ops.hpp),
   and `make_sce_live_module_os_ops(nullptr)` is what r3 calls, so this is
   consistent; recorded here because it relies on that SCE-TU property.
3. On wrapper success it emits chosen_index=0/candidate_count=1 with no
   candidates array — semantics differ from lmf_scan_image_bases usage, but
   this matches the wrapper's own contract (VmScanResult only) and case g/h
   show the result is well-formed for the passthrough and anchor routes.

Receipts: RECEIPT-run.txt (env, fragment md5s, TU sha, full run log), CLAIM.json
(first write), test_anchor_host.cpp, frag_anchor.inc, frag_lambda.inc, REPORT.md.
