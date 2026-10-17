# l13 local admission changes — review before packaging or upload

**Status: PASS_MAC_ONLY. No console contact. No new PRX packaged or uploaded.**

The candidate is isolated here. The l12 source, frozen `024a4143…34b4` PRX,
original captures, and three recorded pad source files remain byte-identical.

## Changes requested by the user

1. **Order:** exact VirtualQuery coverage runs first for each needed heap read.
   Regions must contain every requested byte, permit CPU reads, and be committed.
   The log records needed and returned bounds, protection, flags and admission
   source. Positive VQ needs no copyin cross-check. Inconclusive VQ falls through
   to owned capture/oracle, then the shadow policy.
2. **Budget:** one 60-second admission deadline includes retries and sleeps.
   The actual l12 10-second-per-round schedule reproduced at 475 seconds; the
   new initializer's unavailable/not-ready fixtures stop at 60 seconds. This is
   a scheduling bound, not cancellation of an already-blocked native call.
3. **Sink:** O_CREAT was already present (`0x701` old create flags), and both
   probes used `/data/plugins`. Retained Ploader source restores root/jail state
   after injection, making the delayed absolute-path lookup a supported cause
   of ENOENT, not a console-proven cause. Retain only the parent directory during
   startup; create and read back a uniquely named sink relative to it only when
   VQ falls through. No eager Control D. New create flags are `0xf01`, including
   O_EXCL, so collisions never truncate old evidence. Startup-failure cleanup
   and successful-stop lifetime are covered. Exact paths are losslessly logged
   as ordered ASCII words, with flags, raw rc and normalized errno.
4. **Errors:** normalize signed and zero-extended SCE codes before count
   arithmetic. ENOENT remains a sink availability failure. Short writes advance
   correctly; a following EFAULT excludes the range and invalidates captures.
5. **Shadow:** negative evidence persists and wins over later proofs/captures.
   No direct heap read occurs without exact live coverage. Disk-image identity
   supplies no live heap bytes, so the current shadow policy explicitly declines
   unresolved admission; it does not run an unbounded image scan.
6. **Harvest/checklist:** row N covers startup timeout before rescue. Zero-match
   grep no longer aborts the summary. Partial failed downloads cannot become
   evidence; both probe filename prefixes are discovered. File presence and
   runtime activity are distinguished. `/tmp` working copies and durable copies
   match; original scripts/checklist were preserved.
7. **Pad:** `0x80920004` maps to ALREADY_OPENED in the pinned PS5 SDL source.
   Ownership/borrowing semantics are not established. No pad code was changed.

## Fresh verification

Final aggregate run: `receipts/verification-20261007T152224147051Z/GATES.json`.

| Check | Result |
|---|---|
| Production admission helper, ASan/UBSan | 22 scenarios pass |
| Actual initializer adapter and Phase 3 | 9 scenarios pass |
| Actual bridge sink region and lifecycle helper | 60 assertions pass |
| Actual harvest script, fake curl, network denied | 5 scenarios pass |
| PS5 SDK compilation of both changed TUs | Pass |
| New bridge definitions and SDK import declarations | Pass |
| Frozen l12 PRX/source and pad preservation | Pass |

The initializer emits the same six dependency warnings as the baseline;
the bridge compiles with `-Wall -Wextra -Werror`. No compiler errors remain.
One initial aggregate harvest check failed because macOS refused a nested
sandbox. The runner now lets that harness apply its own network-denied sandbox;
the subsequent full run passed. Both receipts are preserved.

The symbol boundary adds exactly `_openat` and `sceKernelRead`, both declared by
the pinned libkernel SDK. `receipts/l13-import-extension.json` records the
candidate import identities and their evidence. This verifies declarations,
**not native loader resolution**. A full composition/PRX packaging run must
apply those additions at the bootstrap/combined-object contract stage, and
preserve SDK/fake-SELF checks, before any upload can be proposed. The old
packager and l12 build pins were not silently changed.

## Review material

- `review.diff`: complete production source, harvest and checklist diff.
- `receipts/LATEST-GATES.json`: source hashes, preservation results, fresh test
  receipts and explicit upload HOLD.
- `reports/ADMISSION-REPORT.md`: timing, coverage, negative evidence and tests.
- `SINK-ERROR-REVIEW.md`: sink cause evidence, lifecycle and normalization.
- `reports/HARVEST-CHECKLIST-REPORT.md`: script regression and raw receipts.
- `reports/PAD-ERROR-0x80920004.md`: source links and confidence limits.

Re-run local checks with `python3 outputs/2026-10-07-l13-local-admission/verify_local.py`
from the worktree. This command does not run the real harvest or contact the
console. Uploads, payload loads, launches and console configuration changes
still require the user's explicit approval for that step.
