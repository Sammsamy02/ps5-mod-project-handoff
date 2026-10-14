# REPORT — worker-bootstrap-trace

**Verdict: STARTUP TRACE COMPLETE.** Main deliverable: `STARTUP-TRACE.md`
(14-step numbered sequence, silent-failure taxonomy S1-S5, per-import
culpability table). Claim budget: ~35 of 50 calls. Write scope: only this
directory. Read receipts honored; no console contact, no network, no nested
subagents.

## Key findings (essentials)

1. **The composed PRX's `module_start` is the frozen probe bootstrap**
   (`native-runtime-startup/lifecycle/module_entry.c` — note: the task brief's
   `bootstrap/module_entry.c` path does not exist), receipt-proven by
   `static-checks.json` entry-alignment codegen match. The UE4SS bootstrap
   (`ue4ss_backend.cpp` / `runtime_owner.cpp`) is NOT the entry; it sits behind
   `Prepare*UE4SSRuntime`, which — per WIRING.md §4 — is called only from the
   **"future" game-bound entry unit**. `StartActivatedRuntimeWithAck` has no
   call site in the composed image.
2. **First observable write = probe log**
   `/data/townfall_runtime_aligned_entry.log` (`diagnostic_open_channel` +
   `module_start_entered`), steps 4-5, written with only the three frozen
   bootstrap imports, before any composed code, constructor, or added import
   executes. "Zero output" was measured against UE4SS.log/runtime-status.txt —
   the probe log is the artifact that actually pinpoints the failing step.
3. **Structural, not failure:** `runtime-status.txt` can never appear in
   composition-4 as shipped: `RuntimeAckSink::Open` runs only inside
   `PrepareActivatedUE4SSRuntime`, whose only caller is never invoked. Same for
   the ack-driven UE4SS start path.
4. **The 7 additional imports (198 vs 191) are exonerated for the pre-logging
   window.** They are [sceKernelDlsym, sceKernelGetModuleInfo,
   sceKernelLoadStartModule, sceKernelStopUnloadModule, qsort, clock_gettime,
   getpid]. Five execute only after engine init (CppModModule C++-mod loading);
   two only inside the never-called ack supplier. Their only load-time
   footprint is 5 `.data.rel.ro` address slots (`supplier_import_extension.S`);
   an unresolvable slot fails the LOAD loudly, contradicting "ploader OK".
   Composition-4's own composed-object import delta vs composition-3 is EMPTY
   (run-3 receipt).
5. **Most likely silent-failure candidates, ranked:** (S1) step-4 `-22` —
   diagnostic channel open failure, total silence, return ignored; (S2)
   load-time DT_INIT/relocation work over the enlarged 30.8 MB image (what
   DT_INIT@5536 resolves to is not determinable on disk — flagged unverified);
   (S3) a throwing/hanging static constructor among the 235 init slots —
   composition-4's actual new code (query-harness singleton, replaced
   UnrealInitializer statics) runs there and is visible only in the probe log;
   (S4) import-validation/input-preload failure — probe-log-visible only.
6. **Decision rule:** read the probe log's last line. Absent → S1/S2
   (environment or load-time, not imports). Ends at `constructor_begin(i)` →
   constructor hang/crash (index i). `module_start_return 0x0` → module_start
   succeeded; zero output is then the expected un-wired-start state, and the
   real gap is the missing game-bound entry unit, not a bootstrap defect.

## Path corrections to the task brief

- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/module_entry.c` —
  does not exist; actual entry: `.../native-runtime-startup/lifecycle/module_entry.c`.
- The compiled bootstrap sources for the candidate come from
  `.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/`
  (same files, verified via build-receipt compile argv); the shared
  `generated/probe_config.h` pins the log/input paths.
