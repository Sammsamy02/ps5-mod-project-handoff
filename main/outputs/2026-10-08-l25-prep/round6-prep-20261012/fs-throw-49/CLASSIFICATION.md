# fs-throw-49 — what to do with the 49 throwing filesystem calls

All of this is **Mac-only**: I read the source on the Mac on 2026-10-12. There was
no console contact and I read no console logs. No build or package was made.

## Verdict (plain)

- **None of the 49 calls can crash the game on PS5 in the current build.**
- **33 of them never run on PS5.** They belong to the PC way of finding mods on
  disk. On PS5 the mods come from the activation bundle, so the code skips those
  lines before it reaches them (or nothing calls them at all).
- **16 can run on PS5, and all 16 are already caught.** The worst outcomes:
  - at startup, the mod runtime refuses to start for that launch (the game keeps running);
  - from a Lua function, the mod that asked gets a Lua error instead of a
    crash. For most rows PC does the same. For rows 21 and 26 it is different:
    PC normally finds the game's Content folder, so PC never sees that error.
    If PS5 does hit it, it would happen on every launch (see row 21 and D2).
- **So no fix build is needed for safety.** Converting all 49 now would mostly
  change PC code that PS5 never runs. I recommend keeping them on the baseline
  list (tracked debt), not spending a build on them.
- **One optional small fix is staged (P1, 2 calls in `PosixFile.cpp`).** It makes
  the PS5 file layer report "could not create folder" errors the same way PC
  does. It is gate-checked, compiled and host-tested. It is not urgent.

Counts: 49 = 33 UNREACHABLE-ON-PS5 + 16 HANDLED + **0 UNHANDLED**.
`check_classification.py` checks this. It matches all 49 entries against the
gate's own scan of the current tree and the baseline keys. For the 266 file:line
citations it only checks that the line exists in the file (a bounds check, not
the code text). The code text is checked for 97 load-bearing lines (anchors).
It also checks that the two factories without a mod source have no caller
(4,037 files scanned). Result: PASS, `check-classification-result.txt`.

## Why most of them never run on PS5 (fact R0, two independent sources)

On PS5 the native mod source is always set:

1. The only start path is `runtime_start_bridge.cpp:1995`. It goes to
   `runtime_ack_supplier.cpp:87` and then to `PrepareActivatedUE4SSRuntime`,
   which sets `context.mod_source = MakeActivatedUE4SSModSource(...)`
   (`ue4ss_backend.cpp:88-90`).
2. `activated_mod_source.cpp:45` returns `make_shared` and never null. On any
   problem it throws instead (`:23,26,39,41`).
3. Two compiled factories build the program **without** a mod source:
   - `PrepareDeferredUE4SSRuntime` (`ue4ss_backend.cpp:79-82`);
   - `CreateNativeUE4SSProgram` (`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeProgramFactory.cpp:5-22`,
     `make_unique<UE4SSProgram>` at `:14`; its context comes from
     `MakeNativeProgramContext`, `NativeLoaderContext.cpp:7-22`, which never sets one).
   Nothing calls either one: not in the 3,561 compiled files, the 335 unscanned
   prebuilt sources or the package lineage. They appear only at their own
   declarations and definitions (`check_classification.py` check 5 proves this).
   The only other place that builds the program is `ue4ss_backend.cpp:47`, inside
   the backend that `PrepareActivatedUE4SSRuntime` creates at `:102` after
   setting the mod source. Nothing resets the mod source.

Every PC disk-scan branch checks this first and returns
(`UE4SSProgram.cpp:872, 930, 1018, 1964, 2174, 2837, 2941, 2955`;
`LuaMod.cpp:1124/1128, 1435, 8235, 8318`; `Signatures.cpp:43`).

Thread safety nets (all Mac-only source facts):

- PS5 `update()` (the event loop) is inside `try` `U:1689`, with catches at `U:1894/1899`.
- `init()` is inside `try` `U:664`, with a catch at `U:750`.
- The init thread is caught at `runtime_owner.cpp:99/123/131`.
- The constructor is inside `try` `U:222`, with a catch at `U:488`.
- The bridge catches at `runtime_start_bridge.cpp:2007/2018/2029`.
- Every registered Lua function runs inside `TRY` (`LuaMadeSimple.cpp:1726`,
  `LuaMadeSimple.hpp:843-857`). `TRY` turns a C++ exception into a Lua error
  after the handler exits.
