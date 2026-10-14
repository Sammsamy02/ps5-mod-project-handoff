# UE4SS native source-port checkpoint — updated October 3, 2026

**The full native C++ constructor sequence and buffered Lua diagnostic now run inside Townfall.** All 232 constructors returned successfully; a worker executed the owned Lua script after its delay and validated its marker plus integer 136. This is not yet UE4SS engine integration or Item Spawner: Unreal object mappings, game-thread calls and game-facing Lua APIs remain unproved. The user-directed order remains UE4SS, then DLL compatibility, then the PS5 application; Mod Engine is deferred.

## Latest native result: startup and buffered Lua passed

User reported **Ploader OK 1288 ms**. The 66,052-byte log contains exactly one build
marker and 232 constructor starts/zero returns (indices 0–231), successful
`module_start`, worker creation/entry, completed delayed sleep, Lua load/call,
expected-result match, state cleanup and buffered-Lua success. The installed
module/config/input readbacks match their recorded bytes.

The incoming stack alignment measured0 modulo 16; our module normalized its
call stack. This adjustment resolved the prior first-vector-insertion startup
failure in this run. It does not establish a specific crashing instruction in
the old build or general compatibility with every loader.

Evidence: `.superpowers/native-observations/aligned-entry-result-cf3194a85f4048219f2238fb0f04d157/interpretation.json`.
The successful `/data/plugins/modbridge_runtime_aligned_entry.prx` remains in
mode 2, log `/data/townfall_runtime_aligned_entry.log`. Preserve both; no repeat
of this completed milestone is needed.

## Current next work

The tested startup primitives have been preserved as a reusable source-only bundle
at `outputs/ps5-mod-bridge/native-runtime-startup/`. All ten code files match
the proven candidate; five translation units passed focused PS5 SDK compilation.
The installed module and frozen build were not changed.

Carry the verified owned-entry alignment adjustment into the reusable runtime
startup source. Inspect the source-grounded next UE4SS bootstrap/engine-backend
step. Review Claude's `claude/confident-albattani-q783zj` branch load-list and
installer interfaces without merging or broadening support claims. Its reported
host bundle preparation remains intermediate work; final planning/adaptation
must run on PS5 to meet the product requirement.

## Previous failure: first callback queue insertion

The registration diagnostic returned FAIL 863 ms while Townfall stayed playable.
It logged false initialization, empty queue, nonnull callback and before-insert,
but no after-insert marker. The aligned-entry build above subsequently passed
this point and every remaining compiled constructor.

## Prior console result: full module entered; stopped in constructor 11

The user reported FAIL with an all-F hexadecimal code (exact width unconfirmed)
and approximately 900 ms; Townfall kept running. The installed module, input and
configuration match the staged bytes. The 3,926-byte native log confirms
`module_start` entered, the 187 carried imports were nonzero, the owned 88-byte
input loaded, and constructor indices 0 through 10 returned zero. The last line
is `constructor_begin` for index 11, with no matching return. No worker or Lua
execution has been observed from this full module.

This locates the failure after loading, during native global initialization. The
loader's generic -1 does not identify the cause. Local mapping of slot 11 to its
source and package relocations is in progress. No unchanged retest is needed;
the successor above was subsequently staged after a fresh closed-game confirmation.

Evidence: `.superpowers/native-observations/buffered-lua-result-a295b2375a424fe19e308beeba55d592/interpretation.json`.

## Prior staged build: full runtime constructors and buffered Lua

The 30,150,311-byte composed module is now installed in mode 2 at
`/data/plugins/modbridge_runtime_buffered_lua.prx`, with an exact 88-byte owned
Lua input at `/data/townfall_worker_input.lua`. Placement/readback passed;
previous worker probe, log and original Ploader configuration are preserved.
Final module SHA256: `699d8d7dbe59a7ad4047e7b747093c3239ba67a98db6e2bf2d1af4e70dfdd87f`.

This starts all 232 compiled constructor entries, then a delayed worker using
startup-buffered input and a retained log descriptor. It checks a pure Lua marker
and sum (136), with no UE4SS engine initialization or game APIs. Local compilation,
package checks and independent review passed; native execution is not yet known.
The user should run **Ploader Return Code**, launch Townfall, leave it open for
30 seconds and report OK/FAIL/rc. Read `/data/townfall_runtime_buffered_lua.log`.
No automatic loader dispatch or game launch was performed.

Evidence: `.superpowers/native-observations/buffered-lua-placement-84d4d5a996274fca943500792cb6973b/receipt.json`.
Source/report: `.superpowers/native-dependency-spike/native-full-runtime-buffered-lua-1/REPORT.md`.

## Prior native result: worker runs; retained logging works; both reopen paths fail

User reported **Ploader OK 146ms**. The installed 45,943-byte worker probe and
mode-2 configuration matched their expected hashes. Its 2,964-byte log confirms
all 187 original import slots, both diagnostic constructors, worker entry and
successful module startup. `nanosleep` first returned EINTR, then completed.
The worker wrote its post-sleep results through the log handle opened at startup.

The absolute POSIX `open()` of the same log returned -1 with **ENOENT (2)**.
`_openat()` relative to the `/data` handle retained from startup returned -1 with
**EPERM (1)**. There is no successful reopen/write marker for either method.
This establishes the exact observed failures; it does not prove every path or
file operation fails. It explains why the earlier fresh-open logger could hide
worker progress, without showing an earlier worker failure.

The next local preparation loads owned input bytes during startup and keeps the
logging handle. C++ lifecycle cleanup/unwind remains a separate full-runtime
requirement. No full runtime or Item Spawner has run inside Townfall.

Evidence:
`.superpowers/native-observations/worker-channel-result-39046b2efa564f389a2c920d02c2aa4e/interpretation.json`.
The successful worker probe is preserved and disabled while its successor is tested.

## Prior result: imports and constructors passed; delayed worker unresolved

The corrected middle probe returned **OK 97ms**. Its native log confirms all
187 original runtime import slots are nonzero, both diagnostic constructors ran
in order, `pthread_create` returned 0 and `module_start` returned 0. This validates
import resolution and selected startup calls, not every imported function's ABI.
The sysctl correction changes only its library identity from libScePosix to
libkernel and is retained in the reusable full-runtime contract.

The user initially closed Townfall after OK. A repeat then left the game open
for 30 seconds and appended the same successful startup sequence, but neither
`worker_after_delay` nor `worker_sleep_error` appeared. Early closure no longer
explains the repeated absence. The next probe keeps a log descriptor opened at
startup and records worker entry, sleep result and a separate delayed path-open
attempt, so an inaccessible log path cannot silently hide all worker diagnostics.
The user has now closed Townfall and left FTP on for that successor.

Repeat evidence:
`.superpowers/native-observations/sysctl-corrected-user-ok-3c32048c985444e68e980bb30699f9e4/repeat-held-open-result.json`.
The current installed module remains the corrected import probe, SHA256
`1906cc83bf98b4174fa54d3a17dcc79bff848a3aac8b4110ef0d4fc3d4cf6740`.
No full runtime has been installed or started in Townfall.

The actual full runtime also packaged locally as a 30,145,672-byte bare PRX with
190 imports. SDK finalization to `0x11000000` and SELF wrapping passed local loadable-segment
roundtrip checks. Its explicit constructor array contains 232 entries and its
finalizer array two entries. It stays offline while
C++ destructor registration/cleanup, partial construction handling and the worker
channel are addressed. Engine mapping and Item Spawner remain unproved.

## Prior middle-probe result: ESTART, instrumented successor


The full187-import middle probe returned user-reported **FAIL85ms,
rc=0x80020064 (ESTART)**; the game remained playable. FTP verified its exact
installed bytes and found no expected log. Its validation preceded logging,
so the original diagnostic could not identify an early failing import.

The user closed Townfall. A successor is now installed in mode 2 at
`/data/plugins/modbridge_runtime_import_diagnostics.prx`. It preserves the187
runtime declarations and adds the three kernel logging imports already proven
by the SDK-only probe. Its logger checks actual call bindings independently of
the carried pointer table. It logs the first missing index/name/module/library,
missing count, constructor stages and exact worker/start return values.
SDK remains `0x11000000`; old probes are preserved and disabled. Final wrapper
SHA256: `c1c5ad33ab43757153c2e0e603b709c330dcd2cd1bfc25a4f64f30aa865fb5ff`.
Focused checks, independent review and raw placement readback passed.

Await **Ploader Return Code**, then Townfall left open for at least ten seconds.
The owned log is `/data/townfall_runtime_import_diagnostics.log`.
Details: `.superpowers/native-dependency-spike/native-runtime-import-diagnostics-1/REPORT.md`.
This is diagnostic instrumentation, not a claimed runtime fix.

## October 3 return: confirmed log-probe entry and complete local import declarations

**Confirmed: the exact SDK-only native log probe loaded, reached
`module_start`, and wrote its marker.** The user reported **Ploader OK 126ms**
and then closed Townfall. Ordinary FTP found a 91-byte `/data/townfall_mod.log`
containing exactly:

```text
modbridge-log-probe/v2 build=2db853878acd4965a8d8629b117a469e title=PPSA33286 module_start\n
```

