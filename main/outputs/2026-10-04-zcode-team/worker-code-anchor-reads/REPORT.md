# REPORT — worker-code-anchor-reads (igf5 code-anchor reads)

Worker: worker-code-anchor-reads, ZCode team 2026-10-04, 2026-10-04.
CLAIM.json first, this REPORT last. All writes under
`outputs/2026-10-04-zcode-team/worker-code-anchor-reads/`. ~50 tool calls
used (bound: 80). No network. No console (controller stages+launches). No
nested subagents. Existing files elsewhere read-only (vendored copies
byte-verified; Packager.dll used in place, never rebuilt; finalize_sdk.py /
make_fself.py used read-only).

## 1. Deliverable

`modbridge_ingame_facts.prx`, build id **igf5-4186048a6dd6** — the focused
probe that resolves the ONLY remaining unresolved check from the igf4
anchor validation: the 16 code-anchor bytes at Tick `0x33ac8c0` and SCO
`0x890aa0` (VMMAP-reported execute-only, prot=0x4), read under the
controller-ordered mprotect-FIRST sequence, compared against the
retained-image expected prologues; plus the GUA predicate replay with the
CORRECTED GC-order check (worker-predicate-gc-fix landed mid-task and the
build hook wired it in). Same module name, same log path, same 7-verbatim-
import packaging as the loaded igf4 lineage.

- fself 25981 B, sha256 `486845b7e121d5993febdf812ac55815cc974fc1e2a062ebb4ac60fc5b8f8dfa`
- inner ELF 85472 B, sha256 `427fbf2f90aec4982f7e73456d289a5ed5c150d1ae753cfd62377d510dc6f646`
- combined `1abadb41…`, contract `e3755524…`
- Determinism: run1 == run2 byte-identical (final PRX, inner ELF, combined, contract)
- Host checks: HOST_CHECKS_PASSED (+ artifact checks, LOUD gates passed)
- **Native execution: NONE. In-game results: NONE. No promotion claimed.**
- Staging: STAGE-PLAN-CODE-ANCHOR.md (same-file replacement of igf4, launch
  ritual unchanged) — **GATED on controller sign-off, see §3.**

## 2. Controller corrections applied (FULL ACKNOWLEDGMENT)

> CONTROLLER CORRECTION: "Fix igf5: mprotect BEFORE read, never raw-read
> exec-only … The current design says 'if the direct read faults, fall
> through to approach B (mprotect).' This is UNSAFE … REVISED APPROACH:
> STEP 1 SAFE PROBE (mprotect adds PROT_READ, keeps EXEC, on the specific
> 16 KiB page) → STEP 2 READ → STEP 3 RESTORE (verify) → STEP 4 COMPARE.
> NEVER dereference an address in a region whose VMMAP protection lacks
> READ unless mprotect added it first. Change only the specific 16 KiB
> pages. Preserve original permissions. Verify restoration; anomaly if it
> fails. If mprotect unavailable/fails → refusal record, NO read."

Applied exactly, in this order, per anchor (seq 4/5):

1. Current protection queried first (current-invocation sysctl KERN_PROC_VMMAP
   covering span via the byte-identical lineage mapscan; checkpoint
   constant 0x4 only as a LABELED fallback `orig_prot_source=
   checkpoint_constant` when the map is untrusted).
2. `mprotect(page, 0x4000, PROT_READ|PROT_EXEC)` on exactly the page(s)
   covering each 16-byte read (tick → 0x33ac000, sco → 0x890000; never the
   whole exec span). Nonzero return → errno recorded (`mprotect_rc`,
   `mprotect_errno_hex`), refusal record emitted, **no read attempted**.
3. Read 16 bytes only after mprotect success.
4. Restore original protection on the same page(s); `restore_rc` recorded;
   `mprotect_restored=0`/`anomaly=1` if verification fails (never blindly
   retried).
5. Compare against expected (`byte_match`, `diff_mask`).

The original task's "direct read, fall through on fault" approach was
rejected per the correction: it is unimplementable safely (no signal/
sigaction import exists in the frozen contract; a raw read fault would
kill the process before any code could react).

## 3. The mprotect verification the controller requested — NEGATIVE, and what was built instead

Requested: "verify: sceKernelMprotect IS in the pinned SDK's import set (it
was used by the worker-candidate windows-candidate builds)."

Verification result (all in-workspace, this session):

- **Frozen 191-import contract** (console-proven family): NO mprotect row.
- **Pinned SDK v0.43** (`ps5-payload-sdk`, whole-tree grep): NO
  sceKernelMprotect declaration anywhere.
- **Observed libkernel exports** (`ue4ss-original-libkernel-exports-1/
  strict-metadata.json`): zero mprotect strings.
- **windows-candidate**: `test_owned_runtime.c:26` calls HOST-POSIX
  `mprotect(2)` inside the Windows loader emulation — it is NOT a console
  sceKernelMprotect import. The cited evidence does not exist on the
  console side.

What WAS verified in the pinned SDK and used instead (the controller's
"check the exact signature and PROT_* constants" instruction):

