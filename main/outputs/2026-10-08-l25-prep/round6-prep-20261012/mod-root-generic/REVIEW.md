# Adversarial review of revision 2: mod-root-generic (2026-10-12)

All checks here are **Mac-only**. No console contact, no PRX build, no git changes.
The only project file written is this one. Every re-run used copies in this
session's scratch folder. The review of revision 1 is kept unchanged at the end.

## Verdict (plain)

**Most of revision 2 holds up. One safety claim is wrong.**

The design says that when the runtime cannot tell which game is running, it loads
no mods. That is not always true. Say both Townfall and Stellar Blade are listed in
ploader.ini, and Stellar Blade is running. If the runtime cannot find or open
Stellar Blade's own game file (`param.json`), but an old Townfall copy can still
be read, the runtime picks **Townfall**. It then loads Townfall's mods into
Stellar Blade. I showed this on the Mac with the worker's own test program and
two added cases. Two other gaps are smaller.

Nothing found is unsafe while the patch stays staged. The safety claims in DESIGN
must be fixed, and the code or the decision text changed, before the patch goes
into a build.

## What I checked and confirmed (Mac-only)

| Claim | Result |
|---|---|
| 0001 sha `3ccf0364…`, staged entry sha `721c4037…` | Confirmed. The patch applies to the compiled entry (`bc490893…`, the same file as package-A `lineage/lifecycle`) and gives the staged file byte for byte. The staged file has 0 hits for `PPSA`, `ownfall` or `CUSA`. |
| 0002 sha `a804068e…` | Confirmed. It gives both staged self-test files byte for byte, `luac -p` passes, and 0 `PPSA33286` remain. |
| Shim key is the same for the old and new ROOT | Confirmed in the C++ shim itself: `LuaPathShim.cpp:206-282` strips the leading `/` and anchors at the `ue4ss` part, so both give `ue4ss/Mods/ueselftest/progress.txt`. |
| A9: all games share one writable area | Confirmed at `LuaPathShim.cpp:476-483` and `:1685-1687`. |
| Host test 35/35 + 17/17, unbound 1/1 | Confirmed (re-run). |
| Mutants 6/6 killed | Confirmed (re-run). |
| Self-test root Lua check 6/6 | Confirmed (re-run). |
| Binding check survives the PS5 compile | Confirmed in `evidence/staged-runtime_module_entry.o`. `mbt_read_file` loads all three addresses from the GOT and compares each with 0 before `sceKernelOpen`, and the open flags are `0x100` (FreeBSD `O_NOFOLLOW`). |
| ini grammar | All 6 local ploader `main.c` copies have the same INI PARSER section (diffed). The cited line numbers match the 851-line copy. |
| app-info layout conflict (section 3.2) | Confirmed: ploader `main.c:101-106` and SDK `test_privileges/main.c:49-54` versus SDK `ps/main.c:29-35`; `sceKernelGetAppInfo` is not in `EXACT-IMPORT-IDENTITIES.json`. |
| Rules | The worker's scripts have no network calls, no console IP, no /tmp evidence and no Epic text. The originals are unchanged (`bc490893…`, `ueselftest/main.lua` `318f68e0…`). The 06:55-07:58 CDT B4 launch files in l25-prep come from the driver session's launch, not from this track. |

## Problems

### Major

