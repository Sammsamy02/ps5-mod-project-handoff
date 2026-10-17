# DIFF-POINTERS — semantic records for the empty/unarchived byte-diffs of the 2026-10-05 fold

Worker: worker-manifest-repoint · 2026-10-05 · LOCAL ONLY. Scope: bookkeeping only.
Per coordinator instruction the pre-edit bytes of the still-unarchived edits are
treated as unrecoverable (superseded in place, files not git-tracked); NO byte-diff
is fabricated. Each still-empty placeholder below is given a SEMANTIC record by
pointing at the controller-authored provenance file:

  outputs/2026-10-04-zcode-controller/comp6-integration/EDIT-PROVENANCE-seven-edit-fold.md
  (sha256 4e1219df4cb7e1a4..., mtime 19:32:58; the EDITIONS
  `seven_edit_fold_2026_10_05.edits` block is the hash-level record)

## Diff inventory (verified on disk 19:5x local)

`outputs/2026-10-04-zcode-team/worker-luamod-lineage/diffs/`:

| file | bytes | sha256 (prefix) | state |
|---|---|---|---|
| LuaMod.cpp.3c39a86b-to-c92c92ff.diff | 3434 | 1074c4ec68fcf431 | EXISTS (edit 6; 8 hunks, all modbridge:: qualifications) |
| SeedValidation.hpp.b3d81b0f-to-db07ac60.diff | 13890 | 7d67a3bf639cdbf6 | EXISTS (edits 1+2; v5 policy + FailureClass + resolve_listing_gap) |
| bridge_r14.faec2336-to-ec53442b.diff | 0 | e3b0c44298fc1c14 (empty-file sha) | EMPTY — pointer below |
| UE4SSProgram.wpd1.7b5d4636-to-68b8bd18.diff | 0 | e3b0c44298fc1c14 (empty-file sha) | EMPTY — pointer below |
| UnrealInitializer.22f06286-to-e7f349af.diff | 0 | e3b0c44298fc1c14 (empty-file sha) | EMPTY — pointer below |

`outputs/2026-10-04-zcode-controller/comp6-integration/` contains NO .diff files
(verified by directory listing) — the LuaMod and SeedValidation diffs under
worker-luamod-lineage/diffs/ are the only byte-diff receipts of the fold.

## Pointer 1 — bridge_r14.faec2336-to-ec53442b.diff (0 bytes; edit 3)

SEMANTIC RECORD: EDIT-PROVENANCE-seven-edit-fold.md, section
"## Edit 3+7 — bridge (`faec2336` → `ec53442b` → `59db9dcd`)", item 3:
`bridge_kernel_range_verdict(addr,len)` added — one mincore syscall over the
whole page-aligned range through the generic `syscall` trampoline with
kernel-header `SYS_mincore`; 16 MiB cap (4 KiB static vec); fresh-evidence
diagnostic tag `pool_range_verdict_fresh`; includes `<sys/syscall.h>`/`<errno.h>`;
stale pre-v4 span-table comment corrected. Hash-level record: EDITIONS
`seven_edit_fold_2026_10_05.edits['3_launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp']`
(faec2336 -> ec53442b). worker-divergence-explain independently verified the
files differ (bridge delta is the +59-line mincore block).

## Pointer 2 — UE4SSProgram.wpd1.7b5d4636-to-68b8bd18.diff (0 bytes; edit 5)

SEMANTIC RECORD: EDIT-PROVENANCE-seven-edit-fold.md, section
"## Edit 5 — program (`7b5d4636` → `68b8bd18`)": `worker-pad-defer/staging/
program-wpd1/UE4SSProgram.cpp` `setup_mods()` PROSPERO branch: `widen_ascii`
lambda replaces both `ensure_str(mod.name)` / `ensure_str(mod.path)` — kills the
NUL-interleave double-garble (pid 700 log 18:40:10 "Mod is absent from native
Lua source: u\0e\0 4\0s\0s\0…", 15 NULs at log offset 1040, archived at
`launch10-evidence/pid700-fullfix/primary/fullfix-ue4ss.log`). Hash-level
record: EDITIONS `seven_edit_fold_2026_10_05.edits['5_...']`. Prebuild object
expectation for the new bytes exists (program.o e05ffa9c...,
offline-proof/program-widen-ascii-68b8bd18), and the old bytes' object 0d6ed951
was reproduced byte-exact from archived provenance first — the EDIT is proven;
only the per-hunk text receipt is absent.