- The PS5 compiler wrapper builds with `-frtti -fexceptions` (`prospero-clang++:83`).

## The 49 sites

Line numbers are for the current tree (B4 state). The 1e49188b evidence file has
older numbers for some sites; for example `LuaMod.cpp` 3277 is now 3356. The
gate keys do not use line numbers.

Abbreviations:

- **U** = `candidate/src/UE4SSProgram.cpp`, **L** = `candidate/src/LuaMod.cpp`
- **TRY** = the Lua-function TRY, which gives the calling mod a Lua error
- **ctor catch** = `U:488`, so the runtime refuses to start and the game runs on

Port code (14 calls):

| # | Site | Function | PS5 path | Verdict | What catches it / what happens |
|---|---|---|---|---|---|
| 1 | NativeCppModPath.hpp:26 is_regular_file | FindNativeCppModModule | C++ mod load | UNREACHABLE | CppMod is only built by the PC disk scan (`U:2018`); `CppMod.cpp:107` would catch |
| 2 | NativeProgramContext.hpp:131 is_regular_file | ValidateNativeProgramContext | startup, every launch | HANDLED | bridge `:2018`/`:2007` + ctor catch → start refused, game runs on |
| 3 | NativeProgramContext.hpp:132 is_regular_file | same | startup, only if an exe path was observed | HANDLED | same |
| 4 | NativeProgramContext.hpp:135 file_size | same | same | HANDLED | same |
| 5 | Generator.cpp:583 exists | cleanup_old_sdk | Lua `GenerateSDK`/`GenerateLuaTypes` | HANDLED | TRY → Lua error |
| 6 | Generator.cpp:588 directory_iterator | same | same | HANDLED | same |
| 7 | UEHeaderGenerator.cpp:3830 exists | dump_native_packages | Lua `GenerateUHTCompatibleHeaders`; debug key Ctrl+Num9 | HANDLED | Lua: TRY. Key: `U:1894` → mod runtime stops, game runs on (PC would crash here) |
| 8 | UEHeaderGenerator.cpp:3832 remove_all | same | same | HANDLED | same |
| 9 | UEHeaderGenerator.cpp:4250 create_directories | serialize_file_content_to_disk | same | HANDLED | same |
| 10 | Signatures.cpp:46 exists | signature_script_exists | startup | UNREACHABLE | `:43-44` answers from the mod source first |
| 11 | ProcessEventProfiler.hpp:182 create_directories | DumpStatsToFile | none | UNREACHABLE | no caller anywhere |
| 12 | ObjectSearcherProfiler.hpp:245 create_directories | DumpStatsToFile | none | UNREACHABLE | no caller anywhere |
| 13 | NativeLayoutBinding.cpp:47 is_regular_file | ReadBounded | startup | UNREACHABLE | `U:1018` takes the mod-source branch |
| 14 | NativeLayoutBinding.cpp:47 symlink_status | same | same | UNREACHABLE | same |

Candidate tree (35 calls):