**R1. "Fails closed" is wrong when the running game's own `param.json` is not found.**
- `mbt_read_file` treats *every* failed open as "absent" (`staged/lifecycle/runtime_module_entry.c:410-412`: `if (fd < 0) return -1;`).
- The probe loop then just skips that path (`:961-962`, `continue; /* no such live instance */`). Only instances `_000`-`_099` are probed.
- So when the running title R's file is missing or cannot be opened, and another configured title T still has a readable sandbox, the runtime confirms exactly one title, **T**. It does not refuse.
- It does not have to be missing: it can be past `_099`, or the open can fail (permission, symlink, not mounted at `module_start`). `mfa_activate` does not catch this either, because T's folder names T.
- The worker's own fixture `agree_other_title_two_sections` (`host-test/make_fixtures.py:42-44`) expects exactly this pick: Townfall's sandbox has no `param.json`, and the other title is chosen.
- Mac reproduction: a scratch copy of the worker's test with two added cases. ini enables PPSA33286 and PPSA13197. PPSA33286_000 is readable, and the running PPSA13197 is either at `_100` or its `param.json` is `chmod 000`. Both cases returned **0 with root `/data/modbridge/roots/PPSA33286`**, not -108/-109.
- The text that this contradicts:
  - DESIGN `:208-209` ("A wrong pick would need two configured games with live sandboxes at once; the patch then refuses (-109)")
  - DESIGN `:218-220` ("relaunched more than 100 times … would fail closed (-108)"; "stale but still-mounted … would give -109")
  - report risk 1
  - decision 7.1, which frames shipping on the two console-wide sources as relying only on "one game at a time".
- **What it really relies on:** no *other* configured title has a readable stale sandbox, AND the running game's file opens at `_000`-`_099`. Whether closed games' `app0` stays mounted is unproven (DESIGN 3.4 says "suggests").
- **Fix options:**
  1. Fail closed on any open error other than "not found". That needs the SCE ENOENT value from two sources.
  2. Fail closed when `<T>_099` exists, instead of stopping the probe there.
  3. Make the process-identity source (3.2) required before any game other than Townfall is enabled in ploader.ini.
- At the least, correct the three statements above and the decision text, and add the two cases as must-fail fixtures.

### Minor

- **m1. The tool inventory stops at l25-prep, but the plain summary reads as complete.**
  - The plain summary says "21 Mac tools … need a game setting". Build-chain tools outside the scan root also hard-code Townfall:
    - `2026-10-08-l26-runtime/profile/generate_overlay.py:48,56,60,66,92` (`profiles/townfall-build-facts.json`, `compiled-audited-townfall-l25`)
    - `profile-fix-template/*` (5 files)
    - `2026-10-08-l26-package-A/package_prx_l19.py:888-890` (`title_id "PPSA33286"`, `/data/townfall_supplier_composition.log`, the same kind of values as A7)
  - Item A7 should name its generator, and list E should state its scope or add these files.
- **m2. One value with a single source is not listed.**
  - `O_NOFOLLOW` = `0x100` is shipped in the open flags (seen in the object), but the values table (3.6) does not list it.
  - It is safe: if the kernel rejects the flag, every open fails, which gives -101 or -108. It should still be listed with two sources, or marked as policy.
- **m3. Console step 6.1 reads `param.json` over FTP while Townfall runs.**
  - CLAUDE.md section 1 notes that FTP reads under game load come back incomplete. The step does check the size (3,485 B).
  - It should say that a short read is a retry, not evidence.
- **m4. Evidence location.**
  - Evidence sits in `mod-root-generic/evidence/`, not in `outputs/2026-10-08-l25-prep/evidence/` (CLAUDE.md section 5). This is forced by the track's write limit. The driver should copy it there when the build is approved.
- **m5. "ploader main.c" is cited without a path in DESIGN and in the entry comments.**
  - There are 6 local copies in 3 versions (851, 858 and 914 lines). The grammar is identical in all of them, but the cited line numbers fit only the 851-line copy.
  - Name the file, for example `.superpowers/native-dependency-spike/ploader-cleanup-candidate-1/original/main.c`.

## Not found

- No console contact.
- No writes outside the track folder by this track.
- No changes to protected paths.
- No game or mod names in the staged runtime entry.
- No gate that passes on zero items: both scanners, `make_fixtures.py`, the host test and the mutant runner all fail on zero.
- No unlabelled claims.
- No Unreal source.

---

# Previous review (revision 1), kept unchanged

# Adversarial review: mod-root-generic (2026-10-12)