- `SYS_mprotect 74` — `target/include/sys/syscall.h:82`
- `PROT_NONE 0x00 / PROT_READ 0x01 / PROT_WRITE 0x02 / PROT_EXEC 0x04` —
  `target/include/sys/mman.h:52-55` (the VMMAP prot=0x4 IS PROT_EXEC here)
- `int mprotect(const void *, size_t, int)` — `sys/mman.h:263` (FreeBSD ABI)
- `PAGE_SHIFT 14` → 16 KiB pages — `machine/param.h:99` (matches the
  directive; PS5 branch)

Implementation: a 3-instruction-class inline x86-64 syscall shim
(`igcar_sys_mprotect`, src/sys_mprotect_shim.h) whose number and constants
are parsed at BUILD TIME from those pinned headers and stamped into the
generated config — nothing hardcoded, file:line provenance in the build
receipt. **No new import identity is introduced**; the dynsym table still
carries exactly the 7 NID-encoded verbatim rows (artifact-checked), and
`sceKernelMprotect` is on the FORBIDDEN-symbol list to prove it.

- Numbering-preservation evidence: the console-proven lineage successfully
  calls plain FreeBSD wrappers `getpid` (20) and `sysctl` (202) in
  v2/v3/igf4 runs — the low FreeBSD syscall numbers are preserved on this
  console; 74 sits in the same preserved range.
- **Residual risk (the sign-off gate):** the kernel syscall number itself
  cannot be verified on-host. If this console renumbered it, the most
  likely behavior is an errno refusal (recorded; NO read — safe by
  construction), but this cannot be guaranteed in advance. The controller
  must accept this before staging (STAGE-PLAN §0), or supply an
  evidence-backed mprotect import row for an igf6.

## 4. Probe contract (seq 1..7, one build_id, one launch)

1 `entry` marker flushed FIRST (lineage contract) → 2 `startup` →
3 `mapscan` (internal coverage + restore-prot source; NO dump) →
4 `tick_anchor` → 5 `sco_anchor` (each: mprotect → read → restore →
compare; per-anchor fields `method_used, orig_prot, orig_prot_source,
page_start, page_size, pages, mprotect_called, mprotect_rc,
mprotect_errno_hex, mprotect_prot_requested, syscall_impl, read_performed,
bytes_read_hex, byte_match, diff_mask, restore_called, restore_rc,
restore_prot, mprotect_restored, anomaly, refusal`) → 6 `gua_pred_replay`
(CORRECTED predicate under cfgH header-span + cfgL live-spans feeds, both
layout hypotheses, map-gated like igf4; would have been `status=tbd` had
the gc-fix not landed — the hook was built for both timelines) →
7 `summary` (`anchors_confirmed`, `mprotect_anomalies`,
`pred_replay_performed`, `pred_validated_any_cfg`,
`promotion_gate_indicator_anchors_only`, `promotion_gate_indicator` —
INDICATOR ONLY; promotion is the controller's call).

Expected bytes provenance: igf4 evidence file + independent
worker-expected-bytes bundle, cross-checked EQUAL at build time (loud
failure otherwise); image sha `3b6cfad4…`; file_offset = vaddr+0x4000;
expected16 `554889e54157415641554154534881ec` for both anchors.

## 5. Verification done locally

- Two-run determinism (byte-identical all four frozen artifacts).
- Artifact checks: make_fself segment equality, SDK 0x11000000 stamp,
  explicit DT_INIT/DT_FINI, 1/1 init/fini slots, exactly 7 NID-encoded
  dynsym imports / zero weak, forbidden-symbol gate (including
  `sceKernelMprotect`), 2 raw `syscall` instruction sites (the shim).
- Host checks: page-range math (incl. crossing + rejection cases),
  diff-mask semantics, and the REAL byte-identical lineage record writer —
  every emitted JSONL line `json.loads`-validated; refusal/no-read coupling
  asserted.
- Vendored byte-identity vs the igf4 lineage LOUD-verified for all 11
  vendored files (hashes in build-receipt.json).

## 6. Honest limitations

- No console run: every in-game expectation in STAGE-PLAN-CODE-ANCHOR.md is
  a prediction, not a result. Nothing here claims AAA anything, full
  parity, or completion — the anchor check remains OPEN until the launch
  happens and `byte_match=1` is observed for both anchors.
- The syscall-74 residual risk (§3) is real and surfaced, not hidden.
- The restore step returns the page to its CURRENT-map protection; if the
  map was untrusted and the constant 0x4 fallback was used, the record
  says so (`orig_prot_source=checkpoint_constant`).

## 7. Bounds accounting

~50 tool calls of the 80 budget (reads of lineage sources, SDK/contract
verification greps, 11 file writes, 5 build runs including 3 loud failures
fixed at build time — `#ifdef`-vs-`#if`, missing include path, missing
boundaries.S/sentinels.c — and 2 host-check runs). No unrelated files
touched; no failed-job replays; every rebuild was a NEW build after a
source fix, never an identical resubmission.
