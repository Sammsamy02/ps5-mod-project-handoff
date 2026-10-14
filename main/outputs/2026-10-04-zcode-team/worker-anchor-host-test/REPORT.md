# REPORT — worker-anchor-host-test (2026-10-04)

## Task
Host-verify, BEFORE the imminent console launch, the new data-window anchor
logic in `outputs/2026-10-04-zcode-team/worker-module-start-wiring/runtime_start_bridge_r3.cpp`
(sha256 8a8c5961...): the `window_anchor_verify` function and the
`ops.scan_image_base` wrapper lambda, plus byte-verification of the embedded
E/A window bytes against the retained eboot.

## Verdict
**GREEN — all fixture cases pass, no launch blocker.** See GREEN.md for the
full receipt; raw run log in RECEIPT-run.txt.

## How the logic was tested verbatim
- `frag_anchor.inc` = sed -n '113,156p' of r3 (constants + window_anchor_verify);
  `frag_lambda.inc` = sed -n '219,24p' ... precisely r3 lines 219-241 (lambda).
  The test TU `#include`s these fragments; nothing under test was retyped or
  edited (md5s recorded in GREEN.md/RECEIPT-run.txt).
- Real lmf types from `/tmp/comp5_inc` (live_module_os_ops.hpp,
  live_module_base_scan.hpp, sce_module_abi.hpp) — only the ops table
  (mock query_vmentry + mock base scan_image_base) and process memory are fake.
- Memory model: this Mac denies MAP_FIXED at every guest-real address
  (0x100000..0x100000000 all return ENOMEM, arm64 AND Rosetta x86_64, sandboxed
  and unsandboxed; no docker/podman/qemu available). Therefore raw dereferences
  in the verbatim logic are served by a SIGSEGV/SIGBUS handler that decodes the
  faulting LDRB (probe-confirmed encodings; unknown forms abort loudly) and
  serves bytes from shadow ranges, logging every read. This yields exact
  read-count and no-out-of-bounds assertions that plain mmap fixtures cannot give.
- One harness bug during bring-up (not under test code): the builder initially
  pointed `ops.context` at a dead stack copy; fixed to alias the caller's
  long-lived base_ops exactly as r3 line 218 does.
- One fixture correction (documented in GREEN.md note 1): the brief's literal
  span start 0x8018c000 is above E's runtime address 0x81d2a40, so with it the
  correct behavior IS refusal (verified: refused, zero reads). The happy-path
  fixture uses start 0x818c000.

## Results
| case | expectation | outcome |
|---|---|---|
| a happy path | anchor succeeds, base 0x400000, extent 0xbc602c8 | PASS (100 reads = 64 E + 36 A, 0 OOB) |
| b byte flipped in E | REFUSED | PASS (stops after 18 reads; no A reads) |
| c A in exec span | REFUSED, no read of A | PASS (64 E reads only) |
| d query_vmentry fails | REFUSED | PASS (0 reads) |
| e zero mappings | REFUSED | PASS (0 reads) |
| f span edge == E end / one byte short | succeed / REFUSED | PASS both (no read past span edge) |
| g header scan anchors | passthrough unchanged, window verify not run | PASS (query_calls=0, result untouched) |
| h prot=3 / prot=5 | both readable (bit0) | PASS |
| i extra: TOTAL>capacity | clamp still correct | PASS |

Byte verification: r3-embedded E(64B)@p_vaddr 0x7dd2a40 and A(36B)@0xbc5f250
are byte-identical to EXPECTED-VALUES-OFFLINE.json candidates and to
decrypted-eboot.bin (sha 3b6cfad4... verified) at derived offsets 0x7dd6a40 /
0xb93b250; each unique in the 194MB image (1 occurrence). Extent constant
0xbc602c8 == max(p_vaddr+p_memsz) of the recorded phdr table.

## Findings for the controller (non-blocking)
1. Wrapper passes ctx=nullptr to base ops calls (r3 lines 227/232). Safe given
   the SCE TU is ctx-independent (sysctl current pid) and r3 constructs the ops
   with make_sce_live_module_os_ops(nullptr) — but it is an implicit coupling
   worth one comment in r4 if one ever ships.
2. Wrapper success emits chosen_index=0/candidate_count=1 without a candidates
   array — distinct from lmf_scan_image_bases semantics; downstream consumed
   fields (chosen_base/chosen_extent) are complete and correct.
3. Safety properties observed empirically: every refusal path reads ZERO bytes
   except the byte-compare path itself, which never reads outside a
   readable-covered window and never touches the second window after a first
   -window mismatch.

## Deliverables (this directory)
CLAIM.json (first), frag_anchor.inc, frag_lambda.inc, test_anchor_host.cpp,
test_anchor_host (+ .dSYM), RECEIPT-run.txt, GREEN.md, REPORT.md (this file, last).

## Constraints honored
No console contact; no nested subagents; writes only in
outputs/2026-10-04-zcode-team/worker-anchor-host-test/ (plus read-only access
to r3, worker-c2-probe JSON, retained eboot; /tmp probes); logic under test
byte-exact; no replay of other workers' work.
