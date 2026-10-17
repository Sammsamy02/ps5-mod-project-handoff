# Cap 10 (#10 C# runtime) — Independent Review Verdict + Diagnosis

Reviewer: agent JB (Mac-only review + diagnosis session, 2026-10-13). No PS5
builds, no console contact, no integrator source edits. Everything below was
verified against the actual files in `caps-team-work/cap10/` and
`caps-team-work/mono-host-research/`, by reading source and by executing
Mac-side reproductions where stated.

## Verdict in one paragraph

The #10 stage is real and internally honest: a genuine Roslyn-compiled
Probe.dll, two real matching-Mono host executions on this Mac, and a
well-gated production source stage that correctly refuses drift. The
System.Native interpreter failure is fully diagnosed and FIXED (one-line
environment correction); the interpreter check now passes with a green
receipt. However, nothing here constitutes target execution: the production
owner has never been compiled, the strict loader patch is uncompiled, the
PS5 Mono port does not exist, and the UE4SS integration pin went stale
during this very session (third-party edit at 10:19). Status: HOST-PROVEN,
TARGET-UNPROVEN, INTEGRATION-STALE. Approval is granted for the host
evidence and source quality only; every target claim remains open.

## 1a. Probe.dll compilation — SOUND

- `managed/Probe.cs` (sha256 `6b0ec39b…`) → `managed/Probe.dll` (3,584 bytes,
  sha256 `c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d`).
- Compiled by the pinned Roslyn `csc.dll` from the pinned dotnet SDK 10.0.401
  host tree with `/nostdlib+ /noconfig`, against the Mono-selected .NET
  Framework 4.7.2 reference mscorlib (`dependencies/mono-references/v4.7.2/
  mscorlib.dll`, sha `7bceaed6…`, bound by `dependencies/references-receipt.json`,
  reference submodule commit `e68046d5…`, matching Mono commit
  `0cbf0e290c31adb476f9de0fa44b1d8829affa40`). Not .NET 10 corelib — verified
  in `evidence/COMPILE.json`.
- I independently re-derived the assembly identity at runtime (see §4):
  AssemblyRef is exactly `mscorlib, Version=4.0.0.0, Culture=neutral,
  PublicKeyToken=b77a5c561934e089`, single reference, imports limited to
  `modbridge_cap10_abi_version` / `modbridge_cap10_runtime_probe` via
  `__Internal`. Deterministic flags; PE32/IL-only/anycpu; metadata v4.0.30319.
- The source contract (`tools/assembly_preflight.py`, receipt embedded in
  COMPILE.json) matched on re-run (`tools/test_artifact.py` green).
- Conclusion: compilation provenance is complete and sound. (The preflight's
  `managed_execution_proven:false` flags were accurate at compile time; §1b
  adds the execution evidence.)

## 1b. Host evidence — what each run actually proves

1. `evidence/HOST-CHECK.json` — REAL_PROBE_HOST_PASS on Mac CoreCLR (.NET 10).
   Proves the IL is valid and the P/Invoke + deliberate-exception + recovery
   sequence works on a Microsoft runtime. Explicitly and correctly labeled
   `mono_proof:false`, `townfall_proof:false`, `engine_readiness_proof:false`.
   Not Mono, not engine readiness.
2. `evidence/MONO-HOST-CHECK.json` — REAL_MONO_HELPER_HOST_PASS on the exact
   matching runtime (Mono 6.12.0.206 / 0cbf0e290c3, x86_64 under Rosetta), by
   embedding `libmonosgen-2.0.1.dylib` into a compiled x86_64 host that runs
   the PRODUCTION `InvocationBoundary.cpp` (compiled with
   `-DMODBRIDGE_CAP10_MONO_HOST_VALIDATION=1`) against the actual byte-embedded
   Probe image. Two real Run invocations with fresh cookies, one deliberate
   managed exception verified as exact `InvalidOperationException`, two
   wrong-signature refusals (negative cases), nested public GC-unsafe region
   pair, pinned boxed Int32 readback. Proves: the production boundary helper's
   API usage is correct against genuine matching Mono public APIs. Does NOT
   prove: strict resolver path (stock loader; `__Internal` resolved against the
   harness executable via `-export_dynamic`), interpreter-only execution,
   `MONO_AOT_MODE_INTERP`, any PS5 port, or Townfall. Labels honest.
