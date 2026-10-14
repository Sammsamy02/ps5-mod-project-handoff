# REA Trial — worker-rea-trial (2026-10-05)

Mission: complete REA (v3.2.1) provider setup on this Mac (macOS darwin/arm64) and run one
concrete agent-driven investigation through REA's CLI against the retained PS5 eboot.
All work local-only; no project files modified outside `outputs/2026-10-04-zcode-team/worker-rea-trial/`.

## Environment (verified)

- Node v22.22.0, `rea` 3.2.1 at `/WORKSPACE/.openclaw/tools/node/bin/rea`
  (package root: `/WORKSPACE/.openclaw/tools/node-v22.22.0/lib/node_modules/rea-agents`)
- Ghidra 12.1.4 at `/WORKSPACE/Projects/re-tools/ghidra_12.1.4_PUBLIC`
- JDK 21 (`/opt/homebrew/opt/openjdk@21`); export `PATH=/opt/homebrew/opt/openjdk@21/bin:$PATH`
- Target: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  (194,241,424 bytes; ELF64 x86-64 FreeBSD type; PS5 UE 5.6.1)

## Address-space finding (prerequisite, verified from ELF headers)

The eboot's first EXECUTABLE LOAD segment maps `p_offset=0x4000 -> p_vaddr=0x0`
(`file = vaddr + 0x4000`, `runtime = vaddr + 0x400000`). Ghidra imports by p_vaddr, so
**Ghidra/REA addresses == ELF vaddrs**. Two consequences:

1. vaddr `0x23d590` (the FName ctor target) = file offset `0x241590`, where a real
   function prologue lives (`55 48 89 e5 ... movzx edi, word [rax]` — wide-char test).
   Treating `0x23d590` as a raw file offset instead lands mid-instruction inside a
   `lea eax,[rip+X]; pop rbp; ret` constant-thunk region (this affects the standalone
   file-offset-based disassembly in `outputs/2026-10-04-zcode-team/worker-re-fname-pool/tools/ctor_23d590.asm`).
2. vaddr `0x9365ec0` (FName pool) is in the RW LOAD's BSS range (memsz > filesz), which is
   correct for a runtime-populated name pool.

## STEP 1 — setup steps that worked (exact commands)

1. Direct `make decomp_opt` (documented route) FAILS on macOS: the COMMANDLINE target
   includes `bfd_arch.cc`/`loadimage_bfd.cc`, which need binutils `bfd.h` (absent):
   ```
   cd $Ghidra/Features/Decompiler/src/decompile/cpp && make -j8 decomp_opt
   -> fatal error: 'bfd.h' file not found
   ```
2. Working route — build `ghidra_opt` (the actual Ghidra protocol decompiler; no BFD
   sources; the Makefile's own `install_ghidraopt` target copies exactly this binary to
   `os/$(OSDIR)/decompile`), with arm64 overrides because the Makefile's Darwin section
   is stale Intel-only (`-arch x86_64`, `-mmacosx-version-min=10.6`):
   ```
   cd /WORKSPACE/Projects/re-tools/ghidra_12.1.4_PUBLIC/Ghidra/Features/Decompiler/src/decompile/cpp
   make -j8 ghidra_opt CXX="clang++ -std=c++11" ARCH_TYPE= ADDITIONAL_FLAGS=
   ```
   Builds clean in ~3 min (only harmless `-Wmismatched-tags` warnings). Result:
   3.2 MB `Mach-O 64-bit executable arm64`.
3. Install (REA's darwin/arm64 resolution expects dir `mac_arm_64`, NOT `mac_arm` —
   see `dist/ghidra/GhidraInstallation.js:72-77`):
   ```
   mkdir -p .../Ghidra/Features/Decompiler/os/mac_arm_64
   cp .../src/decompile/cpp/ghidra_opt .../os/mac_arm_64/decompile
   chmod +x .../os/mac_arm_64/decompile
   ```
4. Doctor after this: `ghidra-native_decompiler` PASSES. But `ghidra-platform`
   (darwin) and `ghidra-architecture` (arm64) still hard-fail: REA 3.2.1 gates the
   provider to `platform == linux || win32` regardless of the binary (independent of
   the native decompiler — the mission brief's assumption that they were downstream of
   it is not correct for this release). The same provider's runtime code DOES support
   darwin (explicit macOS limitation string + unix-socket transport in
   `GhidraProvider.js`), so the check is over-conservative for this host.

