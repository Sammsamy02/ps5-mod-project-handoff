# FILL-IN FOR worker-exec-writability — exec-write probe skeleton

This directory is your READY ARTIFACT. The machinery is built, host-tested
(mocked mprotect, 99 checks), determinism-double-built, and artifact-gated.
Your design injects transitions WITHOUT touching any machinery file.

## 1. Where the transitions go

The ONLY file you edit is:

    /WORKSPACE/.codex/worktrees/translation-core/what/outputs/
      2026-10-04-zcode-team/worker-writability-probe-build/TRANSITIONS.json

Append objects to its `transitions` array:

```json
{"addr": "0x...", "from_prot": "0x4", "to_prot": "0x5",
 "page_count": 1, "confirmed": 1, "note": "far-from-hooks page"}
```

* `addr` — ABSOLUTE target vaddr (you resolve rt = base + offset; the probe
  does no base discovery of its own).
* prot bits are VMMAP semantics: bit0=R, bit1=W, bit2=X.
  X->RX = 0x4 -> 0x5. X->RWX = 0x4 -> 0x7. Restore is automatic to
  `from_prot` (the probe re-reads VMMAP after restore and records proof).
* `confirmed` MUST be 1 ONLY on rows you have designated (the far page).
* Ordering = array order. Your brief's sequencing (X->RX first, then
  X->RWX on ONE far-from-hooks page) is expressed by putting the RX rows
  before the RWX row. The machinery executes rows in order.

The table becomes `generated/xwp_transitions.{h,c}` at build time. Hard
bounds: 32 rows max (`XWP_MAX_ROWS`), note length 95, quotes/braces
sanitized. Rows beyond 32 are recorded and never run.

## 2. How it is injected (no machinery rebuild)

    cd .../worker-writability-probe-build
    # edit TRANSITIONS.json
    python3 build_exec_write_probe.py        # rebuilds BOTH variants
    python3 check_xwp_artifact.py determinism/run-skeleton-1 skeleton
    python3 check_xwp_artifact.py determinism/run-armed-1 armed
    cd tests && python3 run_host_checks.py   # logic re-verified (mocked)

Only `TRANSITIONS.json` bytes change (they are part of the deterministic
build-id seed, so every build gets a distinct `igd1-<hash12>` id).
`src/exec_write_probe.c` (the machinery), the vendored lineage files, and
the packaging pipeline are NEVER edited.

## 3. What is already decided for you (safety rails, baked in)

* Attempt budget: 8 TOTAL across all rows (`XWP_ATTEMPT_BUDGET`). Row order
  is also budget priority.
* An attempt runs ONLY if ALL of: build is ARMED; row `confirmed=1`; budget
  remains; a FRESH same-invocation `sysctl KERN_PROC_VMMAP` scan covers
  `[addr, addr + page_count*0x1000)` fully; the covering span's observed
  prot == `from_prot`. Everything else is recorded-and-refused with
  `no_memory_touched: 1`.
* Sequence per attempt (each step a flushed JSONL record, mirrored to
  /data/plugins/UE4SS.log): `row` -> `vmmap_persist` (pre-state span) ->
  `attempt` (rc + errno) -> `rw_test` (read always; content-preserving
  write-back ONLY when to_prot has W; `recovered` flag) -> `restore` (rc +
  errno, ALWAYS attempted) -> `post_restore` (fresh VMMAP re-proof).
* `module_start` returns 0 on EVERY path (fail-open). No threads. No unload.
* Zero raw syscalls (the igf5 raw-syscall-74 crash class is structurally
  absent; build gates assert 0 `syscall` instructions / no 0F 05 pair).

## 4. The mprotect route — READ THIS (loud flag honored)

The frozen 191-import contract contains NO mprotect-class strong row
(verified by this build against
`.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json`).
`kernel_mprotect` appears there ONLY as weak/absent/unresolved, and the
lineage Packager REJECTS all weak imports. The ONLY mprotect mechanism ever
run on console (igf5 raw-syscall shim `igcar_sys_mprotect`) is the suspected
console+game crash and stays forbidden.

Therefore the route is TWO-STAGED, both PRXs already built:

1. **SKELETON** (`modbridge_exec_write_probe.prx`,
   igd1-63fa8a03078e, sha256 960aabe15c7dde53fad3e6235969c605b05941966bf5e2ef8d6f5786ec184c66):
   EXACTLY the 7 console-proven imports, ZERO new identities. The mprotect
   binding is a compile-time stub that cannot change any protection — with
   a FILLED table it still runs the complete record/decision/restore
   sequence on console (every attempt records outcome
   `mprotect_import_absent`=11). Stage THIS first if you want console proof
   of the machinery.
2. **ARMED, EMPTY table** (`modbridge_exec_write_probe_armed.prx`,
   igd1-27618a58f062, sha256 7b9e030c74b501a5aa578a533a4f52f32303284730ff15b3926f3c87d8489402):
   adds exactly ONE derived import — `sceKernelMprotect` (libkernel), the
   SharpProspero StubCatalog spelling, **NOT console-proven** (first new
   identity since the v2 isolation; loud flag embedded in
   import-contract.json). With the default empty table it performs ZERO
   mprotect calls — its only purpose is your LOAD-RESOLUTION SANITY stage:
   if seq1/seq2 records appear, the 8th import resolved at load.

Staging order for your design: ARMED+EMPTY (load sanity) -> SKELETON or
ARMED+FILLED per your acceptance decision. As with the whole igf lineage,
in-game staging is USER-COORDINATED; nothing here was executed on console.

## 5. Handoffs and caveats

* Internal module name: the frozen Packager.dll (used IN PLACE, never
  rebuilt) hardcodes `moduleName: "modbridge_ingame_facts"`; the FILE is
  `modbridge_exec_write_probe.prx` at
  `/data/plugins/modbridge_exec_write_probe.prx`. It does NOT replace the
  loaded igf7 module — the ini line decides what ploader loads.
* Records: primary channel `/data/modbridge-exec-write-probe.jsonl`
  (`marker:"modbridge.execwriteprobe"`), mirror `/data/plugins/UE4SS.log`
  (best-effort; mirror failures counted in every summary record).
* If BOTH channels are dead the module is a silent no-op returning 0
  (observable absence is the signal; igf5 lesson).
* rw_test honesty: there are NO signal handlers in this lineage. Fault
  localization is by write-through records already on disk (igf6 safety
  rationale). The write test is content-preserving (same byte back).
* Budget is per module_start invocation (single-shot; module stays loaded
  and INERT afterwards).
