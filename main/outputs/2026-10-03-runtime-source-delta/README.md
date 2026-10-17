# UE4SS source integration delta

Apply this delta to the verified full runtime handoff at commit `eed2ebe4b89ac8650325bb76251f57a895f5ecd1`. It contains source, build recipes, dependency provenance and focused verification records. It does not contain a newly runnable PRX, private account material or game captures.

## What changed

Claude's delivered `modforge_activation` C code now supplies owned, preloaded mod files to the actual UE4SS Program, settings and LuaMod path. The integration retains the activation buffers for as long as Lua readers need them. Entry scripts, `require`, `loadfile` and `dofile` can use that retained source without reopening their original files. This is a generic runtime handler; the mod scripts are not rewritten.

The native build reconstructs the selected source tree and recompiles the real UE4SS code against coherent headers. It retains the earlier native layout, callback lifetime and game-thread corrections. C++ mod discovery recognizes native module filenames, and the genuine `CppUserModBase` implementation is included. The example C++ mod is a compiled source example, not a runnable module or a Windows DLL translator.

The separate app metadata work is on `codex/app-service-2026-10-03`, commit `7c974743b99a369578611b76aea88975ac0c9c83`. It is not mixed into this archive.

## Restore

1. Keep an untouched copy of the verified original handoff. Work in a separate directory.
2. Verify `runtime-source-delta.zip` using `ARCHIVE.json`, then check every ZIP member against `MANIFEST.json`.
3. Overlay the listed source files into the restored repository root. A non-null `baseline_sha256` identifies the original file being replaced. If a local file differs from both baseline and new hash, preserve it and reconcile the changes instead of overwriting it.
4. Read `.superpowers/native-dependency-spike/native-coherent-runtime-1/REBUILD.md` and `FIXED_DEPS.md`. Rebuild outputs go into new directories; frozen outputs are not replaced.

The archive includes every changed or previously omitted source origin required by the coherent snapshot and its recorded external headers. It also includes the source/recipe closure for the previously omitted Rust resolver, nine small helper objects and modified libc. All eleven dependencies were rebuilt locally with their exact pinned output hashes. Host compiler executables, the Rust sysroot and offline Cargo cache remain explicit prerequisites; a Linux cloud rebuild has not been verified.

## Evidence boundary

The existing frozen console result demonstrated module startup, constructors and a buffered pure-Lua test. This delta has host tests, PS5-target compilation and a local relocatable runtime link. It has not demonstrated native engine initialization or Item Spawner in Townfall. The remaining game-side work is the real loader-context supplier, validated target layouts, game-thread/reflection integration and the in-game mod result. No universal compatibility percentage or binary DLL conversion is claimed.