### Documented patches to installed REA dist (backups kept, both one-line)

File: `.../node_modules/rea-agents/dist/ghidra/GhidraInstallation.js`
(backed up as `GhidraInstallation.js.orig-rea-trial-backup`)

```diff
-  passed: platform === "linux" || platform === "win32",
+  passed: platform === "linux" || platform === "win32" || platform === "darwin",
```
```diff
-  passed: (platform === "linux" || platform === "win32") && architecture === "x64",
+  passed: (platform === "linux" || platform === "win32") && architecture === "x64" || (platform === "darwin" && architecture === "arm64"),
```

File: `.../node_modules/rea-agents/dist/ghidra/GhidraDefaults.js`
(backed up as `GhidraDefaults.js.orig-rea-trial-backup`) — the 330s startup deadline
cannot fit a 194MB import+auto-analysis and is not configurable (no env/config plumb):

```diff
-export const GHIDRA_STARTUP_TIMEOUT_MS = 330_000;
+export const GHIDRA_STARTUP_TIMEOUT_MS = 2_700_000;   // 45 min
```

Observed side effect of the old timeout: on deadline abort REA reported
`provider_timeout` (op `list_segments`, timeout_ms 330000) but LEAKED the headless JVM
(it kept burning CPU until killed manually, `PID 34173`, project dir `rea-ghidra-lc3kel`).
Repro note for REA maintainers; kill the JVM when a session times out.

## Doctor final state (`rea doctor --json`)

| check | state |
|---|---|
| node / node-toolchains / host | healthy |
| hopper | missing_analysis_engine — ACCEPTABLE for this trial (Ghidra provider is read-only and sufficient) |
| ghidra, ghidra-platform, ghidra-architecture | healthy (after patch) |
| ghidra-installation, ghidra-version (12.1.4), ghidra-headless | healthy |
| ghidra-native_decompiler | healthy → `.../os/mac_arm_64/decompile` (built here) |
| ghidra-java (21.0.12.1 JDK) | healthy |
| javascript-replay | healthy (disabled by default) |
| skill:identity, registration:{claude_code,codex,gemini_cli} | config_drift — optional integrations, not needed for CLI use; left unconfigured |

Overall `healthy: false` solely due to hopper + optional integration registrations.

## Smoke test (provider engages on darwin)

- `rea analyze <64KB prefix> --provider ghidra` → full Evidence JSON, 7.3 s wall
  (JVM + import + mini-analysis of the truncated ELF).
- `--provider ghidra` must be passed explicitly on query commands; without it, auto
  selection routes to providers that fail on this host (observed invalid_request errors
  from `rea function` attempts without the flag on the prefix file).

## The investigation (full binary, 194MB)

Each REA CLI command launches `analyzeHeadless -import <temp copy> -deleteProject`
(GhidraLauncher.js `ghidraHeadlessArguments`): there is NO persistent program database
across commands; every query pays the full import+auto-analysis cost again.
The `--snapshot` flag only reuses prior Evidence JSON, not the analysis DB.

Per-query latencies and results: see FINDINGS.md (updated as sessions complete).

## Workflow verdict (draft — finalized in REPORT.md)

See REPORT.md for the final verdict; summary: viable after the three documented fixes
(native binary build + darwin gate + timeout), powerful per-session, but the
import-per-command design makes multi-query workflows extremely expensive on a 194MB
target on this 16GB Mac.

---

# RESTART (2026-10-05, second attempt) — completed

Prior attempt state on resume: setup fully green (native decompiler + patches + doctor),
byte-level findings complete, but every full-binary REA query had failed
(45-min timeout; 53-min `execution_failure`). The `/tmp/wrfp_ghidra_proj` Ghidra project
another worker created (program `decrypted-eboot.bin`) existed.

## Root causes found this run

1. Session B's `execution_failure` was **`java.lang.OutOfMemoryError: Java heap space`**
   inside `X86Analyzer`/`SymbolicPropogator` (stack trace embedded in
   `xrefs-pool.OOM-execution-failure.json`). Ghidra's `analyzeHeadless` defaults
   `MAXMEM_DEFAULT=2G`.