The installed 5,757-byte candidate still matches SHA256
`ace9d0a013f5f48d2083f9d79a70a4f601eddd29e3f655ff7fa2326c3a402e0d`.
Evidence is in
`.superpowers/native-observations/sdk-version-probe-user-ok-60f346e25c714b70bab9fc8f0010d036/receipt.json`.
Its SDK declaration is `0x11000000` at MODULEPARAM+0x14 and four version mirrors;
all other ELF bytes are identical to v2, and the original wrapper/options remain.
At that milestone, original v2 was preserved and its SDK variant was the sole mode-2 entry; original game files were unchanged.

The log contains one `module_start` marker and **no init-array marker**. That
absence does not prove constructors never run. This confirms the native log-only
module's load/start/file write for this exact variant and setup; it establishes
neither general acceptance of all SDK versions nor full UE4SS/engine readiness.
The earlier uncertain “OK” reply remains historical and uncorroborated; this new
milestone has separate file evidence. Townfall is now closed by user report.

No further test of the successful log-only probe is needed. The next middle probe
is now built and installed separately as `/data/plugins/modbridge_runtime_import_probe.prx`
in mode 2; the successful SDK-only probe is preserved and disabled in the config.
The new 23,735-byte wrapper has SHA256
`b3fa9d5eb7a50819d2b6055596c6f88dc9f0507136e522ba68945b6d097b522f`.
It carries all 187 strong runtime imports (183 FUNC, four OBJECT), keeps six weak
references null, explicitly calls constructor arrays at startup and reverse
finalizers after joining its worker, and stamps SDK `0x11000000`. Five seconds
after startup its worker independently opens the owned log and writes a marker.
It runs no UE4SS, hooks or game actions. Full C++ DSO cleanup remains separate.

Focused local checks and independent artifact/promotion reviews passed. FTP
readback matched both the module and mode-2 config; original game files and the
loader are unchanged. Details and receipts are in
`.superpowers/native-dependency-spike/native-runtime-import-probe-1/REPORT.md`.
The next manual test is **Ploader Return Code**, then Townfall left at the title
screen at least ten seconds. Await its notification and
`/data/townfall_runtime_import_probe.log`; module execution and full import
resolution for this new variant remain unproved until then.

### Prior probe control sequence

The earlier observations below retain their historical checkpoint status;
this exact SDK-variant result supersedes their pending/no-confirmed-load status
only for the present log-only module.


The new diagnostic has now run: the user reports **FAIL 75ms,
rc=0x80020063**. GPCS4 and fpPS4 public headers name this ESDKVERSION.
This narrows the failure to an SDK-version result, but does not establish which
PS5 check rejected the module, signature acceptance, or module startup.
The probe records SDK `0x02000009` and companion word `0x08050001`;
its wrapper app/firmware versions are both zero. Saved Townfall param.json
records SDK `0x1100000000000000` and required firmware `0x1360000000000000`.
A separate user-requested candidate will change only SDK metadata to the
package-declared `0x11000000`, retain the companion and current wrapper/options,
and leave code/imports unchanged. This is a hypothesis test, not an established
SDK retargeting fix. It is not yet deployed.


Latest follow-up: the user reported the same FAIL 75ms, then explicitly confirmed
selecting **the original ploader / ploader.elf**. That is not a diagnostic-loader
result. Read-only FTP confirmed the installed Ploader Return Code still matches
its reviewed hash, contains the rc format, and lacks the old exact FAIL format.
The next manual run must select **Ploader Return Code / PloaderReturnCode.elf**
with Townfall fully closed; the same v2 module and mode-2 config remain unchanged.


**Latest unchanged-v2 repeat: user-reported FAIL 75ms.** Ploader reported
“Injecting one plugin”; Townfall reached its title screen and normal gameplay
without a crash, followed by **FAIL 75ms modbridge_log_probe.prx** and later
Ploader timeout/shutting down. The restored 5,757-byte v2 PRX remains the exact
`ccb9085c9eb69e2d2bd7294e2666d6b2acbb6aa422e0e7572762f708053c20ce` control,
enabled alone in mode 2; v3 and the prior config are preserved.

The user then reported **“it still says fail after fresh reset”**. The exact
latest elapsed time, game state and fSELF toggle are unreported; this does not
establish that kstuff is inactive. At that fresh-reset test, the diagnostic
payload had not yet been uploaded; the subsequent installation is recorded below.

The historical exact reply **“OK”** is retained against question
`call_Q52OFoay6SiBhJa391sCjAwa`. It is uncorroborated and now uncertain: the user
asked when they said OK and said **“im restarting now but idk if it did say ok”**.
There is no confirmed successful module load. These observations establish
neither a console/HEN change nor a kstuff cause.

The independently reviewed one-line rc-message diagnostic Ploader is now
**installed as the distinct manual entry “Ploader Return Code”**, at
`/data/pldmgr/payloads/PloaderReturnCode/PloaderReturnCode.elf`. Raw readback matches
SHA256 `009445ab35ecf447696a325c88c617d469d5b6cc966ccece914bdc09577f706d`.
The placement receipt is
`.superpowers/native-observations/ploader-return-message-placement-ae808f1a67dd4ff394ee02595bc522df/receipt.json`.
V2 and its config pins remain preserved; no existing payload was overwritten,
no autoload changed, and the diagnostic has **not been executed**.

The latest reply was **“same log also where is the fself and fpkg setting”**;
no new exact timing or game state was given, and fSELF has not been checked.
The menu location is being checked against primary source. Next, the user fully
closes Townfall, manually runs **Ploader Return Code**, launches Townfall and
captures the exact rc-bearing notification. That same-v2 diagnostic test is pending.

A0's first attempt crashed before loading screens; its second reached gameplay
but reported Ploader FAIL 76ms, followed by Ploader finished. Installed A0 bytes
and the selected config matched. A0 and A2 are now disabled. The prior inference
about v3's cause was too broad: A0 also removed dependencies, constructor content
and other package metadata, so it was not a matched v2-minus-notification test.
The package comparison lists those differences without assigning a cause.
The user separately authorized a diagnostic-only Ploader status-message change;
that work is local and does not alter this unchanged-loader control.

The subsequent requested ABI audit found no concrete Linux-constant mismatch
in the compiled probes or active port paths. Both probe builds pass `0x309`
(`O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW`) and mode `0600`; native headers
provide `O_CREAT=0x200`, `O_APPEND=0x8`, and `MAP_ANON=0x1000`. V2 already had
both startup entries. V3 added notification behavior and marker plumbing,
without changing the selected producer or wrapper.

The user then requested the **A0/A2 return-only pair**, superseding A/B.
Build `84234aa60456497db150232c3fba7922` produced two independently reviewed
4,618-byte modules: A0 returns `0`; A2 returns `-1`. Both have zero imports,
no init-array content, no file operations and no notifications. Their shared
embedded identity and all other dynamic metadata match; each exports
`module_start` at `DT_INIT=0x10`. Separate mode-2 configs and instructions are in
`outputs/ps5-mod-bridge/native-module-probe/handoff/return-probes-84234aa60456497db150232c3fba7922.zip`.
Both PRXs were copied to `/data/plugins/` with matching raw readbacks. A0 was
tested as described above; A2 has not been tested. Both are now disabled; the unchanged v2 repeat result is recorded above.
A-open remains deferred.

Interpretation remains conditional: the retained Ploader passes NULL for the
API's separate module-start result. Whether a negative initializer result
changes the API's module handle/error on this native path is unproved.
Both OK would leave execution versus ignored return value unresolved.