3. `evidence/MONO-INTERPRETER-CHECK.json` — NEW, produced by this review
   (see §2): REAL_MONO_INTERPRETER_HOST_PASS. The actual Probe.dll loaded and
   executed via `mono-sgen64 --interpreter` (reflection `Invoke`), 2 runs,
   1 exception, native entry/exit/order/cookie/reply observations read back
   from the real test library, input hash checked before/after. Honest flags:
   `native_jit_disabled_proof:false`, `aot_only_mode_proof:false`,
   `strict_resolver_proof:false`, `townfall_proof:false`. This upgrades the
   earlier "no interpreter receipt exists" state to: interpreter-mode CLI
   execution proven on the matching host runtime (still not a strict no-JIT or
   AOT-only proof, and not the production embedding path).

## 1c. Production owner / bridge / loader source quality — APPROVED AS SOURCE

- `native/MonoOwner.cpp/.hpp`: compile-time admission (`__PROSPERO__` +
  `MODBRIDGE_CAP10_ADMITTED_MONO_PORT==1` + required `MODBRIDGE_CAP10_CORLIB_DIR`
  / `MODBRIDGE_CAP10_CONFIG_DIR`) makes accidental host compilation impossible.
  One-shot atomic claim; refuses an existing root domain; owned-name fallback
  provider answers only the two exact symbols; programmatic dllmap binds
  `__Internal` to the reserved `modbridge-owned:cap10-c328dca…` name;
  `EmitOnExit` diagnostics; cleanup order (image close → `mono_jit_cleanup` →
  fallback unregister) is correct and commented (module cache may be closed
  during cleanup; provider/user outlives it inside the same `Lifetime`).
- `native/InvocationBoundary.cpp/.hpp`: static-signature admission via public
  metadata APIs (static flag, default call conv, exact arity/return/param
  types, no byref), exception checked before any use of the raw result,
  volatile raw-return slot + pinned handle pattern, GC-unsafe region nesting,
  name/class/image identity re-checks. Negative refusals were actually
  exercised in the matching-Mono host run.
- `native/observer_fragment.inc`: TLS admission state, monotonic window
  serials, phase machine (abi→probe→done), saturating duplicate counters,
  poisoning on any anomaly, admission scoped to the ready callback with
  save/restore. The exported provider functions observe entry/exit order and
  cookie equality; `NoNativeCalls` windows prove zero native calls during the
  deliberate exception.
- `native/ProbeImage.hpp`: byte-exact embed of the compiled Probe.dll, sha
  cross-checked by `tools/test_production_source.py`
  (`test_embedded_real_artifact_identity` green on re-run).
- Reviewer notes (advisory, not blockers):
  1. `mono_set_dirs(CORLIB_DIR, CONFIG_DIR)` semantics: Mono 6.12's own
     `assembly.c` documents `config_dir` as `PREFIX/etc`-style (the runtime
     appends `mono/config` internally — see §2, this exact semantics caused the
     interpreter failure). The two admission macros must be documented as such
     at integration time or the same off-by-one will resurface on target the
     first time corlib touches global config.
  2. Probe itself never needs System.Native, but corlib internals do (System.IO
     etc.). On the target, a dllmap/`libmono-native` equivalent must exist even
     for incidental corlib init, or the first managed file I/O fails exactly
     like §2.
  3. `catch(...)` scopes are honest (not Mono-fatal/assert/OS-fault catches) —
     keep that wording.
- `tools/stage_current.py` (UE4SSProgram.cpp, 3 anchors) and
  `tools/stage_mono_loader.py` (mono-dl.c, 3 anchors): both are sha-pinned,
  exact-once anchored, inverse-verifiable, apply-nothing stagers. Verified
  mechanically. SEE §5 FOR THE LIVE DRIFT FINDING.

## 1d. Exact gap: "host helper runs" → "PS5 target execution"

All of the following are missing and none may be waved through:

1. A real Mono port for Prospero: threads/TLS/stack bounds, SGen GC suspend +
   conservative stack/register scanning, signal/context/EH layouts (or a
   no-signal interpreter configuration), synchronization/mmap permissions,
   interpreter trampolines and native wrappers under `MONO_AOT_MODE_INTERP`.
   FreeBSD lineage (`mono-threads-freebsd.c` pinned in mono-host-research) is a
   starting point, not a port. No Sony SDK material was used or assumed.
