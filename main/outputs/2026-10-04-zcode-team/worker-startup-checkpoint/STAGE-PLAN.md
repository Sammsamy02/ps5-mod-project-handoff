# STAGE-PLAN — startup checkpoint diagnostics (worker-startup-checkpoint)

Status: FROZEN, ready to stage. Both payloads are locally verified
(LOCAL_PACKAGE_CHECKS_PASSED, exact dynsym oracle equality, two-run
determinism byte-identical). This worker executed NO staging, NO console
contact, NO launch — the controller stages per this plan; the launch stays
user-coordinated.

## 0. Question being decided

The composed runtime (composition-4, staged at
`/data/plugins/modbridge_supplier_composition.prx`) got ploader "OK" but
produced ZERO output (`/data/plugins/UE4SS.log` never appears). ploader "OK"
means only that `sceKernelLoadStartModule` returned a positive module id
(injection level). We do not know whether `module_start` was ever executed.
Two independent failure classes live in that gap:

1. **Import resolution** — composition-4 carries 5 imports the running
   aligned-entry module does not have (see §4).
2. **Start-step / runtime code** — the module loads but `module_start` (or
   the UE4SS init it triggers) never reaches its first log write.

The two diagnostics separate these classes. They share composition-4's
module name, staged path, and packaging recipe (SharpProspero Packager,
frozen finalize_sdk SDK 0x11000000 stamp, pinned make_fself — the frozen
igd1 lineage recipe end to end), and both run the SAME checkpoint-only
`module_start`: open `/data/plugins/UE4SS.log`, append one line
`DIAG: module_start reached at <epoch>.<ms> pid=<pid> build=<id> variant=<v>`,
close, return 0 always. No UE4SS, no supplier, no harness, no C++.

## 1. Payloads of record (after determinism, `determinism/<variant>/run1/`)

| | sdg1 (min) — path probe | sdg2 (imp) — resolution probe |
|---|---|---|
| payload | `diag/determinism/min/run1/modbridge_supplier_composition.prx` | `diag/determinism/imp/run1/modbridge_supplier_composition.prx` |
| fself sha256 | `ac6497c362d9aeb597bb7398aff62b013d4bba33dbf6623f9341b16e3055f910` | `0e5b36bc03d97619990a061baafd077fb331d3c0fd50249488aaaace50d32d86` |
| fself bytes | 6,557 | 22,087 |
| bare-sdk sha256 | `c9e69cd75d5917871a93868b777ca061d62ddc9ba3f1009f67793273bc56f088` | `ff4275bc6ab33d3aaba644136b9bb5d94fe5486daced93a0c6297ba7c504c5a4` |
| bare-sdk bytes | 68,896 | 82,928 |
| imports | 7 (== igd1 console-proven rows, exact oracle) | 196 (== composition-4 dynsym, exact oracle) |
| build_id | `sdg1-72abf1a3cd8d` | `sdg2-6f404cee6b94` |

Oracle law (build-time, re-verified by `check_diag_artifact.py`): the final
bare PRX dynsym UND set (name+bind+type) equals, entry for entry, the
ground-truth set — igd1 run1 for sdg1, composition-4 bare-sdk for sdg2.
sdg2's on-console import resolution is therefore a DIRECT proxy for
composition-4's.

Readback law (documented channel behavior, worker-v3-startup-diag §2): the
ftpsrv readback of a staged file is the segment-covered prefix of the inner
ELF = bare-sdk − 1560 bytes, e_shnum zeroed in the embedded header; full-hash
equality through this channel is impossible by construction.
- sdg1 expected readback: **68,896 − 1560 = 67,336 bytes**, `\x7fELF` magic.
- sdg2 expected readback: **82,928 − 1560 = 81,368 bytes**, `\x7fELF` magic.

## 2. Staging order (two launches, one variant each)

1. **Backup** the current composition-4 file on console
   (`/data/plugins/modbridge_supplier_composition.prx`, fself sha256
   `de3723027d6f54c0205959a83578e2113a9c74a0560b44a0897acbc17309aa0f`,
   30,778,567 B) to a sibling name — it is the artifact under diagnosis.
2. Note/delete any existing `/data/plugins/UE4SS.log` (record its mtime).
3. **Stage sdg1** (min) AT THE SAME PATH
   `/data/plugins/modbridge_supplier_composition.prx` (same module name —
   same ploader slot, no ini change, exactly the igd1 replacement pattern).
   Verify staged bytes by readback against §1 (size law + magic).
