# Minimal owner/Probe dependency closure — Mac evidence, 2026-10-13

The actual minimal owner flow **does not load or request `System.Native` / `libmono-native` in the executed Mac test**. A complete Mono.Native PAL should not be imposed as an already-proven prerequisite for the first tiny Townfall Probe solely because the separate file-I/O regression used it. The genuine target runtime's own unresolved native dependencies still must be closed; this report does not waive them.

## One actual dependency diagnostic

The existing, frozen `mono-owner-strict` executable was run once with Mono DLL tracing and dyld library/API tracing. No source or binary was rebuilt. Its executable, runtime dylib, real generated config, corlib and corlib AOT hashes were checked before and after; all match the approved strict-owner evidence. Output and new receipts were written only under `cap10-native-deps`.

The real result remained `REAL_MONO_OWNER_STRICT_HOST_PASS`: stage 7; two managed runs; one exact exception; Run, throw, recovery and cleanup successful; clean diagnostics. See `evidence/OWNER-DEPENDENCY-TRACE.json` and the two complete raw logs.

| Observed dependency | Result |
|---|---|
| Managed native-library requests | One: `modbridge-owned:cap10-c328dca11447b2c41ab1580582b957691` |
| Managed imported symbols | `modbridge_cap10_abi_version`, `modbridge_cap10_runtime_probe` |
| Local native images mapped | Owner executable, patched Mono runtime dylib, full/interp `mscorlib.dll.dylib` |
| `System.Native`, `libmono-native`, `SystemNative_*` | No requests, loads or symbol lookups in either trace |
| `MonoPosixHelper`, `libmono-btls` | No requests or loads in either trace |
| Probe AOT image | Ordinary AOT sidecar searches failed; the Probe still completed through the interpreter |
| Corlib AOT image | Loaded; real AOT metadata and trampoline symbols resolved |
| Native runtime direct OS work | Present; includes a Darwin-only libc/`clonefile` initialization probe |

The complete dyld image trace also contains macOS/Rosetta's transitive framework dependencies. Those are **not** a target dependency prescription. Loader tracing establishes the executed native-library surface, not every native system call. The scope is this exact owner, this exact small Probe, and this host configuration.

The existing harness and runtime have no `SystemNative_*` exports, and their Mach-O dependency tables do not link `libmono-native`. This corroborates that the missing trace entry is not hiding a statically bundled implementation. `evidence/HOST-LINK-DEPENDENCIES.json` records the actual `otool` and exported-symbol observations.

## Why configuration does not make the whole PAL mandatory

The owner calls `mono_config_parse(nullptr)` so the package's real global dllmap is available. In pinned `mono-config.c`, `dllmap_start` records strings with `mono_dllmap_insert_internal`; it does not load the mapped libraries. `mono_native_getrootdir` only builds a path string. The actual P/Invoke library probe occurs later in `native-library.c`, and that path emits the observed DLL trace.

The map from `System.Native` to a Mono.Native dylib therefore does not itself load it during initialization. The observed registered-provider search has expected debug rejections for constructed path/prefix/suffix names, followed by successful exact-name resolution. Those messages are not failed owner execution.

## The earlier System.Native path belongs to managed file I/O

`Probe.cs` only performs its two owned native imports, integer checks, and `InvalidOperationException`. It does not read a file. The actual owner opens the compiled Probe from `ProbeImage` bytes.

The separate host-only `ConfigProbe.ReadByteCount` deliberately invokes `File.ReadAllBytes`. The earlier managed interpreter wrapper also read the DLL from a path to hash its input. An existing preserved failure log, `cap10/evidence/MONO-INTERPRETER-CHECK.FAILED-20261013.log`, records the real path:

`File.ReadAllBytes → FileStream constructor → Directory.Exists → FileSystem.DirectoryExists/FileExists → Interop.Sys initializer → LChflagsCanSetHiddenFlag → System.Native`.

The pinned Mono/CoreFX source corroborates this path: corlib selects CoreFX `File.cs`, which creates Mono's `FileStream`; that constructor checks `Directory.Exists`; the `Sys` initializer imports `SystemNative_LChflagsCanSetHiddenFlag`. Thus the native dependency observed there comes from the added managed file-I/O path, not from the tiny embedded Probe's own two imports. No second diagnostic or file-I/O rerun was performed in this task. Source excerpts, original failure evidence and exact hashes are preserved in `evidence/PINNED-SOURCE-CORROBORATION.json`.

## First-test integration scope

For the first tiny owner/Probe test, prioritize:

1. The real target Mono native core with SGen/interpreter, its required thread/TLS/stack/signal/context behavior, and a truthful link/provider closure for the selected runtime objects.
2. The matching corlib bytes and genuinely target-compatible full/interp AOT/trampoline path. The successful Darwin corlib AOT is a host artifact only.
3. The exact embedded Probe bytes, strict owned-library patch, same-image two-export provider, actual production owner/boundary, and genuine readiness/call observation in the game.
4. Real package prefixes/configuration and the native file/loading support actually needed for corlib/config/AOT, or an independently proven static registration design. Configuration text parsing may remain even when optional mapped libraries are absent.

Keep the `File.ReadAllBytes` regression outside that tiny embedded-Probe acceptance path. Add Mono.Native/System.Native capabilities when the tested managed plugin starts using APIs that need them, such as the demonstrated filesystem route; then validate those APIs explicitly. General C# plugin compatibility and Unity integration require broader evidence later.

This narrows the optional managed PAL work. It does not justify empty API stubs, fake successful OS operations, omission of real runtime native requirements, or a target-readiness claim.

## Target archive inventory status

The target-port worker was notified and supplied the prospective runtime archive path, `cap10-port-closure/target-work/mono/mini/.libs/libmonosgen-2.0.a`. It was still building at this report's preparation. No final archive existed to bind an honest unresolved-symbol inventory, so no target dependency count is inferred from host symbols or partial archives.

Once the actual archive is complete, inventory unresolved symbols from that exact hash. Distinguish archive-wide external references from members retained by the owner/corlib link; optional archive members can overstate the minimal link surface. The final target link and real execution remain authoritative. This pending native inventory does not change the direct result above: System.Native was absent from the successful measured owner flow.