2. **Zero-patch heap fix**: `analyzeHeadless` honors `GHIDRA_HEADLESS_MAXMEM` /
   `GHIDRA_MAXMEM` env vars. REA's launch env passes my shell env through
   (`ghidraJavaEnvironment` spreads `process.env`; it blanks only
   `_JAVA_OPTIONS`/`JAVA_TOOL_OPTIONS`/`JDK_JAVA_OPTIONS`/`GHIDRA_JAVA_OPTIONS`),
   so `export GHIDRA_HEADLESS_MAXMEM=8G` before any `rea` command fixes the heap with no
   dist patch. Verified in the child JVM argv (`-Xmx8G`).

## Persistent-project process mode (third and decisive fix)

REA re-imports the 194MB target per command (`-import <target> -readOnly -deleteProject`
in `GhidraLauncher.js`); import+auto-analysis cannot fit any deadline on this Mac.
Patch applied to `dist/ghidra/GhidraLauncher.js` (backup
`GhidraLauncher.js.orig-rea-trial-backup`): when env
`REA_GHIDRA_PROCESS_PROJECT_DIR` + `REA_GHIDRA_PROCESS_PROGRAM` are set (+ optional
`REA_GHIDRA_PROCESS_PROJECT_NAME`), the launcher emits
`<projectDir> <name> -process <program> -noanalysis -readOnly ...` instead, omitting
`-deleteProject`. Unset env = stock behavior. Integrity holds: the bridge itself
compares `currentProgram.getExecutableSHA256()` against the session descriptor
(`ReaGhidraBridge.java` ~line 152), and the persistent program's sha256 equals the
target file's (`3b6cfad4...22fc`), verified before reuse. The per-command 194MB target
snapshot copy still runs (per-query byte-exactness preserved).

## Reused project + full-analysis attempt (honest record)

- `/tmp/wrfp_ghidra_proj` (other worker's) verified read-only: sha256 match; 4,021
  functions incl. `CORROB_fname_ctor` 0x23d590, `CORROB_pool_init` 0x23a5d0,
  `CORROB_reserve` 0x246090; no locks; no JVM running. Copied to
  `/tmp/rea_trial_ghidra_proj` (original left untouched).
- Tuned-options pass (Disable `Decompiler Parameter ID`, `Embedded Media`,
  `Embedded File Formats`, `Non-Returning Functions - Discovered`) via Java GhidraScript
  (12.1.4 headless has no PyGhidra Python), then full auto-analysis with 8G heap.
  Ran ~113 min (past the 2G OOM point; jstack verified real progress in
  `ConstantPropagationAnalyzer`), then the background task was **killed externally**
  (not by this worker). Per no-blind-replay rules it was NOT restarted; in-memory
  analysis was lost, DB reverted to the saved pre-kill state. Image-wide caller
  verification therefore remains bounded (see FINDINGS 5.3).
- Bounded substitute: cluster enrichment 0x230000-0x249fff — flow-following
  disassembly from the 31 known FName-cluster entries, then operand references built via
  Ghidra's own `getOpObjects`/rip-relative semantics (`tools/BuildClusterRefs2.java`),
  241 refs from 5,339 cluster instructions, saved. Result: 49 refs to 0x9365ec0.

## Final doctor state

Identical to the table above: all ghidra checks healthy (incl. native decompiler);
hopper + skill/registrations red = acceptable. Overall healthy:false only from those.

## Per-query latency (persistent process mode, `/usr/bin/time -p real`)

| query | result | latency |
|---|---|---|
| `rea function 0x23d590` | dossier + pseudocode (46.7KB) | 7.44 s |
| `rea xrefs 0x9365ec0` | empty (pre-enrichment DB) | 6.91 s |
| `rea xrefs 0x23d590` | empty (pre-enrichment DB) | 6.03 s |
| `rea decompile 0x23a5d0` | pool-init C (61KB) | 7.09 s |
| `rea xrefs 0x9365ec0` (after cluster enrichment) | 49 refs | 13.72 s |
| `rea xrefs 0x23d590` (after) | 2 refs | 10.36 s |
| `rea function 0x23d590` (after) | callers + pseudocode | 10.04 s |

vs import mode on the full binary: 45-min timeout (Session A), 53-min OOM crash
(Session B), 7.3s smoke on a 64KB prefix only.