4. Launch once (user-coordinated). Read back `/data/plugins/UE4SS.log`.
5. **Stage sdg2** (imp) the same way (after preserving the sdg1 log copy),
   launch once, read the log again.
6. **Rollback**: restore the composition-4 backup to
   `/data/plugins/modbridge_supplier_composition.prx` after either result.

Never stage both diagnostics in one launch; one variable per launch.

## 3. Outcome matrix (decides WHERE startup fails)

| sdg1 (min) | sdg2 (imp) | Conclusion | Next action |
|---|---|---|---|
| line appears | — | `/data/plugins/UE4SS.log` writable; a 7-import module reaches module_start in this slot. Path + start step exonerated at small scale. | stage sdg2 |
| no line, game fine | — | module_start NOT reached even for the console-proven 7-import shape in this slot → problem is slot/staging/start-step level, NOT imports, NOT UE4SS code. | compare with aligned-entry sibling in same launch (it logs to its own file); control-restage; bisect slot assumptions |
| line appears | line appears | ALL 196 composition-4 imports resolve AND module_start runs. The gap is INSIDE the composed runtime: its entry executes but dies (or writes elsewhere) before the first UE4SS.log write. | bisect composed-runtime entry path (init_array ctors → UE4SS init → first log write); NOT a loader problem |
| line appears | ploader FAIL / no OK | **Import resolution is the blocker**: some of the 5 new identities (§4) — most suspects: the #B#C-flavored libkernel module-query rows. | build sdg3..sdgN reduced-surface variants dropping one suspect group at a time (all machinery already in build_diag.py) |
| line appears | ploader OK, no line | Full surface resolves, but module_start still not reached with composition-4's dynsym → the load→start gap is size/segment/packaging-shaped (sdg2 is 22 KB vs composition's 30.8 MB), not import-shaped. | size-class bisection (padding objects), DT_INIT reachability at 30 MB scale |
| any | game crash | Format-level fault at this slot — treat separately. | stop, restore backup, report |

## 4. The import delta being tested (composition-4 minus aligned-entry)

Both dynsyms parsed from disk (`import-diff.txt` this dir; parser
`dynsym_und.py`): aligned-entry has 191 UND imports, composition-4 has 196,
and the 5 extra identities are exactly the composition lineage's supplier
extension (composition-3's `supplier_import_extension.S` order; slot→NID
mapping verified from composition-4's own relocation table — slot 2
`wzvqT4UqKX8` cross-checks against ploader's documented
`sceKernelLoadStartModule` NID):

| NID long form (composition-4) | function | note |
|---|---|---|
| `LwG8g3niqwA#B#C` | `sceKernelDlsym` | new vs aligned-entry |
| `kUpgrXIrz7Q#B#C` | `sceKernelGetModuleInfo` | ploader itself imports `kUpgrXIrz7Q#A#B` — different library flavor |
| `wzvqT4UqKX8#B#C` | `sceKernelLoadStartModule` | NID repo-proven via ploader |
| `QKd0qM58Qes#B#C` | `sceKernelStopUnloadModule` | new vs aligned-entry |
| `AEJdIVZTEmo#A#B` | `qsort` | libc row |

Corroborating risk history: the earlier packaging candidate
(worker-prx-packaging 20261004-133501 static-checks.json) flagged exactly
these five as `imports_outside_composed_baseline`; the v1 in-game probe was
silently load-rejected with DERIVED (non-verbatim) import identities while
the aligned-entry sibling loaded — the #B#C flavors are not yet
console-proven by any loading module.

## 5. Provenance / scope

- Recipe: vendored frozen igd1 pipeline (`worker-v3-startup-diag/diag/`),
  Packager.dll vendored binary; build receipt per variant in
  `diag/build-<variant>-*/build-receipt.json` and
  `diag/determinism/<variant>/run{1,2}/`.
- Contract rows: sdg1 = 7 verbatim frozen-191 rows (byte-equality verified
  against igd1 run1's contract); sdg2 = 196 verbatim rows from
  composition-4's own `supplier-contract.json` (`contract.imports`).
- Both payloads: `promotion_allowed=false`, `engine_initialization=false`,
  proposal-only diagnostics; they never run on this Mac.
- Writes confined to `outputs/2026-10-04-zcode-team/worker-startup-checkpoint/`.