| # | Site | Function | PS5 path | Verdict | What catches it / what happens |
|---|---|---|---|---|---|
| 15 | PosixFile.cpp:267 create_directories | PosixFile::open_file | first log line (startup); Lua dumps/generators | HANDLED, **P1 optional** | ctor catch / TRY. PC differs: see P1 |
| 16 | PosixFile.cpp:390 create_directories | persist_cache | none (serialization API unused) | UNREACHABLE, P1 optional | — |
| 17-19 | L:1146, 1149, 1157 exists | LuaMod::LuaMod | mod load | UNREACHABLE | `L:1128-1140` mod-source branch returns first |
| 20 | L:1500 exists | custom_module_searcher | `require()` | UNREACHABLE | `L:1435-1451`. Latent: no catch if ever reached |
| 21 | L:3356 exists | IterateGameDirectories | Lua; the self-test calls it each launch (call 134) | HANDLED | TRY → Lua error, no crash. **Not PC-equivalent in practice:** PC finds Content, so it never raises this. On PS5 a non-"not found" stat error would fail call 134 every launch, skip the B3 layout probe (`L:3358-3417`, after the throwing `L:3356`), and give any mod calling it a Lua error. See D2 |
| 22-25 | L:3617, 3622, 3626, 3633 exists | `__index` `__files` branch | Lua, rare (to_wstring failure + LogicMods path) | HANDLED | `RC::TRY` `L:3537` → log line, Lua gets nil |
| 26 | L:3751 exists | CreateLogicModsDirectory | Lua | HANDLED | `L:3805` catch → Lua error (raised inside the catch, see D3) |
| 27 | L:8243 exists | load_and_execute_script | mod start | UNREACHABLE | `L:8235` mod-source branch first |
| 28 | L:8318 exists | start_mod | mod start | UNREACHABLE | `m_native_source ||` short-circuit |
| 29 | U:465 file_size | constructor | startup, only if an exe path was observed (`U:464`) | HANDLED | ctor catch (PC also catches → no start) |
| 30 | U:886 directory_iterator | setup_paths | startup | UNREACHABLE | `U:872-879` |
| 31-32 | U:909 exists ×2 | setup_paths | startup | UNREACHABLE | `U:872-879` |
| 33-34 | U:969 exists ×2 | setup_mod_directory_path | startup | UNREACHABLE | `U:930-951` |
| 35-38 | U:1982, 2015, 2017 exists; U:1990 directory_iterator | setup_mods | mod setup | UNREACHABLE | `U:1964-1977` |
| 39-44 | U:2195, 2210, 2216, 2227, 2298 exists; U:2305 directory_iterator | start_mods | mod start | UNREACHABLE | `U:2174-2186` |
| 45 | U:2781 exists | queue_start_lua_mod_by_path | none | UNREACHABLE | no caller anywhere |
| 46-48 | U:2851, 2860, 2866 exists | get_mods_txt_entries | none | UNREACHABLE | no caller, plus `U:2837-2843` |
| 49 | U:2929 weakly_canonical | make_compatible_path | none | UNREACHABLE | all 4 callers are behind PS5 guards. The source comments say this is the only one that really throws on PS5 |

`classification.json` has every row in full: caller chain, thread, handler,
what happens if it throws, and PC behaviour, each with file:line.

## P1 (optional) — PosixFile folder creation, PC parity

- **PC** (`WinFile.cpp:323-338`): it catches the filesystem error inside the
  file layer, sets `internal_error` (`File/Macros.hpp:5-7`) and throws a File error.
- **PS5** (`PosixFile.cpp:267, 390`): the `std::filesystem::filesystem_error`
  escapes and `internal_error` is never set.
- **P1** adds `create_parent_directories()`. It calls the `error_code` overload.
  On error it sets `internal_error` and throws `std::system_error`. That is a
  `std::runtime_error`, the same family `io_error` in the same file uses.
- Success path: unchanged.
- No handler in the compiled set catches `filesystem_error` or `system_error` by
  name, so every existing catch behaves the same.
- Files:
  - `patches/P1-posixfile-parent-dirs.patch` (unified diff)
  - `patched/.../PosixFile.cpp` (sha256 `04e89a43…5326`)
  - the source it was made from is sha256 `7ae47172…9df8`

Checks:

- **Gate** (`gate/`, rerun with `rerun_gates.sh`: 8 runs, all with the expected exit code):
  - Current tree vs baseline: 3,561 files, 68 calls, 19 safe, 49 violations →
    49 known / 49 present / 0 resolved / **0 NEW**, PASS.
  - With P1 (rebased baseline): 3,561 files, 67 calls, 20 safe, 47 violations →
    49 known / 47 present / **2 resolved / 0 NEW**, PASS.
  - Control copy with the same rebasing: 49 / 49 / 0 / 0. This proves the
    rebasing alone changes nothing.
  - Patched file alone: 1 file, 1 call (error_code), 0 violations. Self-test 23/23.
- **Syntax** (`syntax_check.py`, one compile at a time, more than 2 GiB free before each; 6 of 6 OK):
  - Mode 1 is the build's exact argv for the posix-file unit (`prospero-clang++`
    with `-fsyntax-only`, the same `LLVM_CONFIG` as `build_runtime.py:98`).
  - Mode 2 is host `/usr/bin/clang++` with the project's PosixFile host recipe
    (`logwriter-impl-verify/build_and_run.sh:87-95`).
  - Control and P1 pass in both modes. A deliberately broken copy is rejected in
    both modes, so the check really compiles the new code.
- **Host test** (`host-test/`, 6 of 6 checks per binary):
  - Success path: both copies create the missing folders and leave
    `internal_error` false.
  - Error path, CONTROL: throws `filesystem_error`, `internal_error` stays false.
  - Error path, P1: throws a non-filesystem File error with `internal_error` true,
    which matches PC.