## Pointer 3 — UnrealInitializer.22f06286-to-e7f349af.diff (0 bytes; edit 4)

SEMANTIC RECORD: EDIT-PROVENANCE-seven-edit-fold.md, section
"## Edit 4 — initializer (`22f06286` → `e7f349af`)":
`launch11-combined/units/UnrealInitializer.cpp`: pool wait loop consults
`resolve_listing_gap` ONCE per boot on the first listing-gap miss
(`bridge_kernel_range_verdict(blocks0, kFNameBlockCapacityBytes)`,
`ReadableCoverageSource::None`); the resolution note is appended to any
`seed_refused` message; the normal shape-ok break condition is unchanged.
Hash-level record: EDITIONS `seven_edit_fold_2026_10_05.edits['4_...']`;
comparator-regen-receipt.json adds feature grep evidence (resolve_listing_gap +
ReadableCoverageSource::None at lines 1027-1030).

## Pointer 4 — edit 7 (ec53442b → 59db9dcd): NO diff placeholder was ever created

SEMANTIC RECORD: EDIT-PROVENANCE-seven-edit-fold.md, section "## Edit 3+7",
item 7: return-convention fix from on-target evidence — the Prospero trampoline
returns kernel errors as POSITIVE raw codes and does not set errno (observed
`raw_rc=12=ENOMEM`, `errno==0`); mapping now accepts BOTH conventions so
NotMapped is never demoted to Unavailable. Hash-level record: EDITIONS
`seven_edit_fold_2026_10_05.edits['7_bridge-error-convention-fix']`
(sha256_after 59db9dcd78316b3738b7f7ef91a125d6f15165621554889dd87c89f82e7b103a;
evidence worker-payload-selftest/results/selftest.json check 5 case-b +
SELFTEST-REPORT.md). The ec53442b intermediate bytes are superseded in place and
not archived anywhere found — the faec2336→ec53442b→59db9dcd chain is anchored
by the recorded hashes and the selftest evidence.

## Out-of-fold observation (controller lane, NOT recorded as a fold edit here)

SETTLED UPDATE (20:0x): the 19:40 wave was recorded by the controller as fold
edits 8-12 in EDITIONS `seven_edit_fold_2026_10_05.edits` (aacbe5dd):
8_header-warning-hardening (SeedValidation v6 8a9a1c83 x3 copies),
9_program-start-reinstall-widen (wpd1 cb6cb14a), 10_stringhpp-print-pipeline,
11_bridge-errno-macro (bridge f9747267), 12_gate-accessor-allowlist. The
malformed runner pin described below was FIXED at 19:52:09 (clean 64-char
8a9a1c83...923c2). The observation paragraph below is preserved as the
as-observed record.

At 19:40 local a further SeedValidation wave landed: all three copies
(launch10-build/final-units, launch11-combined/units,
worker-luamod-rebase/staging/initializer-l10-r3) moved db07ac60 -> 8a9a1c83
(36359 bytes), EDITIONS rewritten 19:40:36, runner stage-header pin refreshed
19:40:27 — but the pin string at launch11_one_pass.py line 164 was then
MALFORMED (64-char new sha `8a9a1c83...923c2` + 8 leftover chars `64871f83`
from the old db07ac60 tail = 68 chars; fixed 19:52:09). No diff archived for
the v6 delta yet; edits 8-12 are the controller's fold, outside this worker's
seven-edit scope.

## Rule going forward

Per the coordinator: never fabricate a byte-diff. For any future edit of an
untracked file, capture `provenance-originals/<file>.<sha8>.orig` BEFORE the
edit (worker-luamod-lineage precedent) so procedure §3 diffs stay completable.