2. `libmono-native` (the `System.Native` implementation library; on macOS
   `libmono-native-compat.dylib` exporting 272 `SystemNative_*` symbols)
   compiled for the target + dllmap, per §2.
3. Deployment of the matching corlib (`lib/mono/4.5/mscorlib.dll`,
   platform-neutral IL — transportable as data) and `etc/mono`-style config
   with `PREFIX/etc` semantics (see §4 for the full dependency chain).
4. The patched Mono loader (§3) compiled into that port — the reserved
   owned-name provider path only exists with this patch.
5. `MODBRIDGE_CAP10_ADMITTED_MONO_PORT=1` + genuine corlib/config dirs + the
   `observer_fragment.inc` stage applied to the integrator's current
   UE4SSProgram.cpp (re-pinned — §5), followed by a full labelled target build.
6. PS5 has no ambient dlopen for plugins: the runtime must be embedded in the
   provider image; the loader patch makes the owned-name path deterministic.
7. In-game proof: Run → Throw → recovery observed through `diagnostic_event`
   receipts in Townfall, under the strict resolver, with no second Mono owner.

## 1e. Final verdict

- Compilation: APPROVED (sound, provenance complete).
- Host evidence: APPROVED as host-only proof (CoreCLR run; matching-Mono
  embedding run; interpreter CLI run — the latter new from this review).
- Production source: APPROVED as uncompiled source stage with the §1c
  advisories.
- Strict production loader execution: NOT PROVEN (uncompiled).
- Target Mono port / dependencies: NOT ESTABLISHED (open prerequisite list §1d).
- Integration: STALE — re-pin required (§5).
- Remaining prerequisites: §1d items 1–7. No claim beyond host proof may be
  made today.

## 2. System.Native diagnosis — ROOT CAUSE FOUND AND FIXED

Symptom (preserved in `evidence/MONO-INTERPRETER-CHECK.FAILED-20261013.log`
and `MONO-INTERPRETER-OUTPUT.FAILED-20261013.log`):
`System.TypeInitializationException` for `Interop+Sys` →
`System.DllNotFoundException: System.Native` at
`Interop+Sys.LChflagsCanSetHiddenFlag()` ← `System.IO.FileSystem.DirectoryExists`
← `FileStream..ctor` ← `File.ReadAllBytes` (harness Main).

What System.Native is here: the Mono 6.12 runtime corlib
(`lib/mono/4.5/mscorlib.dll`, 4,634,112 bytes, CoreFX-derived) P/Invokes the
library name `System.Native` with `SystemNative_*` entry points. It is not a
macOS system library; the MDK implements it in
`lib/libmono-native-compat.0.dylib` (universal i386/x86_64; 272 exported
`SystemNative_*` symbols including `SystemNative_LChflagsCanSetHiddenFlag`;
also covers `System.Net.Security.Native` and, on macOS,
`System.Security.Cryptography.Native.Apple`). Stock
`etc/mono/config` maps it:
`<dllmap dll="System.Native" target="$mono_libdir/libmono-native-compat.dylib" os="!windows" />`.

Why it was missing — precise root cause (verified with `MONO_LOG_MASK=cfg|dll`
reproductions, log preserved at `cap10/tmp/interp-debug-full.log`):
`MONO_CFG_DIR` was set to `…/Versions/6.12.0/etc/mono`, and Mono appends
`mono/config` to the config dir, so it tried to parse
`…/etc/mono/mono/config` (nonexistent) — the global dllmap never loaded, and
the P/Invoke fell through to raw dlopen of `System.Native`/`libSystem.Native.dylib`
variants (all failing). Mono 6.12's own `mono/metadata/assembly.c` documents
the correct semantics: `config_dir: PREFIX/etc`. The env var and the embedding
`mono_set_dirs` argument must both be the `…/etc` directory, one level above
the `mono/` directory. (In the passing embedding run this was masked: the
harness's P/Invokes resolved via `__Internal` against the executable, so no
global config was needed. Advisory: `host-check/MonoHarness.cpp` argv[2] and
`tools/run_mono_host_check.py` still pass `…/etc/mono`; harmless for the saved
receipt's actual assertions, but should be corrected if that harness is ever
extended to corlib file I/O — left untouched to preserve the green receipt.)

