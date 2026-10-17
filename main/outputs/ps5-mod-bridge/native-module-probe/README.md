# Native module log probe

Local experiment for the module-loading milestone. It contains no UE4SS, engine
scanning, hooks, injector, network client or game-file writer. The SDK-only v2 successor has confirmed `module_start` execution and a log write
inside Townfall on the user-reported OnionHEN 13.60 setup; see the current result below.

## Current confirmed result — October 3, 2026

The user ran Ploader Return Code with the SDK-only v2 successor and reported
**OK 126ms**. Ordinary FTP then read the previously absent `/data/townfall_mod.log`:

```text
modbridge-log-probe/v2 build=2db853878acd4965a8d8629b117a469e title=PPSA33286 module_start
```

The installed 5,757-byte PRX matched SHA256
`ace9d0a013f5f48d2083f9d79a70a4f601eddd29e3f655ff7fa2326c3a402e0d`.
This successor changes only the SDK field and its four mirrors from `0x02000009`
to package-declared `0x11000000`, plus the regenerated wrapper digest. The original
v2 build remains preserved and previously returned `0x80020063` (ESDKVERSION).
The builder below retains its historical defaults; it does not itself include
that later SDK metadata adjustment.

The record confirms this probe's native startup and file write. No `init_array`
line was present. That does not establish constructor behavior for other modules,
full UE4SS initialization, or access after Ploader restores the sandbox.
The next import-table probe must explicitly own constructor/finalizer handling.
Evidence: `.superpowers/native-observations/sdk-version-probe-user-ok-60f346e25c714b70bab9fc8f0010d036/receipt.json`.

## Two distinct startup paths

The probe records a build-specific line for each entry:

- `init_array`: one explicit retained `.init_array` entry.
- `module_start`: an exported function also bound to the PRX's `DT_INIT`.

Both use the same bounded append routine and only three system imports:
`sceKernelOpen`, `sceKernelWrite` and `sceKernelClose`, assigned to `libkernel.prx`.
Each opens the profile's log in append mode, writes its own tagged line, closes
and returns. `module_start` returns 0 as requested. The optional
`modbridge_probe_result` export reports the last completed append's status:
1 initially, 0 for success, 2 for open failure, 3 for write failure, 4 for close
failure. A loader OK alone does not establish that the log write succeeded.

The PS5 compiler's constructor attribute emits `.ctors`, which the frozen
probe tool does not handle. The explicit `.init_array` entry fixes that mismatch.
A separate full-runtime packager successor now handles SDK-style legacy arrays;
this probe and its already inspected artifact remain unchanged. Exporting
`module_start` alone would not connect startup: the original writer emits a
return-only `DT_INIT`. The local tool correction adds an explicit library-init
symbol, preserving the array and ELF entry-point defaults. Loader invocation of
these recorded entry points still requires hardware evidence.

`profiles/townfall.json` holds game-specific facts: title **PPSA33286**, observed
content version **02.003.000**, log path and module path. Shared C code contains
no game-specific paths or identifiers.

## Local build

Compile with PS5 Payload SDK v0.43. Use the locally corrected public
[SharpProspero PRX writer](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/docs/modules.md),
based on commit `5dfdb4b0b166e83dc9ae275001afb695f12c7174`. The unmodified writer
does not accept `--library-init` and has a library parameter-relocation defect.
The derived tool's patches are in `toolchain/`; its build records are saved
separately from the original pinned source.

```sh
python3 build.py \
  --sdk /absolute/path/to/ps5-payload-sdk \
  --llvm-config /absolute/path/to/llvm-config \
  --dotnet /absolute/path/to/dotnet \
  --tool /absolute/path/to/corrected/sharpprospero-bindgen.dll
```

Every invocation creates a fresh directory under `build/`, retaining exact
commands, compiler/linker/inspector logs, expected tagged lines and `result.json`.
The root `modbridge_log_probe.prx` is a plaintext fake SELF made by the SDK's
`make_fself.py`, with no custom authentication info. Its first four bytes must
be `4f 15 3d 1d`. The original ELF is retained under `bare/`, beginning
`7f 45 4c 46`, with type `ET_SCE_DYNAMIC`. A container inspection and extraction
check compare every loadable segment with the original ELF. The wrapper omits
ELF section tables and padding; extraction is not a whole-file hash match.
Neither format
inspection nor the fake wrapper supplies a Sony signature or hardware acceptance.

## Loader evidence and interpretation

