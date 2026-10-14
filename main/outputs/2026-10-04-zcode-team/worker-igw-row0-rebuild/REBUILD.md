# REBUILD — worker-igw-row0-rebuild (2026-10-05)

The designed injection procedure (FILL-IN-FOR-DESIGN-WORKER.md §2) executed in MY OWN
staging copy: `outputs/2026-10-04-zcode-team/worker-igw-row0-rebuild/`. The original
`worker-writability-probe-build/` tree was left byte-identical (verified after every step).
LOCAL ONLY — no console contact, no launch.

## 1. Trace verification (step 1) — all claims confirmed first-hand

| claim | verification |
|---|---|
| gate at `exec_write_probe.c:166-168` | read: `} else if (t->confirmed == 0) { why = XWP_SKIP_NOT_CONFIRMED; gate_failed = 1; }` — counted skipped at :190-193; `XWP_SKIP_NOT_CONFIRMED = 2` at `exec_write_probe.h:72` |
| builder default at `build_exec_write_probe.py:136` | read: `"confirmed": 1 if t.get("confirmed") else 0,` |
| no `confirmed` key in TRANSITIONS.json | read all 6 rows — no key; staged snapshot sha `9cac04a1…` equals the launch-9 build receipt's `transitions_sha256` |
| build-id seed | `build_exec_write_probe.py:446-457`: `h.update(TRANSITIONS_JSON.read_bytes())` — new table bytes ⇒ new `igd1-<hash12>` |

## 2. Staging (step 2)

- Copied into this dir: `src/` (machinery), `tests/`, `build_exec_write_probe.py`,
  `check_xwp_artifact.py`, `TRANSITIONS.json` — all verified byte-identical to the
  originals (`diff -r` / `cmp`).
- NOTE: the builder asserts `HERE.parents[2] == what`, so the staged tree lives at THIS
  dir's root (same depth as the original), not in a subdirectory.
- The ONE edit — byte-minimal, row 0 only:

```diff
--- TRANSITIONS.json.orig     (9cac04a1d250adbc…, 1167 B)
+++ TRANSITIONS.json         (2b0a363530b431e9…, 1186 B)
@@ -5,6 +5,7 @@
    "addr": "0x7878000",
    "from_prot": 4,
    "to_prot": 5,
    "page_count": 1,
+   "confirmed": 1,
    "note": "IGW-1 T2: X->RX far page (past extent 0x7879b1c); read-test; restore follows"
```

- JSON-semantic check: rows 1-5 and the schema field are untouched (asserted
  key-by-key); row 0 gained ONLY `confirmed: 1`. The other five rows (T3/T4/T5
  restores, D-1 pair) stay `confirmed: 0` — per the task and the probe's own
  transition table, T3/T5 presuppose the T2 verdict and the machinery already
  restores + re-proves after every attempt.

## 3. Build (step 3) — both variants, deterministic double-build

`python3 build_exec_write_probe.py` (unmodified) — every builder gate passed:

- frozen lineage PRX hash `3b998f09…` verified; forbidden-symbol gate; inverted
  syscall gate (`== 0`); import-contract divergence gate; inner `.text` 0F 05 gate;
- determinism run1 == run2 byte-equal (final_prx / inner_elf / combined / contract)
  for BOTH variants.

| variant | build_id | fself sha256 | bytes | inner ELF sha256 |
|---|---|---|---|---|
| ARMED (deliverable) | `igd1-e030a09eb4d1` | `e1541af612f97bb450b5f61888ae0c2fc93dea72454723dc9e0ffafabd6002df` | 20013 | `cc544d6e8fe94c151ca9d49bd12613f87999e8be20dcb935ff824bb1ef37a6f9` (69464 B) |
| SKELETON | `igd1-5f1a25c33203` | `d8d16ed7a222bedffa8fde8627e6f490c01490ed2bd098b23c8aef943bbff134` | 19933 | `8de0eb54a28e629255a9745c092aabc9652552899e93044ec9100a1ac8619463` (69384 B) |

Both ids are NEW and distinct (launch-9 was `igd1-6ebab07f2111` armed) — no resubmission
of an identical build.

Host gates carried by the tree, all run and green:

- `python3 check_xwp_artifact.py determinism/run-skeleton-1 skeleton` → rc=0
  (7 strong imports, 0 weak, 0 syscalls, stub route);
- `python3 check_xwp_artifact.py determinism/run-armed-1 armed` → rc=0
  (8 strong imports — the 7 verbatim + `sceKernelMprotect` typed reference exactly
  once; 0 syscalls; route `sceKernelMprotect_import`; loud NOT-console-proven flag
  carried in `import-contract.json`);
- `cd tests && python3 run_host_checks.py` → PASS (armed 93 checks, unarmed 6;
  mocked mprotect; every JSONL record `json.loads`-parsed; ordering/grammar/budget
  assertions OK — machinery logic re-verified against the confirmed-row semantics).

SHT-strip upload constants for the new ARMED fself (computed with the exact
STAGE-PLAN-COMBINED §3 rule, validated against the documented launch-9 constants):

| field | value |
|---|---|
| staged fself | `e1541af6…` (20013 B) |
| bare-sdk | `cc544d6e…` (69464 B) |
| readback bytes | **67832** (max(p_offset+p_filesz) over all phdrs — reproduces 67832/`daea2ff4…` on the old artifact) |
| readback body sha (64..end) | `e3f13d1994a1b4d27de05e31885d5a5cf9ea91daea3c57c2c229afd3696c295e` |

## 4. What did NOT change

- Machinery: `src/` diff-clean; budget 8 (`XWP_ATTEMPT_BUDGET`), restore-always,
  fail-open `module_start`, zero raw syscalls — all intact (binary-verified in
  BINARY-DELTA.md).
- Rows 1-5: `confirmed` fields read back as 0 in the built binary; rows 1, 4, 5 are
  byte-identical to launch-9's table; rows 2, 3 differ ONLY in their relocated
  `note_ptr` (string-pool address shift, see BINARY-DELTA.md).
- Generated `xwp_transitions.c` diff: exactly one field, row 0 `0u` → `1u`.
  `ingame_config.h` diff: only the `IG_BUILD_ID` line.
- Walkguard fold: NOT touched (fold-0 remains the only admissible state until a real
  verdict records exist on console).
- Receipts: `build-receipt-armed.json` (`c2551118…`) and `build-receipt-skeleton.json`
  (`6c5a1b53…`); `native_executed=false`, `in_game_results=false` — honest limits kept.