The fix: `tools/run_mono_interpreter_check.py` line 10,
`'MONO_CFG_DIR': str(ROOT/'etc/mono')` → `str(ROOT/'etc')` (one-line edit,
applied by this review). Rerun result — PASS:
`evidence/MONO-INTERPRETER-CHECK.json` =
`REAL_MONO_INTERPRETER_HOST_PASS`, 2 real managed runs, 1 managed exception,
`interpreter_cli_requested:true`, honest `strict_resolver_proof:false` /
`townfall_proof:false` flags, assembly sha unchanged (`c328dca1…`), harness
sha `fc85f517…`, native test library sha `40cc95fa…`. The failed receipts are
preserved verbatim under `.FAILED-20261013.log` names; the original exception
was a configuration error, never a Probe/Mono defect, and was not a filter or
policy issue.

Can it pass on the Mac host? YES — demonstrated above. Scope: this proves the
matching runtime's interpreter executes Probe.dll on the host; it is not an
AOT-only/no-JIT proof and not the production embedding resolver.

## 3. Production loader patch (`tools/stage_mono_loader.py` → mono-dl.c)

Assessed against the pinned stock source
`dependencies/mono-source/mono/utils/mono-dl.c`
(sha256 `b7ccf30b3828f0fb64cf07afda22f574f00b6f665b17faeb9f9b7fb3df8f9e8e`;
anchor lines verified in-file). All three changes are CORRECT:

1. Reserved owned-name handling (line 212 context): skips the direct
   `mono_dl_open_file` for any library name containing `modbridge-owned:`.
   Correct: the owned name must never reach the ambient OS loader; the exact
   match is not required because `metadata/loader.c`'s search constructs
   prefixed/suffixed path variants that all still contain the marker.
   `found_name` bookkeeping stays consistent (`lib` remains NULL for owned
   names until a fallback provides it).
2. Missing provider refuses (inserted after the fallback loop, before the
   libtool branch): without it, an unhandled owned name would fall into the
   stock libtool path (`<name>.la` probing + `mono_dl_open_file` on
   constructed variants) — an ambient-file oracle. With it, Mono returns NULL
   deterministically with error "Owned library has no registered provider".
   Correct placement (only when `!lib && !dl_fallback`), correct
   `*error_msg`/`g_free(module)`/`return NULL` style; `strstr` is available
   (`string.h` is included).
3. Fallback unregister head assignment (line 470 context):
   `g_slist_remove (fallback_handlers, handler);` →
   `fallback_handlers = g_slist_remove (fallback_handlers, handler);`.
   This fixes a GENUINE STOCK BUG: `g_slist_remove` returns the new list head
   and does not patch in place; `mono_dl_fallback_register` PREPENDS, so the
   production owner's single handler IS the head when unregistered — stock
   code leaves `fallback_handlers` pointing at the freed node (use-after-free
   on the next `mono_dl_open` fallback walk). The one-line fix is minimal and
   correct. This bug is triggerable by the production owner's own cleanup.

Patch mechanics verified: stock sha pin, three exact-once anchors, exact
inverse, derived sha `0a6c6d86…`, applied:false, artifacts
`native/mono-owned-loader.patch` + `evidence/MONO-LOADER-STAGE.json`;
`test_strict_mono_route_and_unregister_patch_inverse` green.

What compiling the patched Mono for PS5 actually requires: the FULL Mono
6.12.0.206 source tree at commit `0cbf0e290c3` (only 4 .c files are pinned
locally), the Sony Prospero toolchain (host clang + target sysroot; the
project's ps5-payload-sdk is a payload SDK, not a full platform build
environment for a runtime this size), a new platform configuration (there is
no upstream Prospero/ORBIS target; FreeBSD-family `config.h`/thread/EH
plumbing is the starting point per mono-host-research NOTES.md), SGen GC +
signal/suspend strategy that survives the target's restrictions, an
interpreter-only configuration (no JIT; `MONO_AOT_MODE_INTERP` plus whatever
trampolines/wrappers the port needs), static embedding (no ambient dlopen —
which is precisely why the owned-name patch matters), and a target
`libmono-native` build + dllmap for corlib interop (§2). Feasibility in THIS
session: NOT feasible — it is a port project with real unknowns, not a
compile step, and Mac-only/no-PS5-build scope forbids starting it here. The
loader patch itself is ready for that future build.

