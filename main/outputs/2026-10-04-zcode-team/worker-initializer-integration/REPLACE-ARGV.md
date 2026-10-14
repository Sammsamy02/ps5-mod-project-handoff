# REPLACE-ARGV — the --replace-runtime-unit invocation for the merged UnrealInitializer

Worker-initializer-integration, 2026-10-04. The mechanism is the existing
`--replace-runtime-unit` flag of the fixed composition script (S =
`worker-composition-4-fix/build_runtime_composition4_fixed.py`; `unreal-unrealinitializer`
is in the affected fresh-unit inventory and NOT in the staged-default
`new_sources` set, so it is replaceable — REPLACE-MECHANICS §1 verdict table).

## The one-line delta vs the run-3 / composition-5 APPLY-ORDER argv

Keep the proven Phase-A argv verbatim and add/redirect exactly these two flags:

```
  --replace-runtime-unit unreal-unrealinitializer=outputs/2026-10-04-zcode-team/worker-initializer-integration/staging/UnrealInitializer.cpp \
  --replace-receipt outputs/2026-10-04-zcode-team/worker-initializer-integration/receipts/replace-receipt.json \
```

(Replace any earlier `--replace-runtime-unit unreal-unrealinitializer=…` line
pointing at the s2-arm staging copy; `--extra-*`/`--replace-bootstrap-*`/receipts/defines
stay verbatim per REPLACE-MECHANICS §2 Procedure A. Fresh run dir required — S
refuses an existing one.)

## Mechanism facts (verified against S)

- `unreal-unrealinitializer` ∈ affected inventory (S:61); not in
  `new_sources` (S:75 lists only ownedmappedscansource/loadercontext/
  programfactory/ownedserializedelfsource) → the S:83 refusal does not fire.
- `--replace-receipt` is mandatory for runtime replacement (S: `if rt_repl and
  not a.replace_receipt: raise`). The delivered receipt covers the replacement
  source AND the two staging-local headers (the fresh-unit dep gate resolves
  new dependencies through the receipt's `files` map).
- The unit compiles with the BASE receipt's recorded argv for
  `unreal-unrealinitializer` (190 args, prospero-clang++, -std=c++23, -O0,
  `-I W/include -I tick-witness-1/include` prepended by S) with only the
  source/-o/-MF paths redirected — exactly what the compile-gate receipt ran.

## Compile-gate receipt (host-executed, exact composition argv)

- Compiler: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
  (LLVM_CONFIG=`outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`,
  as recorded in the run-3 unit entry).
- Result: exit 0, zero errors, zero warnings from the merged files
  (`receipts/compile-gate.stderr`).
- Object: `receipts/compile-gate/unreal-unrealinitializer.o`
  sha256 `fe639a859b2f33d1ecc4d000e9e1980c7b53413269acbb4ffe823e973dcc496f`.
- Control experiment: the unmodified s2-arm staged source compiled with the
  same argv reproduces run-3's object BIT-EXACTLY (1,859,160 bytes, 0/2484
  extern-symbol delta) — the tree is reproducible, so the delta below is
  attributable to the merge alone.

## Shape A compiled state (the landed {0,2,4} table)

`SeedTable.h` has `kRequiredSetComplete=false`; the constants fold, so the
compiled `ScanGame` IS the named refusal (`[Seeds] Required-slot seed table
incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal]
missing [gmalloc] …`) — no seed binds, no scan, no vtable walk, no detour
(LAUNCH9-PREDICTION §2-I3/§4-#2). Verified in the object: ScanGame = 0x4a4
bytes, no DoScan lambdas, no `ps_scan` import; 296 extern symbols from the
unreachable tail drop out; object 1,676,944 bytes vs control 1,859,160.

## Composition gate impact (simulated exactly, using run-3's exports)

- Dropped initializer imports (`ps_scan`, `ScanForGameFunctionsAndData`,
  `ScanForGUObjectArray`, `SetupGUObjectArrayAddress`,
  `SetupStaticConstructObjectInternalAddress`, query-harness, wide-string
  helpers, …): **every one is DEFINED by another composed unit** → the
  composed `ld -r` object's undefined set loses nothing attributable.
- Added initializer imports: only `fmt::vformat<char>` — resolved by the
  composed fmt unit.
- **Composed undefined-delta vs the notify-cancel base = `[added:
  sceKernelGetModuleInfo]`, removed: [], unchanged bindings — bit-identical to
  run-3's own delta** (`baseline_extended: true` carries over). The S:167-179
  gate passes with the existing argv.
- Defined-symbol delta vs control: 0 lost, +5 externs (SeedTable constants and
  new Output::send instantiations in reachable code).

When a future complete table (gmalloc derived) lands: regenerate SeedTable.h
(dialect or canonical — verified identical), update replace-receipt.json
hashes, recompose. The same source then compiles the full armed path (verified
interim: full ScanGame 0xad7 with all 17 lambdas, +75 externs, composed delta
still identical to run-3).