All checks here are **Mac-only**. No console contact, no PRX build, no git changes.
The only file written in the project is this one. Re-runs of the tests used copies
in this session's scratch folder (not evidence, and not stored in /tmp evidence).

## Verdict (plain)

**The patch and its tests hold up.** I re-ran them and got the same results.
**Two claims in the write-up do not hold:**

1. **The list of Townfall-only parts is incomplete.** The self-test that must run
   in the Stellar Blade test has Townfall's mod folder built in. So do the mod
   manager and several other tools.
2. **The design skips the strongest title source.** It says reading the game
   process's own title would need a new import. But the runtime already imports
   `sysctl` and already uses it at that moment.

Nothing found is unsafe to keep as a staged patch. Both problems should be fixed in
DESIGN.md before the build is approved.

## What I checked and confirmed

| Claim | Result |
|---|---|
| Compiled entry = `2026-10-08-combined-outer-lifecycle/candidate-v2/runtime_module_entry.c`, sha `bc490893…` | Confirmed: `l26-package-A/BUILD-INPUTS.json` `outer_entry`, `package_combined.py:53`, `pass-1/build/runtime_module_entry.d:2`. The B4 snapshot (63 files) does not contain it. |
| Patch applies and gives staged sha `38eed77f…` | Confirmed on a scratch copy, byte for byte. Patch sha `76130491…` matches. |
| No game names in the staged file | Confirmed: 0 matches for `PPSA` or `ownfall`. |
| Host test 35/35 + 17/17 PASS | Confirmed (re-run from a scratch copy). |
| Mutants 5/5 caught | Confirmed (re-run). |
| PS5 compiler, packaging flags, `-fsyntax-only` exit 0 | Confirmed with the exact flags from `pass1-controller.log:7` (staged and original both exit 0). |
| Only new undefined symbol is `sceKernelPread`, already imported | Confirmed against the real B4 object; it is in `EXACT-IMPORT-IDENTITIES.json`. |
| Stack and memory cost | Confirmed: `module_start` frame grows from 760 to 1,896 B; each JSON nesting level adds about 128 B; one 64 KiB `.bss` buffer. |
| ini parsing copies ploader's rules | Confirmed against `main.c:579-683`. Injection matches on the app-info title at `:767-770`. |
| Rules | No network in any script, writes stay in the folder, no console IP, no Epic source, claims labelled Mac-only. |

## Problems

### Major

**M1. The inventory misses the self-test, which runs in-game with Townfall's folder built in.**
`ueselftest/gen_selftest.py:737` and the generated `ueselftest/main.lua:18` contain
`local ROOT = "/data/modbridge/roots/PPSA33286/ue4ss/Mods/ueselftest"`.
`PFIND` tries that path first, opening it with `"w"`. Townfall's folder exists on
the console, so on Stellar Blade the self-test would write and resume its progress
in Townfall's folder. That is cross-game, mislabelled evidence, the same harm the
design flags for A7. Section 6.2 requires this self-test to run on Stellar Blade.
List E also misses `mod-manager/modmanager.py:95,152` (remote root and the
generated load list title), `round5-staging/shim-fix/deploy-precreate-writable.py:57`,
`host-tests/run-path-shim.sh`, `host-tests/run-regression.sh` and
`mod-manager/test_modmanager.sh`. That is 16 tools in l25-prep, not 10.

**M2. Neither source observes this process; the import-free process-identity route was not evaluated.**
Both sources describe the whole console: which titles are configured, and which
of them have a mounted sandbox. Source 2's path is built from source 1's title, so
the "disagree" code (-107) can only fire if a package is mounted under another
title's folder. The real identity link ("this process is that sandbox") is
inferred from "one game at a time".

DESIGN section 3 rejects `sceKernelGetAppInfo` as a new import. But `sysctl` is
already an exact import, and the runtime already calls it with `KERN_PROC` during
module_start (VMMAP; l24 diag `bridge_vmentry_entries`=22761, earlier session).
That function is, per public homebrew, a thin wrapper over a `KERN_PROC` app-info
sysctl. Called for `getpid()`, it would give exactly the title ploader matches on,
with no new import and no Sony SDK material. The mib value and struct layout
would need two public sources. Recommendation: evaluate it as the primary source,
with ini + sandbox as the cross-check.