The published [13.60 port's pinned kstuff-lite inputs](https://github.com/GronedWaffel/etahen-13.60/blob/f59d709228954918e61e8e1b7e64d4b70b24be2f/KSTUFF-SHADOWMOUNT-COMPATIBILITY.md)
lead to a [FSELF parser](https://github.com/EchoStretch/kstuff-lite/blob/85fb88a526e58e357d6f1e626ce05af4acdb8b5d/ps5-kstuff/uelf/fself.c)
that expects SELF entries, an embedded ELF and program type 1. That supports
preparing this wrapper; it does not demonstrate native-game loading on 13.60.
A non-ELF header alone is not sufficient format validation.

The generated `ploader.ini.example` is a local template, not an installed
configuration. This project does not bundle or launch Ploader or etaHEN.
[Ploader v1.1.0](https://github.com/yourphonelinging0x0/ps5-plugin-loader/tree/1409a94aa6a5851acde72f85d3a1f6ae64a1b0a6)
documents firmware 11.60/12.20 and PS4-game testing; native PS5 games and 13.60
remain unverified. Mode 2 uses timing/module-count heuristics, not an engine-ready
callback. The unofficial port also leaves plugins and the older `/data` sandbox
patch unverified.

| Observed result | What it establishes |
| --- | --- |
| FAIL, no fresh marker | No successful probe execution established; this does not identify the failing layer. |
| OK, no fresh marker | Loader report only; initialization or log-path access can still have failed. |
| Fresh `init_array` line | That entry reached its append. |
| Fresh `module_start` line | That entry reached its append. |
| Both fresh tagged lines | Both entries reached their appends. |

Associate results with the exact new build marker and intended process. The log
alone cannot independently identify which process loaded the module. Old logs,
compilation, a module handle or a loader notification do not establish Townfall
execution or UE4SS compatibility. This probe intentionally allows both startup
paths to log; the full runtime would need initialization once, after its actual
engine prerequisites are satisfied.

No game-module replacement, executable patch or process-memory operation is
part of this local build. Original captures and stage27 remain unchanged.

## Installed v2 artifact and reported result

Build `2db853878acd4965a8d8629b117a469e` passed compilation, PRX linkage, container
inspection and loadable-segment round trip. The root PRX is 5,757 bytes, SHA256
`ccb9085c9eb69e2d2bd7294e2666d6b2acbb6aa422e0e7572762f708053c20ce`.
`DT_INIT` names `module_start` at relative address `0x20`; the sole array entry
at `0x8030` relocates to the distinct callback at `0xe0`. The artifact has three
libkernel imports and nine relocations with no duplicate destinations.
Exact commands and expected lines are beside its `result.json`. The earlier
single-entry bare-ELF build is retained as superseded evidence.

The user reports OnionHEN 13.60. The initial standalone Ploader “no .prx
loaded” and later “config empty” reports were traced to a missing configuration.
Ordinary FTP then promoted the already verified owned probe to its final name
and installed the matching single-title config. Both raw readbacks match the
handed-off files. No loader or game was started by this placement. The new local handoff directory
contains unchanged module bytes, the single-title config, expected tagged lines
and a ZIP. Creating that bundle did not contact or modify the console.

The subsequent user report was “Injecting 1 plugin(s)” then **“OK.”** The saved
FTP check after that report still returned `550 No such file or directory` for
`/data/townfall_mod.log`; the saved sandbox listing had no PPSA33286 mount.
That historical observation was the table's “OK, no fresh marker” case; the user
later questioned that early OK report. The confirmed SDK-only result above supersedes it.
Neither a failed startup nor a successful entry can be concluded from it.

Upstream OnionHEN documentation lists bundled kstuff for fSELF/fPKG with a Toolbox
switch. The exact installed 13.60 fork/build has not been identified; this general
feature statement does not establish native Townfall module acceptance. Earlier
etaHEN-specific source evidence above is historical.

## Local v3 visible-entry successor

`handoff/9ee3214808cb4d3d9b22ff268b4f41c0/` contains a separate 6,845-byte probe
and its ZIP; SHA256
`1a172a6e42b986ba0e9b5a37f4c8695ad489b1227940ec511925eec8061f2a55`.
It adds `sceKernelSendNotificationRequest` as its fourth explicit libkernel
import. Each entry posts its own build-tagged notification before trying the
file append, then reports the exact operation and signed error return if an
open, write or close fails. `module_start` still returns 0.

Three focused host control groups and the native compile/link/wrapper round-trip
pass. The v2 sources and installed artifact are unchanged. The v3 source/build
lives separately in `.superpowers/native-dependency-spike/native-log-probe-visible-1`.
It has not been installed or run. Console replacement waits for known-closed game
state. A v3 notification would provide separate entry evidence; absence of one
would still be ambiguous because notification delivery itself can fail.