- Side effect, the same as PC and as the existing `io_error` paths: after one
  folder-creation failure, later `RC::TRY` errors in that session print to stdout
  instead of UE4SS.log.

## Extra findings (outside the 49)

- **Gate scope (Mac-only).** 185 of the 390 objects linked into the runtime are
  prebuilt. 335 of their sources/headers (SDK headers excluded) were never in the
  gate's file list. I scanned them: 0 filesystem calls.
- **Package lineage (`2026-10-08-l26-package-A/lineage`, compiled at packaging).**
  The gate finds 6 calls there:
  - 3 are a byte-identical copy of `NativeProgramContext.hpp` (sha `0a068bb9…`),
    so the same 3 sites as rows 2-4.
  - 3 are false positives at `runtime_start_bridge.cpp:529/533/538`. They do pass
    an error_code, but it is named `ea`/`eb2`/`ed2`, which the gate's name rule
    does not recognise.
  - Seen in passing: `:538` hard-codes the Townfall title id in a diagnostic.
    This is already known from the morning report.
- **`LuaMod.cpp:3808`** raises a Lua error from inside a `catch` block. Lua is
  built as C here, so the caught exception is never freed. This is the same leak
  class B2 fixed in the TRY wrappers. It is not a crash and not filesystem-specific.

## Unknowns and assumptions

- That a C++ `catch` really works on PS5 is assumed. The constructor, init,
  bridge and event-loop catches all depend on it. Source comments describe earlier
  launches where catches fired, but I did not verify it this session. If it did
  not work, every throw would abort the game, not only these 49.
- Which error PS5 returns for `stat("/Content")` (used by rows 21 and 26) is
  unknown. ENOENT means no throw; another errno means a Lua error on every
  launch. A B4 launch-1 harvest is already in the evidence tree and may answer
  this, but this track is not allowed to read console logs. The driver session
  should check the self-test line for call 134 there (decoded with
  `harvest/decode_session_log.py`).
- R0 relies on the prebuilt `activated_mod_source.o` and `runtime_ack_supplier.o`
  matching their sources on disk. I did not re-check this byte for byte.
- `EnableDebugKeyBindings` (only matters for the row 7-9 debug-key path) is
  **most likely false, so that path is most likely off.** Two sources: the code
  default is false (`SettingsManager.hpp:38`), and today's read-only copy of the
  console ini says `EnableDebugKeyBindings=false`
  (`evidence/morning-readonly-20261012T114008Z/spool/rows/ue4ss/UE4SS-settings.ini:13`,
  FTP RETR at 11:40Z). Caveat: the B4 deploy ran after that copy. Whether it
  changed the ini is unknown; I did not read the deploy log.

## Decisions needed

- **D1:** keep the 49 calls on the baseline as tracked debt and do not spend a
  labelled build on them (recommended).
- **D2:** check the self-test line for call 134 in the B4 launch-1 harvest (the
  driver session, not this track). If it shows a filesystem error on `/Content`,
  decide whether a stat error should count as "not found". That would let the B3
  layout probe and `IterateGameDirectories` work on PS5. It differs from PC
  behaviour only in an error case PC never reaches. Not staged.
- **D3:** `L:3808` raises a Lua error inside a catch block (leak, B2 class). Fix
  in its own build, or fold it into the LUA_COMPILE_AS_CPP decision.
- **D4:** optional P1. Bundle it into the next labelled build that touches
  `PosixFile.cpp` after Townfall acceptance, or drop it. If it lands, shrink the
  baseline by 2 with `--write-baseline` in that build.
- **D5:** optionally widen the gate scope to the package lineage and the prebuilt
  sources. The 3 false positives at `runtime_start_bridge.cpp:529/533/538` would
  then need renaming (`ec_*`) or an approval marker.

## Changes after the adversarial review (Mac-only)

- R0 now names the second factory without a mod source
  (`CreateNativeUE4SSProgram`). The verdict does not change: nothing calls it.
  `check_classification.py` now proves "no caller" for both factories.
- `EnableDebugKeyBindings`: now backed by two sources (see above).
- Rows 21 and 26: no longer called "the same as PC".
- The checker description is now accurate: citations are bounds-checked only,
  anchors are checked for their code text (97 anchors now, was 89).
- Write scope: see README.md. The first two gate self-test runs of the earlier
  pass used the system temp folder (cleaned up by the gate itself).