The preserved, now disabled **v3** log probe is build `9ee3214808cb4d3d9b22ff268b4f41c0`,
6,845 bytes, SHA256 `1a172a6e42b986ba0e9b5a37f4c8695ad489b1227940ec511925eec8061f2a55`.
After the user launched Ploader and Townfall, they reported **FAIL 76ms
modbridge_log_probe.prx**, while Townfall reached its title screen and gameplay.
No probe startup notification was reported, and the last FTP check found no
startup log. The retained Ploader source uses 76ms as elapsed time, not an error
code; the notification does not identify the failing operation or raw return
value. The user supplied the [Dekazeta download page](https://www.dekazeta.net/foro/files/file/5319-ploader/),
which advertises 1.1.0, matching the retained upstream release version. The
installed payload bytes are not matched to that release. Native module startup
is unproved. The earlier v2 OK notification below is historical, not the latest
result.

Offline comparison found no confirmed defect in the v3 ELF or fake SELF wrapper.
All four imports exist in the original console libkernel export metadata.
Missing own SONAME and different SDK parameter words are observed differences
from the original game libc, but neither is established as a rejection cause.
No speculative v4 was built and no repeat test was requested.

Separately, the **actual composed runtime object now passes the local import
contract check: 187 of 187 strong imports have explicit identities and types,
zero missing imports and zero conflicts.** Ordinary read-only FTP supplied
original libc, libkernel and LibcInternal export metadata. The candidate selects
all 124 relevant LibcInternal exports together, matching the SDK runtime's
provider and declared symbol kinds, instead of mixing eight newly found symbols
into the game's libc family. The other 63 required imports retain their existing
declarations. Six optional weak hooks remain unresolved.

This is dependency metadata closure only: the check emits no PRX and proves
neither loader acceptance nor ABI compatibility. C++ module loading, executable
memory services, target Unreal mappings and real game-thread execution remain
unproved. The existing runtime object and previous candidate artifacts were
preserved. Detailed evidence is in
`.superpowers/native-dependency-spike/ue4ss-original-export-integration-1/REPORT.md`
and `gate-coherent-libc.json`; the probe comparison is in
`.superpowers/native-dependency-spike/native-log-probe-format-comparison-1/REPORT.md`.

## Earlier source-port and probe milestones

The latest composed build is `consolidated-27/result.json`: **355 production C/C++ units (296 C++, 59 C)** plus one Rust object with 17 endpoints. The ordinary SDK shared-library build `link-21` links with no unresolved symbols under its SDK stub declarations and a valid dynamic table; 51,731,768 bytes, SHA256 `ee24562b5a6280fcf8945ab3f861b7398c42026af2b47e654256e9dcfc6a0b51`. This successor has not executed on the console. The frozen stage24 standalone payload passed its PS5 layout-publication test; no build has demonstrated in-game UE4SS or downloaded Lua-mod execution.

The public native Linux [PR1347](https://github.com/UE4SS-RE/RE-UE4SS/pull/1347) was compared at commit `d68f58c02bf3ddecb56f9421106677a3e30dda23`. Its preload shim supplies an actual Linux application entry point, and its separate reflection runtime uses target-specific UE5.1 layouts. Those are not PS5/Townfall facts. Our saved PS5 payload dispatch calls `elfldr_spawn`, which creates another process. Our existing factory begins after a legitimate in-process caller already exists; it does not supply that caller. Stage27 already implements the Lua queue and Tick callback connection, but no genuine Townfall Tick has driven it. The earlier stopped game-memory action was not retried.

A separate `ue4ss-native-static-find-resolver-1` candidate ports the public fork's generic `StaticFindObjectFast` algorithm. Six owned-input tests, the PS5 Rust build, SDK C consumer compile and partial link pass. It has no matching Fast consumer in the current runtime and remains unintegrated; ordinary `StaticFindObject` has a different signature. Stage27, engine binding and game-test readiness are unchanged. The candidate's REPORT.md and integration-manifest.json retain its exact scope and reproduction evidence.

The current setup is **user-reported OnionHEN 13.60**, correcting the earlier
etaHEN identification. Old etaHEN observations remain historical and do not
identify the active HEN. The user subsequently downloaded and ran `ploader.elf`
and reported no crash with a “no .prx loaded” message. That establishes reported
standalone Ploader startup only. After the missing config was placed, the user
gave the exact reply “OK” to question `call_Q52OFoay6SiBhJa391sCjAwa` after
the reported “Injecting 1 plugin(s)” prompt. That historical reply remains
uncorroborated; the user now questions whether the notification actually said
OK. A subsequent FTP read found no `/data/townfall_mod.log`; the sandbox listing
had no Townfall mount. No successful module load, Townfall startup-entry or
UE4SS execution is confirmed. The exact downloaded loader build is not yet
recorded. No HEN or loader was dispatched by this project.

The user's subsequent lead identified the public **unofficial**
[GronedWaffel 13.60 port](https://github.com/GronedWaffel/etahen-13.60).
Its maintainer reports limited hardware testing; its status document leaves
plugins and the older `/data` sandbox patch unverified. This corrects the earlier
search's omission, without establishing native Townfall module loading.
[Ploader v1.1.0](https://github.com/yourphonelinging0x0/ps5-plugin-loader/tree/1409a94aa6a5851acde72f85d3a1f6ae64a1b0a6)
matches configured title IDs, but documents only 11.60/12.20 and PS4-game tests.
Its numeric mode 2 is a timing/module-count heuristic, not an engine-ready event.

An independent log-only native module has now compiled, linked and been wrapped
locally under `outputs/ps5-mod-bridge/native-module-probe/`. That earlier v2 build is
`2db853878acd4965a8d8629b117a469e`: 5,757-byte SDK fake SELF, SHA256
`ccb9085c9eb69e2d2bd7294e2666d6b2acbb6aa422e0e7572762f708053c20ce`.
Its embedded ELF is ET_SCE_DYNAMIC; wrapping/extraction preserves every loadable
segment. This is a plaintext fake wrapper, not a Sony signature or proof of
13.60 acceptance. The bare ELF is retained separately for inspection.

The two requested startup paths have separate build-tagged lines: DT_INIT
points directly to exported `module_start` (returns 0), and the sole init-array
entry points to a distinct callback. Only open/write/close are imported, all
from libkernel.prx. Local inspection confirms nine unique relocation targets.
The public SharpProspero writer at `5dfdb4b0b166e83dc9ae275001afb695f12c7174`
needed two small retained patches: omit executable-only parameter relocations
for libraries, and support an explicitly selected library DT_INIT function.
Seven focused initializer controls pass, with unchanged default behavior;
original upstream source and binaries remain preserved. The compiler's default
`.ctors` output required an explicit retained `.init_array` entry in our source.

Townfall ID PPSA33286 and all game-specific paths remain in a profile. The
current PRX, mode-2 config template, exact commands, source patches and expected
tagged lines are retained locally. Ordinary FTP initially left an owned temporary
module and no config. After the user reported “config empty,” a read-only check
confirmed the missing file. The exact probe was promoted to its final path and
the single-title mode-2 config was installed; both raw readbacks matched. The first readback comparison differed because this FTP server
returns decrypted SELF contents by default. A connection-scoped raw readback
then matched all 5,757 wrapper bytes exactly. This is file-transfer evidence,
not module execution. The earlier single-entry bare-ELF probe is retained as
superseded evidence. No original game files changed; Stage27 remains separate
and unbound, and this probe contains no UE4SS.

## Experimental full-core PRX and remaining runtime dependencies

The user subsequently resumed local source-port work while unavailable for
playtests. Loading is a provisional development assumption; no console success
is assumed and no new deployment follows from it.

The stage27 partial object has 186,885 ELF sections. Derived packager fixes
support its extended section/symbol indices, genuine SDK module/unwind span
symbols and legacy constructor/destructor arrays. The corrected producer skips
R_X86_64_NONE before symbol resolution. Its kept-unwind/discarded-COMDAT failure
was resolved by using the actual SDK `ld.lld -r` to normalize the original
production object and five real static runtime archives. No unwind stripping,
replacement addresses or producer-source changes were needed for this step.

`ue4ss-native-prx-prelink-1/result.json` records the first complete experimental
full-core artifact: 30,153,632-byte ET_SCE_DYNAMIC ELF, SHA256
`c1000aabeee4bbb35cfb314afe22d855128bf7f8570495f3aa24beddb1a0e6d7`.
The SDK fake SELF wrapper is 30,166,463 bytes. Five nonoverlapping load segments,
226,276 indexed unwind entries, 230 initializers, relocation checks and wrapper
segment round-trip pass locally. Frozen production inputs remain unchanged.
This artifact is not ready for deployment: the original SDK stub identities
remain unverified, and runtime/bootstrap prerequisites remain absent.

The actual import review identifies six undefined weak hooks. Five `__dl*`
wrappers check availability and refuse native C++ mod loading without their
providers. A separate SDK `mmap`/`mprotect` guard refuses executable-memory
requests when optional `kernel_mprotect` is absent. Seventeen owned host modes
and native compilation pass. A derived `libc.a` changes only that member; the
shared SDK and original stage27 remain untouched. The guard supplies no new
executable-memory service. Genuine emulated TLS and static C++ runtime objects
are already linked.

The original composition needs 208 strong imports plus those six weak hooks.
Saved import declarations and the explicit public catalog cover 126 strong
names; 82 lack a complete module/library/version declaration. The new runnable
`ue4ss-prx-import-contract-1/compile_contract.py` refuses that input with exit 2
and emits no import contract. Seventeen focused checks pass, including type and
cross-symbol group conflicts. Declared identities do not establish provider
availability or ABI compatibility.

A real LLVM compiler-rt object now supplies the three previously external
128-bit divide/remainder helpers and their upstream dependency. Actual PS5
compilation and an isolated consumer link pass; two host architectures each
pass 4,673 defined-input arithmetic cases against Python integer results.
The separate `ue4ss-native-prx-prelink-2` successor now includes those helpers
and the guarded libc. It has 205 external imports, with exactly the three
arithmetic names removed and no new imports. Its 30,153,440-byte bare PRX has
SHA256 `01c864a873cfec3b29925fe2d19e61bd6dbce20d0000b0fd99c42ff50106a28a`.
Guard code is present in the normalized object and emitted executable segment;
segment, unwind, initialization and wrapper checks pass. Original inputs and
the first experimental artifact remain preserved.

This comparison also exposed an import-kind defect: normalized undefined
references are NOTYPE, and the provisional writer defaults all imports to FUNC.
Five required data symbols are OBJECT in the ordinary SDK reference link. The
experimental PRX therefore has a known metadata error in addition to incomplete
library identities. The separate `ue4ss-native-prx-import-types-1` correction now carries explicitly
evidenced FUNC/OBJECT metadata into the writer, rejecting unsupported/conflicting
kinds. The original seven-field API and legacy output remain intact. Owned
fixtures demonstrate the corrected kinds; the older experimental PRXs are
unchanged and retain their known error.

Four further pure libc functions (`strpbrk`, `wmemchr`, `difftime`, `frexp`)
now have genuine static implementations from pinned FreeBSD/musl sources.
SDK type checks establish the provider's 16-bit unsigned wchar and 64-bit signed
time ABI; native compilation and four host/reference runs of 3,070 cases pass.
`ue4ss-native-local-deps-3/attempt2` links these into the previous normalized
object, removing exactly those four undefined names with none added. Its object
SHA256 is `a84ab98267cb3159e5908f775621260591377fe5fa518737a4f1eef0aed80b57`.
The new `guarded_package.py` resolves requirements from this actual object,
checks explicit SCE identities, verifies supplied types against hashed SDK
symbol records, and sets the corrected writer's ExplicitType for each import.
It rechecks the object and requirement set before writing. The actual latest
object has 201 strong imports and six weak hooks: 126 strong names have complete
identity/type declarations and 75 lack identities, with no conflicts. The gate
returns 2 without invoking the writer. No full-core PRX has been emitted through
this corrected path. Root ran 17 contract tests and seven actual-ELF gate tests;
all passed, including correct NOTYPE reconciliation and output preservation. An initial SDK wrapper invocation failed before
linking because LLVM_CONFIG was unset; the corrected, recorded invocation passed.

The next source batch replaces Lua's private FILE macro use and fmt's direct
FILE-buffer optimization with public interfaces. Three rebuilt production
units are substituted into the original component recipe, with real hidden
fgetc and upstream formatting/filesystem and digit providers. The actual
`ue4ss-native-local-deps-4/attempt1` link passes, preserving original inputs.
Its SHA256 is `a186366b4be3932402cc2249f6f8b29d39495f797badff2359b25c8afcc09369`.
Eight external names disappear and rmdir is added with an explicit catalog
identity and SDK type record. The current independent gate reports **194 strong
imports, 127 covered, 67 missing, six weak hooks and zero conflicts**; it returns
2 and emits no PRX. Actual host Lua/formatting/fgetc and static provider checks
pass. The old optimized formatter failed its buffer-span assertion in the host
fixture; the fallback is checked against required output/error behavior, not
claimed equivalent to that failing path. The remaining getc and __isthreaded
references are in retained SDK iostream/arc4random members. Wide-character and
locale replacements remain unproved because of concrete state/ABI differences.
No console or account contact occurred in this batch.

The following source batch produces `ue4ss-native-local-deps-5/attempt1`, SHA256
`dc393423b4784d549bd7abb37109e6ccdfee1c460c714424584db80be8ea5c91`.
It removes seven more actual external references with none added. Checked public
random locking removes the private thread flag, with failed entropy acquisition
now stopping instead of inventing seed data. Real stream/equal-range wrappers
supply getc, wcstoul and fseeko/ftello; a process CPU-clock provider supplies clock
with target ABI and overflow checks. Native compilation, focused host controls
and actual composition pass. The current typed gate reports **187 strong imports,
126 covered, 61 missing identities, six weak hooks and zero conflicts**. It still
refuses packaging and invokes no writer. The remaining localtime_s cannot be
substituted with Windows' signature or an unproved shared-storage localtime shim.
No native target code ran and no console/account contact occurred in this batch.

The local `ue4ss-prx-export-evidence-1` reader now preserves complete serialized
module/library identities, distinct versions, raw export kinds and optional
SONAME for the existing build. Its bridge reads actual ELF bytes and emits only
uniquely matched declarations, without filename guesses or alias inheritance.
The initial reader passed 53 owned fixtures and the legacy compatibility probe;
a narrow hidden/internal-symbol correction then passed its targeted test. The
final binary also passes the end-to-end bridge into the existing native-object
import gate (3/3 owned references), and reads both exports from the actual
locally generated log-probe ELF. That module legitimately lacks SONAME; the
reader reports the absence without guessing. Fourteen bridge controls pass.
This closes a tooling gap, not the 61 actual missing identities: original
libc/libkernel export bytes remain unavailable locally. No writer, console,
account or game code was used. The scope's REPORT.md and result.json retain
separate initial/final test evidence and the next real-input requirement.

A subsequent bounded source audit could not close additional imports. The actual
private file callers require semantics that a public cancelable open/read/close
substitution does not establish, including an entropy fallback under a mutex.
Native declaration/width probes pass, but no replacement was built. Actual
pthread detach/trylock consumers use opaque POSIX handles; retained SCE declarations
do not establish their interoperability or error contract. Pathname adapters
likewise lack required providers or introduce different resource/side effects.
The three `ue4ss-public-{file-primitives,thread-functions,path-functions}-1`
reports preserve exact consumers and blockers. The runtime object and its 61
missing identities remain unchanged. New target evidence is required before
continuing these routes; no further equivalent shim audit is warranted.

The installed log-only probe remains v2. The separate local v3 successor adds
visible entry notifications before file open, followed by exact error codes if
logging fails. Its native build and three focused host control groups pass;
only four libkernel imports are needed. It has not been installed or executed.
This separates “entry did not run” from silent log-file failure when a later
console test is possible. It contains no UE4SS, hooks or game-memory code.

The earlier offline Ploader cleanup candidate has thirty focused host controls
but no native build or installation. Two subsequent credential/ptrace/register
and temporary-page/partial-transfer correction tasks were rejected by automatic
approval review as possible cybersecurity risks. Those tasks remain stopped;
partial work must not be promoted, retried or rerouted through another loader.
The stop record is `ploader-promotion-review-stop-2026-10-03.json`. The later reported Ploader “OK” still lacks a matching startup-log result.

## Private source backup

The user-authorized clean source export was pushed to private `Sammsamy02/ps5`,
branch `codex/source-backup`, commit
`d409e39494158d0ab2fd8096c09f61b124409f4e`. All 1,551 remote file object hashes
match the reviewed local export. Existing worktree files, history and remotes
were preserved. Credentials, game/mod captures, compiled artifacts and rejected
Ploader work are excluded. Private-provenance Unreal bodies and certain authored
changes depending on them are also excluded and documented; this is not a
complete reproducible UE4SS source distribution. The export's PUBLICATION.md and
manifest describe the precise gaps.

## Lua keybind and reflected-call ownership

Stage27 changes six normal/async keybind captures to own their registration-local helper instead of keeping a reference after it expires. The unchanged Item Spawner uses the normal F7 registration path. This fixes that helper's lifetime only; Lua state and mod lifetime remain borrowed under the existing runtime contract.

Native reflected calls now own their temporary out-table registry references through the actual protected Lua call boundary, with cleanup before error propagation. C++ stack RAII alone was insufficient: this build compiles LuaRaw as C, and protected Lua errors use longjmp. This source adaptation preserves the existing Lua-facing reflected call API and mod script; it does not supply engine objects or prove their layout. Its supported callers are the existing registered Lua callbacks. Direct C++ invocation without an outer Lua error handler is not covered by an error-parity claim, and other inherited C++ locals skipped by longjmp remain outside this reference fix. Detailed normal/error-path observations are recorded in each experiment's REPORT.md.

The two changed production units compile through the actual SDK and link into stage27. The original downloaded mod is unchanged. No console/game access occurred. The source audit also confirmed that another loader abstraction would not resolve the remaining target evidence gap: module ownership alone does not establish engine heap/object lifetime, declared layout metadata does not prove target offsets, and no legitimate engine producer is presently connected. The port's existing wchar/TCHAR width assertions match its SDK build; this is not evidence for every game's ABI.

## Native startup and loaded-code lifetime

Stage26 closes concrete startup/cleanup defects. Native FName verification now checks successful callback removal and retains its forwarding hook, because removing a callback does not drain other callers of the original function. Failed removal preserves the pending state; a failed game-thread initializer registration now throws instead of silently continuing. The callback is registered once: this is not a new automatic retry mechanism. Source-backed host controls reproduce five baseline failures and pass all 25 checks with the correction.

Ordinary native detour deactivation now leaves active hooks installed. The internal reclamation method requires independent exclusion of new callers and drainage of every admitted call, including callers paused before the counter increments. Full invocation accounting covers the original trampoline, callbacks, derived work, exceptions and the custom InitGameState entry. A failed unHook retains its exact owner and backing. Real owned host AsmJit/PolyHook controls exercise these paths; no live-game exclusion provider exists, and counter zero never supplies that authority.

The actual native module loader retains acquired code references until process exit. Wrapper Close disables lookup without dlclose. A fixed 64-slot source-private registry reserves capacity before dlopen and publishes a successful handle without allocation or locking, including initializer reentry and startup/export failure. This is a bound on acquisitions/reservations, not on compatible mods. Genuine local dynamic-library controls pass with sanitizers, including deferred execution, concurrent capacity and missing exports. The module consumer's declarations/layout are unchanged; the same SDK-compiled CppMod object remains usable.

Native mod discovery now accepts the original `Scripts` directory spelling, with the same lowercase fallback as LuaMod. Previously discovery checked only lowercase `scripts`, which would skip the unchanged Item Spawner archive on a case-sensitive filesystem before LuaMod could inspect it. The downloaded archive and script remain unchanged; discovery is not engine execution.

Four production objects were rebuilt and integrated: Hooks, UnrealInitializer, CppModModule and Program. This does not retain mod userdata, retire external workers, make Program teardown safe, or prove the game C++ ABI. Game binding, unchanged Item Spawner execution, DLL translation and the application remain outstanding. This batch did not contact the console or retry the stopped game-memory action.

## Current callback lifetime correction

Stage25 strengthens successful native callback removal: it first excludes new invocations, waits for the existing executor count, and serializes callable destruction before returning. Stale callback snapshots then retain empty callable storage. Same-thread synchronous self-removal refuses before invalidation, including recursive invocation and destructor reentry. Native executor accounting now also covers exception logging and caught exception destruction. Windows-selected source tokens and public layouts remain unchanged.

Six focused source-backed owned host controls pass with sanitizers, including two simultaneous removers, retained snapshots, recursion, destructor reentry and blocked exception handling/destruction. Actual SDK dependency analysis found two affected production units; both were rebuilt and integrated. This is not owner-wide callback retirement or a guarantee that detours/trampolines can unload. A failed unregister does not establish quiescence. Stage26 now corrects the concrete engine-tick cleanup path, as described above.

The width helper also now sizes the genuine private UFunction.Func declarations: the compiled member-function-pointer representation is 16 bytes and the pointer representation is 8. Conservative bounds retain both and use 16, giving 304 available canonical fields and 115 refusals. Public GetFunc currently selects the latter representation; the conservative maximum is not game-storage or call-ABI evidence. UStruct.SuperStruct remains unavailable because its legacy type is incomplete.

## PS5 layout publication passed

One dispatch at `2026-10-03T00:29:33.952299+00:00` completed the genuine stage24 layout-loader test on the console. The receipt confirms exact member/vtable maps, canonical aliases, inherited map merging, full-width bounds, rejection without mutation, actual static size accessors and same-facts retry. The test used only its own synthetic INI files and production map storage. No game objects, game memory, game hooks or downloaded mods were involved. Actual target layouts and an in-game UE4SS trial remain outstanding.

Receipt: `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-layout-console-1/console-result.json`; artifact SHA256 `be18182f0d324c5dc4f27fee0cbab15f678159619441c7306d786a1553a6b2fe`.

## Current native layouts and Lua lifecycle

Stage24 implements a native path through the existing member/vtable INI files. It stages both files, checks matching declared metadata and actual available engine axes, and publishes the existing UE4SS maps before use. Native containers no longer fill missing entries with generated Windows offsets or apply the old debug-size adjustment. Shared inherited vtable maps are merged by actual storage identity; conflicting values refuse. Same-bundle/same-facts retries retain maps after a second-pass scan failure. Parser controls pass 89 host assertions.

Field bounds use the target compiler's actual accessor value sizes, with the maximum across existing accessor alternatives. Unavailable/incomplete types refuse when supplied instead of assuming one byte. The frozen stage24 helper resolves 303 of 419 canonical fields and explicitly refuses 116. These are port-type sizes, not proof that a game's declarations match. Revision/build IDs establish consistency between the files, not independent identity. No actual game layout values were produced. The complete binding compiles/links; a direct host execution attempt failed because the genuine Prospero headers require a different wchar/TCHAR ABI from Darwin. No workaround or passing binding-runtime claim is used.

Native mod discovery, shared-library loading and C++ startup callbacks now wait until real Unreal initialization succeeds. This prevents the program's mod callbacks from reading empty maps or caching unavailable sizes before publication. Windows ordering remains unchanged. Five native TypeChecker reads now use its genuine map-based class accessor instead of the fixed offset 8; focused extracted owned-storage controls pass.

Queued Lua actions now retain the existing recursive action lock through invocation, reference cleanup and delayed-action updates. Nested dispatch is refused, a competing close waits, and uninstall releases its preflight lock before joining a worker. Native ProcessEvent uses one shared dispatcher registration. Actual LuaMadeSimple/LuaRaw controls passed simple/delayed execution and concurrent-close cases. Callback retirement and full engine-hook quiescence remain unresolved.

Those tests exposed a separate real LuaRaw defect: completed close left the process-wide mutex locked. The corrected finalization balances exactly the close-entry acquisition. Genuine Lua tests pass cross-thread close/reopen, overlapping states, coroutine cleanup, failed partial creation and outer recursive lock preservation. Windows source and public layouts remain unchanged.

All of these changes are reusable source adapters. No mod-specific script was rewritten, no console/game access occurred during this milestone, and the earlier stopped game-memory action was not retried. Owned native map publication has now passed. Valid target layouts, a legitimate loaded-game source, callback lifetime completion and unchanged in-game UE4SS mod execution remain outstanding. DLL compatibility and the application remain later work.


## Current loader and game-thread changes

Stage23 adds two genuine loader-facing units. They assemble the existing native context from retained module bytes, paths, mapped-image facts, scanner policy and optional input, then construct the actual UE4SS program. The factory neither discovers a game image nor runs engine initialization. Twenty-two host context/ownership controls pass; both production units compile and link. Actual factory execution remains untested.

Native Lua hook helpers now check real callback-registration errors before claiming success or creating action references. FName verification checks the same error before waiting. An error can occur after detour activation, so it preserves live state and does not imply safe unloading. Focused extracted-source controls exercise disabled/unready registration through genuine Lua error delivery; full LuaMod/FName game runtime is untested.

The actual Tick callback now publishes game-thread identity under a private lock. Existing raw getters remain nonthrowing, and ProcessEvent returns before touching Lua action state when called from an unknown or different thread. Fifteen owned-thread host controls pass, and both changed production units compile. Without real Tick publication, pending actions remain pending. Callback ownership and action/uninstall lifetime coordination are still required. A stalled host TSan diagnostic is retained without a passing TSan claim.

The native program status getter now reads its existing atomic event-loop/shutdown fields instead of an unassigned legacy flag. This supports normal reinstall notifications; it is not successful engine startup, all-mod success or teardown quiescence. The real SDK translation unit compiles, with no public layout or Windows behavior change.

## Remaining native engine-layout gap

The source audit found a concrete ABI gap: version5.6 currently loads Windows PDB-generated member and virtual-slot offsets. The native call adapter changes how a function is called; it does not convert those offsets. No constant slot shift or valid PS5 layout is established by the available sources. Successful component tests cannot close this gap.

Stage24 now implements binding explicit native layouts through the existing `MemberVariableLayout.ini` and `VTableLayout.ini` paths before accessors cache them, retaining Windows behavior. Correct numeric mappings require matching target build/layout evidence, which is absent locally. An admitted loaded-game source, valid layouts, callback lifetimes and actual unchanged-mod execution remain before game-bound UE4SS can be called working. DLL compatibility and UI follow that work.

The latest local changes did not contact the console or alter game files. The existing stopped game-memory action was not retried or rerouted.


## What the composed code now contains

- Actual UE4SSProgram startup, settings, mod lifecycle, LuaMod/CppMod and Input source. Its native interfaces accept loader-supplied paths/module context and an injected input provider. The controller producer now builds as described below; an actual game-module producer remains absent. Native startup rejects unavailable desktop services. Event callbacks run outside the queue lock and update errors are observed after join. Actual engine-hook/callback quiescence is unresolved; safe unloading of live engine hooks is not established.
- The real Lua 5.4.7 VM, LuaMadeSimple and all 32 LuaType source files. No Lua VM, LuaMadeSimple or LuaType imports remain in the partial object. Host tests execute genuine Lua states, callbacks, tables, threads, errors and registry operations; they do not substitute fake Unreal objects or prove gameplay.
- 138 actual Unreal source files, including reflection/property implementations, containers, version storage, actor/world/data-table methods, memory/string helpers and hook callback machinery. Compiled version maps are not proof of PS5 game layouts. The original Windows version initializer was preserved separately and has no native replacement.
- Real signature/scanner helpers, ObjectDumper, custom properties, Lua logging/shared functions and the native C++ module owner. Native C++ loading rejects Windows PE; DLL translation is not implemented. The genuine Windows `deref_to_int32` body is preserved separately. Its new native provider uses the real owned-region service, returns a signed integer only on an exact four-byte read, and returns nil for unavailable or unreadable input. Thirty genuine Lua/LuaMadeSimple host cases pass; the native object compiles and links with the SDK.
- Five genuine generators including SDK/USMap and the newly compiled JMap source. Native module-name derivation uses the existing validated loader executable path through a const accessor. It performs no additional process discovery; the filename alone does not prove its correspondence to an engine module name. The USMap writer's successful UTF-16/UTF-8 filename and binary write/seek path passed a real owned-file host test. Upstream unchecked write/seek results and unconditional completion logging remain a known failure-path limitation before runtime promotion.
- Actual Zydis and common Zycore implementations plus ASMHelper. A concrete operand-lifetime bug was fixed by keeping decoded operands in the caller's scope, without changing the public structure. Twelve owned-byte decoder/jump/call controls passed ASan and UBSan; no machine bytes were executed. No Zydis/Zyan imports remain.

The earlier native platform, SysV virtual-call adaptation, module snapshots, scanner, File, Unicode, logging and callback-ownership work remains. A real FileDevice close-error control reproduced termination before its destructor fix and passes afterward. Native file cleanup uses its existing nonthrowing owner; no game files were accessed.

## Standalone Lua console evidence

`.superpowers/native-dependency-spike/ue4ss-native-lms-console-1/console-state.json` records one dispatch through Payload Manager 0.5.2 at 17:56 UTC on October 2. Both native receipts matched actual host-test bytes. The genuine LuaMadeSimple/LuaRaw test exercised primitives, registry references, C++ callbacks, tables, a child Lua thread, error callbacks, C++ exception propagation, traceback, stack cleanup and explicit state close. Native assertions remained enabled. No engine or downloaded mod code was involved.

The initial standalone executable's successful link concealed an oversized `PT_DYNAMIC`. The existing caller-local SDK linker correction fixes that concrete geometry failure; the SDK stays untouched. The reviewed console ELF is 1,925,760 bytes with a 400-byte dynamic table, genuine `_start` at `0xa3368`, and 169 imports covered by the three declared SDK libraries. Host replay, absent-directory and symlink controls also passed before the independent promotion review. Exact sources, commands, import mappings, review and readback live beside `REPORT.md` in the console experiment directory.

This proves the exercised Lua support and C++ error path on this console, not engine access or full UE4SS. The only console writes were a new owned test payload and two receipts in its UUID namespace. Game files, autoload and FTP service settings were unchanged. The transaction is complete and must not be redispatched.

## Native scanner and generated-code console evidence

At 18:33 UTC on October 2, one dispatch of `ue4ss-native-jit-console-1` passed both components on the PS5. The unchanged 20 caller-owned-byte scanner controls completed. Genuine AsmJit allocated a 16,384-byte page, exercised protection and release, emitted the exact six-byte `mov eax, 42; ret` function, executed it with return value 42 and released it. The actual receipt reports phase `complete` and zero error. This is native generated-code execution, not merely assembler output or a host substitute.

The reviewed 1,316,592-byte ELF has SHA256 `a38d3cb1e4cc744b551de1eec604e4a96a297cb721942fb5599ad9da62a1b304`. `console-state.json`, `console-entry.json` and `console-result.json` retain the one-dispatch result. Review corrected durability of the local dispatch claim and state record before promotion. Only the new owned payload and its two receipts were written. No game memory, downloaded mod, installed mod or actual hook target was used. Do not redispatch the completed transaction.

## Concrete remaining boundaries

The ordinary SDK link has no unresolved symbols under its SDK stub declarations; actual provider identities remain a separate gap. Native `ps_scan` is implemented over an already-owned typed mapped-image source with the real read lease and genuine seven-leaf resolver collector. It returns failure for unavailable required results; disabled/version preservation and optional-result behavior remain intact. Static file labels are not converted to runtime pointers. An actual admitted game-image producer and engine/runtime validation remain required.

PolyHook's real native memory-access methods now link through the explicitly owned-region backend, as do native Lua integer reads and the full JMap source. This closes eight previous missing symbols without placeholder results. The backend does not automatically grant access to a game image, heap or old allocator: genuine ownership and lifetime integration remain necessary.

The generic object-table implementation now compiles. Its optional optimized hash-table path explicitly reports unavailable on the native target; normal callers can retain the genuine existing GUObjectArray fallback. This closes the linkage without inventing a table address or lock. Real engine-array traversal remains unverified.

The genuine PatternSleuth scanner core now builds for the actual PS5 LLVM target with Rust core/alloc, exposing bounded caller-owned byte scanning and cross-reference scanning. Twenty C ABI controls, eight upstream scanner tests, three edge regressions and 256 differential cases pass locally. The earlier scanner checkpoint still lacked its native runtime/provider closure; that source/link gap is now closed by the owned typed provider. A subsequent no-std adaptation now builds the complete genuine EngineVersion resolver with the real Iced-x86 decoder and ELF/PE parser. The earlier version 5.4 fixture result was synthetic; the newer saved-file result below is actual captured-game evidence.

The new `ue4ss-native-resolver-1` integrated Rust object provides the byte scanner plus the complete EngineVersion resolver with one core/alloc runtime. A direct merge of independent Rust partial objects reproduced duplicate runtime symbols, so the integrated object replaces the earlier standalone scanner object. Native compilation/linking and the owned resolver controls pass; this new resolver has not executed on the console.

Against the unchanged saved Townfall executable, the first real run failed because its section table points past EOF. The adapter now uses validated file-backed ELF loadable segments when the section table is absent or invalid. Six targeted segment/truncation/overflow controls pass. The actual saved-file run then identified Unreal 5.6 in 31.74 CPU seconds using the host debug build. The 194,241,424-byte capture's SHA256 remained unchanged. This is automatic static engine-version detection, not proof of live engine layouts or a game hook. Exact before/failure/after evidence is in `townfall-static-resolver-result-final.json`.

AsmJit now uses the SDK's ordinary-page allocator. The absent optional `getpagesizes` import caused an actual native link failure; the port leaves large pages unavailable and uses the upstream ordinary-page path. Its real native executable links and the combined scanner/JIT console test passes as described below.

The two CSV data-command bodies are now separated from rendering and define their genuine providers. Native output paths use the existing Unicode conversion and filesystem joins; focused owned-file controls pass. Actual engine traversal remains untested. The asset-registry source also compiles, preserving the original Windows body. Its native memory monitor is unavailable: nonzero budgets explicitly refuse asset loading before engine lookup or callback registration. Zero retains the explicit upstream option to disable budgeting; it is not evidence of safe memory pressure. The unavailable monitor is still a functional gap even though these registry symbols now link.

Standard C/C++ runtime names link against SDK stubs; this does not prove their SCE identities or game-process availability. An actual admitted loaded-game image producer, engine allocation ownership/lifetimes, hardware validation of the new input source, validated reflection/layouts and one real game-thread callback remain required. A clean source build does not supply those services.

Inherited Lua exported functions also contain buffer APIs without capacities/guaranteed terminators and unchecked engine userdata. These are documented limitations, not verified safe APIs. Full-runtime native execution, reflection traversal, generator output correctness, script input and Item Spawner behavior remain untested. No arbitrary-DLL or broad-game compatibility claim is established.

## Object discovery and controller implementation

The next genuine resolver port preserves all six RIP-relative signatures, two immediate signatures, three capture layouts and uniqueness handling for GUObjectArray. Thirty-four owned-image controls and three checked-memory tests pass, and the integrated PS5 object builds/links. Its one read-only scan of the unchanged saved Townfall file returns **status 3: no candidate**. The existing upstream signatures therefore do not yet locate Townfall's object array. That original signature result remains recorded; the newer CFG-based candidate below is a separate strategy. `ue4ss-native-guobject-resolver-1/integration-manifest.json` identifies the earlier four-export Rust object used in stage 12. Live object validity remains unverified.

The new controller producer supplies the existing native Input interface with configurable button-chord to key/modifier bindings. It uses the pinned [PS5 SDL backend](https://github.com/ps5-payload-dev/SDL/tree/ee4c47dc0d617b3bc8f35108f9956baf228a1322/src/joystick/ps5) contract, actual SDK pad/user-service imports, its own pad handle and shared service lifetime management. The real SDK build/link and native-context consumer compile pass; 32 focused host controls cover dispatch and lifecycle. `ue4ss-native-pad-provider-1/build-2/receipt.json` is authoritative. No controller hardware or game-thread input test has run. Linking includes `ScePad` and `SceUserService`; no input injection or per-mod rule was added.

## Reproduction and retained evidence

Run from the managed worktree:

```sh
python3 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/build_port_stage9.py <fresh-output-label>
python3 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/check_object_composition.py <fresh-output-label>
```

Those commands reproduce the earlier stage-9 closure. `compose_stage12.py <fresh-output-label>` retains the verified stage-11 objects and adds the actual controller sources plus the single integrated Rust support object; then use the same object-composition command. The newer `consolidated-12/result.json` identifies all 342 C/C++ source/object inputs plus the single Rust support object and their individual retained command receipts; commands and diagnostics are beside every built object. Earlier build results and replaced source bytes remain retained. These are experiment scripts, not the final product build system.

Public source pin: `e3ba1016562d6c0868c410d0a71e88bfcdbf691b`. Private source pin: `38e7171e9e8c4a871ff84765848f0290ca34a44e`. The public dependency is an extracted pinned archive; running Git inside it finds the enclosing project, not a dependency checkout. The two Program provider originals were checked against the retained archive to resolve that misleading HEAD observation. Current public and private patches/manifests are under the ignored source-build directory; private source remains local.

Focused evidence also lives in `ue4ss-native-lms-port-1`, `ue4ss-native-luaraw-closure-1`, `ue4ss-native-luatype-port-1`, `ue4ss-native-program-providers-1`, `ue4ss-native-unreal-properties-1`, `ue4ss-native-version-storage-1`, `ue4ss-native-asm-dependencies-1`, `ue4ss-native-generator-closure-1` and `ue4ss-native-lualibrary-port-1` under the same dependency-spike directory. Source closure stayed local. The separate owned Lua test was staged and dispatched once as described above. No game/system-settings change or commit occurred. Paused user files and UI work were preserved.

README hints now feed the actual existing C++ inventory/planner in the host-only `readme-plan-integration-1` experiment. The original bounded C reader remains unchanged (51 earlier host controls and native compilation). The integration passes 27 controls plus focused review regressions, reads the unchanged Leon archive without extraction, and keeps exact member/line evidence for paths, prerequisites and variants. Review exposed two defects: explicit runtime versions were discarded and post-filtering could leave dependents eligible after a prerequisite was rejected. Unsupported prerequisite wording now remains unresolved, and adapter refusals propagate through the dependency graph. The original failures and passing corrections are retained under `review-red-1`, `review-green-final-2` and `green-reviewed-final-2`. This is metadata planning, not native app or archive-caller wiring. Real target/backend admission and runtime capabilities remain required.

## Native function discovery implementation

The saved-image adapter now parses actual ELF CIE/FDE metadata and PE runtime-function/unwind chains. Twenty-one owned ELF/PE cases and 268 C ABI assertions pass; the real PS5 core/alloc build passes. Townfall has 13,300 readable FDEs, but none covers the 227 queried object-discovery reference/call labels. The adapter leaves those roots absent instead of treating metadata gaps as function ranges.

The genuine FName constructor/ToString and shared UGameEngineTick algorithms now build for PS5 in the same Rust runtime as the existing scanners and resolvers. Final combined build 4 (`ue4ss-native-engine-resolvers-integrated-1/integration-manifest.json`) has eleven endpoints, with 39 Rust controls passing. Its object is 1,607,728 bytes and is now composed with the 342 unchanged C/C++ units. The final actual SDK link still reports the same nine missing providers; source coverage is not full-runtime linkage. Their original strategies return no candidate on the saved Townfall executable. The tick controls cover a real assembled unique root, two anchors in one root, ambiguity, missing references and missing metadata. Batched cross-reference searching preserves the upstream matching/order rules: one 8 MiB owned host benchmark returned identical results at 192 ms sequential and 2 ms batched. That timing is not a console benchmark.

An additional generic ELF FName constructor candidate matcher follows call arguments from the same five standard Unreal module names. It requires a preserved string argument, stack-local destination and constructor-mode argument before the first direct call, with one common target across all five names. Nine assembled ELF controls and actual PS5 compilation pass. The unchanged saved Townfall capture yields file label `0x23d590` automatically in approximately 0.615 seconds. This is static candidate evidence; no address was hardcoded and no native function was called. The original FName rules remain unchanged. Evidence is in `ue4ss-native-fname-call-matcher-1/integration-manifest.json`.

The complete upstream GMalloc and StaticConstructObjectInternal resolver strategies now compile for the native target, including their genuine control-flow decoder helpers. Six owned ELF/PE cases and three CFG controls pass. A single saved-file trial returned no GMalloc match but selected the StaticConstructObjectInternal file label `0x490aa0` using the original generic rules, with the capture unchanged. Neither result establishes a callable native pointer. The exact closure is recorded in `ue4ss-native-required-resolvers-1/integration-manifest.json`.

The object-array candidate finder uses allocation/removal diagnostic references, bounded decoded control flow and matching caller arguments. Its first strict adapter refused the capture because an unrelated raw LEA encoding had a signed RIP target below zero. That impossible target cannot match a valid diagnostic literal, so the corrected anchor scan excludes it while retaining relevant CFG/argument failures. The new regression failed before the correction and passes afterward; all 11 focused controls and the native build/SDK standalone link pass. The saved-file endpoint now returns static candidate `0x93ba110`, independently supported by allocation and removal callers. This remains candidate inference rather than validated runtime array contents. Both the original refusal and narrow correction are retained in `ue4ss-native-object-discovery-1/native-matcher-2`.


## Native owned-memory providers and current full link

`ue4ss-native-owned-memory-1` implements the actual PolyHook MemAccessor ABI over explicit caller-owned ranges and its own anonymous mappings. Bounds, read/write permissions and lifetime are checked under the service lock; real protection changes update metadata only after successful `mprotect`. No arbitrary process mapping is registered. SDK compile/link and host allocated-memory controls pass. Existing JIT/trampoline allocators need to join these lifetimes before an actual detour can work.

`ue4ss-native-lua-deref-owned-1` supplies the native integer-read function using that same provider. Its Lua argument never authorizes or registers its source pointer. `ue4ss-native-jmap-port-1` compiles the full genuine JMap generator with fresh permission queries, a retained module snapshot, real UTC conversion and the existing executable-path getter. Twenty helper assertions pass; no engine dump or game traversal was run. Permission queries are metadata snapshots, not access leases.

The 345 C/C++ units plus one integrated Rust object compose successfully. The actual SDK link in `link-7` resolves the new JMap, Lua and PolyHook methods and leaves only `ps_scan`. This remains a partial source build rather than a runnable full UE4SS image.

A further tick-candidate experiment rejected three tempting raw CALL matches after actual instruction decoding showed that their E8 bytes overlapped other instructions. The original tick strategy remains unchanged. `ue4ss-native-tick-cfg-1` retains the decoded evidence; no unsupported tick address was promoted.

## Allocator integration and GMalloc candidate

The genuine AsmJit mmap/protect/free paths and PolyHook bounded allocator now register their own successful allocations before publication. Their exact free/protect paths serialize with checked memory operations, retain bookkeeping on syscall failure, and reject mismatched allocator ownership. This fixes the reproduced case where AsmJit allocated memory but the checked service could not recognize it. Actual SDK compilation and standalone owned-data links pass; allocated-memory tests run on the host. The new backends have not executed on the PS5. Final sources are frozen in `ue4ss-native-allocator-owned-1` and `ue4ss-native-bounded-owned-1`.

An additive generic GMalloc candidate strategy is now integrated as the twelfth Rust endpoint. It follows a standard Unreal literal and decoded SysV call arguments through the virtual allocator call, including the compiler's register-indirect global load form. Ten generated ELF cases and original resolver controls pass. The combined endpoint returns static file label `0x9350950` from the unchanged saved Townfall file in 0.280 seconds on the host; the original GMalloc strategy still returns no match. It is not a validated live allocator singleton. `ue4ss-native-engine-resolvers-build5-1/build-5/integration-manifest.json` preserves the exact evidence.

The allocator providers and new single Rust object compose in `consolidated-15`. The actual SDK `link-8` still leaves only `ps_scan`; all other providers resolve. A separate genuine PolyHook hook/unhook fixture is being built on owned generated code. It has not supplied a game hook or gameplay result.

JMap review qualification: inherited object/vtable/class accesses at source lines 2190 and 2226 precede readable-region checks. Its full generator is compile-only until those traversals have adequate guards and stable ownership for every followed reflection edge.

## Executed owned hook/unhook path

`ue4ss-native-owned-detour-1/link-2/host-execute.stdout` records actual Darwin x86/Rosetta execution with ASan/UBSan. Genuine AsmJit emitted the source and replacement functions, and genuine PolyHook with the owned-memory backend performed the explicit INPLACE hook. The source returned 42; after hooking it returned 84; the trampoline returned the original 42; unhooking restored 42. The second unhook refused, and generated allocations were released. No game, process discovery or external module was used.

A controlled accessor then performed an actual seven-byte prefix write and refused the rest. The installer restored all 100 checked original source bytes, the original return value and an empty trampoline handle before returning failure. This tests the concrete cleanup defect found in review. All throwing patch construction and registration now precedes the first source change; the successful commit path only publishes scalar state afterward. Persistent rollback/unhook refusal may still terminate during noexcept destruction; this is not a general transactional or concurrent hook API guarantee. Default/recommended schemes other than explicit INPLACE remain unproven.

The native source and helper compile with the actual PS5 SDK. Three changed PolyHook objects, one new trampoline helper and the two dependent Unreal objects are integrated in `consolidated-16`; no fixture main enters that build. Compiler dependency scanning identified the affected inline-header consumers so the composition does not mix their old and new definitions. At that local build stage, the native detour had not executed on the PS5; the exact owned INPLACE path was subsequently exercised below.

The standalone owned-detour SDK fixture also links successfully: `ue4ss-native-owned-detour-1/link-2/receipt.json` records 77 imports covered by actual SDK exports and valid dynamic geometry. `manifest.json` pins the final production sources, objects, inline headers and host run. This standalone link was separate from the then-current full UE4SS link, which still lacked `ps_scan`; the later link-11 milestone below closes that source/link gap. That standalone fixture was unexecuted at the local-link checkpoint; the separate reviewed console artifact below subsequently exercised the exact owned INPLACE path.

## Completed owned detour console evidence

One reviewed dispatch at **21:03:34 UTC on October 2** passed on the PS5. `ue4ss-native-owned-detour-console-1/console-state.json` records dispatch count 1; its matching entry/result receipts report phase `complete`, return 0, AsmJit error 0 and all 1023 completion flags. Genuine emitted source returned 42, the explicit INPLACE hook returned 84, its real trampoline returned 42, and unhook restored 42. Both generated functions were 166 bytes. The real seven-byte prefix-write refusal restored all 100 checked source bytes and return 42, revoked its trampoline and cleared its handle; normal unhook revocation and genuine generated-function release also passed.

The 2,764,064-byte ELF has SHA256 `3a0e25777e16499dcc7ca3cf1f04880ce100be26406eefd5fc11eeb694932a1f`. Only its fresh owned payload and two UUID-scoped receipts were written. Game files, autoload and FTP service controls were unchanged; no engine/game target or downloaded mod ran. This supersedes earlier “not executed on PS5” qualifications only for that exact owned generated-function INPLACE path and its exercised allocator/service/trampoline lifetimes. Other schemes, concurrent lifecycle and persistent-refusal cleanup remain unproven. **Do not redispatch the completed transaction.**

At the 21:03 console checkpoint, full UE4SS still lacked `ps_scan` and remained unlinked and not game-bound. The next implementation then was its genuine scanner binding over explicitly owned mapped images; the later library-link milestone below supersedes that linkage qualification only. `ue4ss-native-ps-scan-abi-1/manifest.json` freezes the exact shared 13-slot C/Rust ABI and typed caller; `ue4ss-native-read-lease-1/manifest.json` freezes the additive source read lease. Actual SDK compilation and host ASan/UBSan controls pass for the lease, including blocked service protect/free/write, actual permission splits and retained ModuleSnapshot ownership. The new lease itself has no PS5 runtime claim. Its mutex is nonrecursive: synchronous same-thread Rust reads must finish and release the caller before logging or other owned-service operations. Geometry/role admission does not establish engine identity or a callable engine ABI.

## Full SDK shared-library link milestone

The actual SDK link now succeeds with `--no-undefined` and `missing-providers.json` equal to `[]`. Link-10 is retained as the first complete symbol-closure result. Link-11 is authoritative after the existing caller-local script resets dynamic-segment orphan inheritance; `linked-library.json` verifies the 432-byte `PT_DYNAMIC`, section match and terminal `DT_NULL`. No SDK files were changed.

`ue4ss-native-ps-scan-provider-1/integration-manifest.json` freezes the actual binding/source/config propagation and all 27 coherently rebuilt C++ consumers. The genuine owned Rust/C++ host harness passed 279 collector assertions and source controls with four success/three failure callbacks. Source owners stay retained; callbacks occur after the same-thread nonrecursive read lease ends. Constructor admission requires already registered owned spans and derives real permissions. The fragment-fixed Rust object retains thirteen exports and all original resolver algorithms; same-byte ELF/PE metadata splits preserve exact roots/results, and gaps refuse.

This establishes successful native source compilation and SDK shared-library linking. The full library remains unloaded, unexecuted and game-unbound. An actual admitted game-image producer, engine/reflection lifetime validation, one genuine game-thread callback and unchanged Item Spawner remain; DLL compatibility and the application follow. No game/process source discovery, fake scanner success, console dispatch or game writes occurred in this link checkpoint. Earlier milestones and completed one-dispatch owned tests remain historical evidence.

## Owned mapped-source console evidence — 21:45 UTC

One root dispatch at 21:45:43.011160 UTC passed the actual native owned mapped-source/collector path underlying the scanner binding. `ue4ss-native-owned-mapped-console-1/console-state.json` and its exact entry/result receipts report return 0, phase `complete`, flags 1023, masks 8191/1117/0, fixture version 5.7 and four success/four failure callbacks. All thirteen slot statuses match `[0,3,0,0,0,-1,0,-1,-1,-1,0,-1,-1]`; five actual addresses point into the fixture-owned mapping. Required absence, source-owner mismatch, missing source, real permission refusal, callbacks after lease release and retained-owner/release checks completed. The reviewed 1,489,792-byte artifact has SHA256 `4cd7a8adb53a542da471ada885084789c551ba91c192ad3a941f352ea780f9a3`.

The fixture is compiler-owned PIC bytes materialized into genuine owned mappings and decoded without executing its function bytes. No game/process source was discovered or registered, and no game memory, downloaded mod or game hook was accessed. This is native component evidence for the actual source/collector used by `ps_scan`; its GlobalConfig entry wrapper and the full shared library were not exercised by this payload. Fixture version 5.7 and returned owned addresses establish no actual game identity, engine ABI or Item Spawner behavior.

The earlier owned INPLACE test passed at 21:03 UTC with 42→84/trampoline42→restored42 and rollback/revocation/release. Both console transactions are complete and must not be redispatched. Full SDK library link-11 remains valid, but the library is still unloaded, unexecuted and game-unbound. Only the fresh owned mapped-test payload and its two receipts were written for this test; game files, autoload and FTP service state were unchanged. Actual admitted game-image production, reflection/lifetime validation and a genuine game-thread callback remain before unchanged Item Spawner, then DLL compatibility and the application.


## Native engine-property integration

Stage18 adds the genuine BuildConfiguration and Stats classifiers to optional result slots 11 and 12. This supplies evidence used to choose Unreal object layouts; it does not itself validate any game layout. The real Rust/C++ integration passed 303 local assertions plus four successful and three failure callback controls. Compiler-owned ELF markers produced Development/StatsOn and Development/StatsUnknown; insufficient markers stay unknown, and disabled fields retain prior values. The existing required mask and public result layout remain unchanged. Only the collector and the single Rust runtime were replaced; the production library contains no fixtures. These additions have local execution and PS5 compilation/linking evidence, with no new console execution.


## Default owned-hook integration

The default recommended hook mode exposed a real source-layout error: PolyHook returns an eight-byte callback holder and a separate six-byte source jump, while the previous native writer expected one contiguous patch. The native writer now handles those owned regions separately and restores the source before releasing its holder. Actual local x86 execution passed original42 → hooked84, trampoline42 and restored42, including partial-write recovery, failed unhook retention and active destruction. The original INPLACE test also passed. Stage19 replaces only the production x64Detour object and links cleanly. This new default path has not executed on the PS5; the earlier INPLACE console pass is separate evidence.


## Additional mapped-address candidate methods

Three separate ELF SysV endpoints now apply the generic object-array CFG, five-anchor name-constructor and allocator receiver/free strategies directly to supplied owned spans. Two distinct owned allocations produce different addresses derived from their encoded operands. Local tests cover missing anchors, clobbered arguments, ambiguity, incomplete competitors and unsuitable data/code permissions; 52 tests passed. The single native Rust object retains all fourteen existing exports and adds these three candidates, and it compiles and links with the SDK. They are not yet used by the production collector or any automatic fallback, and they do not establish live engine identity or callable ABI. No game data or console execution was used for this checkpoint.


## Full-production standalone console result

At22:24UTC on October2, one dispatch of `ue4ss-native-full-startup-1` passed. The40,068,192-byte executable contains all349 production C/C++ units and the single14-export Rust runtime, with genuine SDK startup and no linker section GC. Its receipts recorded Lua result42, both expected invalid-context construction/init/destruction paths, five unpublished-program getter rejections, and the actual GlobalConfig ps_scan wrapper returning failure with exact preserved/cleared outputs and three error callbacks. All expected flags were present and the test returned0. The shared-library `.so` itself was not dynamically loaded; this standalone used the same production object composition.

This is real console execution of the exercised Lua, lifecycle and scanner-binding paths. Successful engine initialization, reflection, game-thread callbacks and ItemSpawner remain pending. The new default hook and property classifiers were linked into this artifact but were not exercised by this trial, so their runtime claims remain unchanged. `ue4ss-native-full-startup-1/native-result-evidence.json` pins the console source, step and status receipts. The transaction is complete and must not be redispatched.

## Default hook native execution and saved-image candidates

At22:41UTC, the new default `RECOMMENDED` hook path passed one actual PS5 standalone trial. It selected `VALLOC2`, redirected owned generated code42→84, called the original trampoline42, and restored42 with the complete100-byte source snapshot unchanged. Its separate partial-holder-write refusal left the source unchanged; both failed and successful lifecycles retired holder/trampoline resources, and the genuine AsmJit owner retired its mappings. Artifact SHA256 is `187e891d253be84c640ca0ec199f702324ff74278e1c50f2d91692e14505aa2e`; receipts are in `ue4ss-native-owned-default-detour-console-1`. This does not establish a game hook.

The unchanged saved Townfall ELF also passed a new offline reconstruction trial: all three strict shared algorithms produced unique GU/FName/GMalloc analysis candidates with no resolver changes or per-game addresses. Virtual spacing and zero-filled tails were reconstructed from validated load spans; no relocations were applied and ELF flags were only declared analysis roles. The frozen mapped C endpoints were not called. These are file-coordinate evidence, not live pointers or proof of game layouts. Exact evidence is in `ue4ss-native-saved-reconstruction-1`; its captured input hash remains unchanged.

## Native settings and explicit candidate selection integration

The PS5 defaults now disable absent desktop console/GUI/crash services and leave input selection empty for a supplied native provider. Genuinely absent settings retain defaults without creating a file; errors/nonregular selected paths still fail. Review found original SettingsManager never parsed General.InputSource. The source-only successor now retains explicit provider requests for existing validation. Five native compile-time assertions, actual host filesystem cases,13 affected production consumer rebuilds, and full stage21/link15 succeeded. Stages20/21 and source versions remain frozen separately. Settings file path selection preserves its original legacy fallback; the helper's symlink rejection applies to the final selected path. The later stage21 console trial below verifies actual no-INI defaults, program construction and teardown.

The mapped candidate successor adds immutable OriginalOnly(default), CollectEvidenceOnly and explicit SelectUniqueAfterNoMatch policies. A separate report retains the complete original result, typed endpoint evidence and selected-result provenance. Only original NoMatch/status3 without image error permits fallback; original available/ambiguous/invalid/incomplete outcomes are never replaced. Witness/output roles are checked under the retained read lease, with provenance callbacks afterward.87 genuine owned-fixture assertions pass; three actual consumers rebuilt and one adapter added. Stage22/link16 combines all of this with one17-export Rust object. Neither optional selection nor its successful local tests establishes engine identity, layouts or callable ABI, and no game source was admitted.

## Actual PS5 program construction

At 2026-10-02T22:59:33.904777+00:00, one stage21 standalone dispatch passed actual program construction and publication with no INI, no Mods and the five intended native defaults. Its real init returned the exact missing-scanner error before engine initialization. Program publication was revoked; destructor and output-device cleanup completed; the retained owned module mapping retired; relevant engine flags/version/source pointers stayed unchanged. All 1023 observation flags and every typed receipt matched. Artifact SHA256 is `d359b4dfd4c4e7cae47ccfcd140ac0b041c04c3ceb46ff83a3a95369ae593f96`; `ue4ss-native-program-construction-console-1` retains the source/step/status/log receipts.

The module was an owned compiler-produced PIC fixture and its code was not executed. This proves the native startup preparation path after the settings fixes; it does not prove game-bound initialization, reflection or a game-thread hook. No game files, autoload or service settings changed. Stage22's separate candidate-selection addition remains locally tested/linked, without a native execution claim. Both completed console transactions are retained and must not be redispatched.