## 4. Corlib compatibility — CONFIRMED, with the exact chain

Verified at runtime under the matching Mono (reflection receipt, not just
metadata strings):

- Probe.dll references: `mscorlib, Version=4.0.0.0, Culture=neutral,
  PublicKeyToken=b77a5c561934e089` (single reference).
- Matching runtime corlib `…/6.12.0/lib/mono/4.5/mscorlib.dll` presents the
  exact same identity (`mscorlib, 4.0.0.0, b77a5c561934e089`), CLR image
  version v4.0.30319. "4.5" is the Mono profile directory; the compile-time
  4.7.2 reference assemblies share the same assembly identity, so binding is
  exact. Compatibility is additionally proven EMPIRICALLY: the same Probe.dll
  bytes loaded and executed against this corlib in three real modes (CoreCLR
  host run; Mono embedding run; Mono `--interpreter` run). Scope note: this is
  compatibility for the APIs Probe actually uses (P/Invoke, InvalidOperationException,
  boxed Int32) — not a blanket 4.7.2⊂4.5 API guarantee; Probe's tiny surface
  makes that sufficient.

Exact PS5 deployment dependency chain (what must ship with the future owner):

1. `Probe.dll` bytes (embedded via `ProbeImage.hpp`; sha `c328dca1…`).
2. Ported `libmonosgen-2.0` (embedding engine, commit `0cbf0e290c3`) WITH the
   §3 mono-dl.c patch — provides mono_jit_init_version, image/assembly load,
   runtime_invoke, gchandle, GC-unsafe region APIs, dllmap, dl-fallback.
3. Runtime corlib `lib/mono/4.5/mscorlib.dll` (platform-neutral IL; deploy as
   data at the admitted `MODBRIDGE_CAP10_CORLIB_DIR`-derived location).
4. Target `libmono-native` (the System.Native implementation; 272
   `SystemNative_*` entry points on the Mac build) + dllmap
   `<dllmap dll="System.Native" target="…"/>` — required for corlib internals
   even though Probe never calls them directly (§2 failure mode otherwise).
5. Config tree with `PREFIX/etc` semantics (`…/etc/mono/config` + 4.5
   `machine.config` etc. as needed) at the admitted config dir.
6. On the Mac host only: `bin/mono-sgen64` + `lib/libmonosgen-2.0.1.dylib`
   (i386/x86_64, Rosetta) + `lib/libmono-native-compat.0.dylib` — these are
   host-proof artifacts and DO NOT transfer to the target.

## 5. Live drift finding (new, session-dated)

While this review ran, the integrator's
`what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp`
changed (mtime 2026-10-13 10:19, current sha256 `ad3ae621a7c86d771f4ed56e54eddf3330b8db6a1525c5a73a8466a6c57078cf`
≠ pinned `fe9fe7bc…`). `tools/test_production_source.py::test_current_source_transform_and_inverse`
now correctly REFUSES ("current original hash drift/reapplication") — the
fail-closed design works. I verified all three cap10 anchors still occur
exactly once in the drifted file and no Cap10 content is present, so a
one-line re-pin + fresh derived sha is mechanically possible. (Agent JD's
integration report records the same drift independently.) Consequence:
`evidence/CURRENT-STAGE.json` and `native/current-owner.patch` are STALE
relative to the live tree; re-stage and re-review before any apply. No apply
was attempted. The `evidence/PRODUCTION-SOURCE-GREEN.log` predates the drift.

## 6. Artifacts added by this review

- `evidence/MONO-INTERPRETER-CHECK.json` (new green receipt) and
  `evidence/MONO-INTERPRETER-CHECK.log` (new passing run output).
- `evidence/MONO-INTERPRETER-CHECK.FAILED-20261013.log`,
  `evidence/MONO-INTERPRETER-OUTPUT.FAILED-20261013.log` (preserved originals).
- `tools/run_mono_interpreter_check.py` — one-line `MONO_CFG_DIR` fix
  (`…/etc/mono` → `…/etc`).
- `tmp/interp-debug-full.log` (dll-mask debug reproduction),
  `tmp/Refs.cs`/`tmp/Refs.exe` (corlib identity check).
- This `REVIEW-VERDICT.md`.

Review ends. Mac-only; no builds, console, uploads, or integrator edits.