### Minor

- **m1. A2's fail-closed reason is wrong; the conclusion holds.** DESIGN says that on
  another game, "open fails or whole code-image SHA256 mismatch". On this console,
  Townfall's decrypted executable stays at that `/data` path whatever game runs, so
  the open succeeds and the hash matches. What actually refuses is the live
  base/extent check (`CodeImageCache.hpp` prepare: "actual startup base/extent
  differs from pinned image") plus the A3 live anchors. This matters for the
  generator: the profile's file path is not tied to the running game.
- **m2. The "third check" is overstated.** `mfa_activate` gets its root and title from
  the same derived title (staged `:1062-1063`). It checks that the folder matches
  its own load list, not which game is running.
- **m3. A stale status file can be misread.** When the title cannot be derived, no
  `runtime-status.txt` is written (staged `:278-279`). The previous launch's file
  then stays in the Townfall root, and `harvest.py:129` reads that path. It could
  report an earlier session's status as this launch's (rule 5). Harvest should
  check pid/time or the UE4SS.log line.
- **m4. The sandbox evidence already shows numbers climbing.** The cited
  `ftp-sandbox-list.txt` lists `NPXS40093_000`…`_044` from about one day, with old
  folders left behind. A game relaunched more than 100 times without a reboot would
  fail closed (-108). A stale, still-mounted sandbox of another configured game
  gives -109. Section 6.4 lists this for console proof but does not say the local
  evidence already points that way.
- **m5. Unsafe apply wording.** README says "apply from the worktree root with
  `patch -p1`". That edits the frozen, sha-pinned and git-untracked shared input
  that every l24/l26 build restages (`package_combined.py:53`). After that, re-running
  any archived build would fail its pin check. DESIGN section 4 says to repoint
  `outer_entry` to the staged file instead. README should say: never patch in place.
- **m6. The syntax-check log has no command in it.** `target-syntax-check.log` holds
  only `target-syntax exit=0`. I reproduced exit 0 with the flags from
  `pass1-controller.log:7`; this needs the LLVM-21 bin folder on PATH.
- **m7. Binding discipline.** The original entry never calls a kernel import before
  `diagnostic_ready()` has confirmed it is bound (Open/Write/Close). The new
  `mbt_read_file` calls `sceKernelPread` unguarded and is now its first caller,
  before `mfa_activate`. The risk is low, since the bridge already calls it
  unguarded and earlier runs used it. A one-line non-null check would keep the
  rule.
- **m8.** `lineage/FNameAbiProfile.hpp` ("Generated game profile", a 32-byte FName
  prologue) is compiled in B4 but not named in the inventory. It is covered
  indirectly as A2 pin 1, but the generator's file list should name it.
- **m9. The ini rules have effectively one source.** The deployed ini only shows
  `[section]` and `path = 0|2` lines. Comments, the no-slash prefix, the `..` rule
  and long-line splitting come from `main.c` alone, and which ploader build is
  deployed is unproven (DESIGN admits this). A mismatch usually fails closed.
- **m10.** `scan_game_constants.py:213` fails only when the total count is zero. An
  input that yields zero paths passes silently.
- **m11.** "The driver only replaces `outer_bridge`" is not quite right:
  `derive_manifest` also appends two `outer_headers` rows and rebuilds
  `lineage_overlays` (`l26_packaging_driver.py:113-150`). The `outer_entry`
  conclusion is unaffected.

## Next step

Fix M1 (add the self-test and the tools to the inventory, and make the self-test
root come from its own folder) and M2 (evaluate the `sysctl` app-info route) in
DESIGN.md. The console steps in section 6 are unchanged; each still needs the
user's OK.
