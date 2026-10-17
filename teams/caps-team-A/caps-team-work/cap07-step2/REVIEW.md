# REVIEW — cap #7 step 2 (runtime pak mount, no file change)

Independent reviewer, 2026-10-12. Mac-only: no console contact. All claims below are
Mac-only unless they cite an existing console evidence file.

## Verdict: BLOCK (for the launch). The code is sound and generic; three cheap fixes are needed first.

The staged unit is generic and data-only, and most of its paths fail closed. It has no
game or mod names and no detours. I re-checked this with the repo gates and by reading
the code. But as written, the one launch would be confounded or would report a false
FAIL, and the test runner cannot fail. Fix B1-B3, then this is ready to integrate.

## Host tests re-run (reviewer)

`tests/run.sh` on the Mac host:
- verify_pins.py: 12/12 assertions PASS across 8 pins
- gate_no_hardcode.py: 3 files / 327 lines scanned, PASS
- test_pak_mount_host.cpp: 23/23 PASS (clang++ -std=c++20 -Wall -Wextra -Werror)
- Repo gates on PATCH/ (read-only run): `no_detour_gate.py --root PATCH` scanned 3 C++
  sources, PASS. `fs_throw_gate.py --root PATCH` checked 3 files / 205 lines, PASS.
- Housekeeping: `run.sh` writes to `tests/run-output.txt`, so my run overwrote the team's
  copy. I restored the team's original (header `2026-10-13T03:15:31Z`) from a copy I
  took first.

## BLOCK

**B1. The Leon mod trio is already in the game's paks folder. The team missed it, so
the launch cannot prove anything.**
The Oct 8 scan (`outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/
harvest-20261008T174239Z-25929d/storage/directory-listings.jsonl`, the
`.../townfall/content/paks` record) lists `zzz_Leon_Townfall_2K_P.pak` (347 B),
`.utoc` (50,364 B) and `.ucas` (185,446,608 B), dated Oct 01 22:56, inside the game's
own Paks folder. Neither `evidence/console-state-oct8.md`, NOTES, the rollback list nor
the launch card mentions them. The same scan shows that `/data/modbridge/roots/PPSA33286`
holds only `ue4ss/`, `loadlist.txt` and `runtime-status.txt`: there is no mod trio in the
mod root.
- Failure scenario: the engine's startup scan mounts every `*.pak` in Content/Paks
  (IPlatformFilePak.cpp:6317-6372), so Leon shows up whether or not our runtime call
  works. "Mod visible" proves nothing. Runtime-mounting a second copy of the same
  container may also be rejected by the IoStore backend. The call then returns null and
  logs a false "refuse", while Leon is still visible from the boot-time mount.
- Fix: the launch card needs a "game is pristine" precondition, with each step a separate
  console action the user OKs:
  1. Move the zzz trio out of Content/Paks.
  2. Restore `pakchunk0-ps5.pak` and `pakchunk0-ps5.utoc` from their `.bak` copies.
  3. Remove `pakchunk0-ps5_s1.ucas`.
  4. Read back sha256 of each file against the .bak shas.
  5. Upload the unchanged trio to the mod root (also a console action needing OK).

  Add the zzz trio to `needs_from_integrator`. Also note in NOTES that Leon reaches the
  game by two routes (the boot-time mount of the trio and the utoc overlay), not only
  the overlay.

**B2. Following APPLY.md's "mounted pak count" pin gives a false FAIL on success.**
APPLY.md step 3 says to read `[pak_layer + 0x28]` as the MountedPaks count. Epic
`PakFile/Public/IPlatformFilePak.h:2010-2014` lays out the fields after the vptr as:
`LowerLevel` +0x08, `TArray<FPakListEntry> PakFiles` +0x10 (Num at **+0x18**), then
`TArray<FPakListDeferredEntry> PendingEncryptedPakFiles` +0x20 (Num at **+0x28**).
IPlatformFile has no data members (GenericPlatformFile.h:337-948).

The earlier RE note (`l25-prep/PAK-RUNTIME-MOUNT-DESIGN.md:52`) describes a 0x38-byte
record with priority at +0x20 and an FGuid at +0x24. That is exactly
`FPakListDeferredEntry` (h:2000-2007: FString, FString, ReadOrder, FGuid, int32). The
real `FPakListEntry` is 16 bytes (h:1988-1989). So +0x28 is the pending-encrypted count.
- Failure scenario: an unencrypted mod mounts. PakFiles grows, but +0x28 stays 0. The
  unit returns `RefusedCallFailed` and logs `[PAKMOUNT] refuse: engine mount did not take
  (fail-closed, vanilla)`, while the mod is actually live. The one launch is wasted.
- Fix: pin the count at +0x18. Two sources: the Epic header, plus the `PakFiles.Add`
  store in the image's Mount body at 0x176ec30 (IPlatformFilePak.cpp:6102-6106). Add it to
  pins.json, verify_pins.py and the generated .inc. Do not leave it as prose in APPLY.md.
  Also log the raw `ret` value. A non-null IPakFile* from HandleMountPakDelegate
  (cpp:6406-6418) is itself a success signal.

**B3. `tests/run.sh` cannot fail, which breaks the gate rule.**
The runner is `set -e; { ... } 2>&1 | tee "$OUT"` with no `pipefail`. A pipeline's exit
status is `tee`'s, so it is always 0. Reproduced on a scratch copy outside the deliverable:
- When the pin verifier crashed because it could not find the image, `run.sh` still
  exited 0.
- With a forbidden name injected into the .cpp, the gate printed `GATE FAIL: 1
  violation(s)`, but `run.sh` still exited 0.

The integrator would see a green exit code.
- Fix: run under `bash` with `set -eo pipefail`, or write to the file and check each
  stage's status. Also give `verify_pins.py` an explicit missing-image error. Today it
  depends on a relative path four levels up and dies with a traceback.

## FIX-SOON

**F1. The signature gate checks the wrong function, and the actual call target is never
checked.** `mount()` checks only P4 `GetPlatformFile`, which the unit never calls (the
bridge does). The function it does call, `exec_fn`, is read from a heap instance's vtable
and called without any check. Pins P7 `handle_mount_pak` and P6 `slot_get_lower_level`
are carried but unused, and `valid()` does not require P7.
- Scenario: on the planned re-pin to another title, a build with inline delegate storage
  (`NUM_DELEGATE_INLINE_BYTES>0`) would make `[deleg+0]` the inline instance, not a
  pointer. `inst_vt+0x60` would then be junk, and the unit would call into it.
- Fix:
  - Before the call, require all of these: `inst_vt == base + <pinned iface vtable>`
    (0x7fef570 is already in pins.json), `[inst+0x18] == pak_layer`,
    `[inst+0x20] == base + P7`, and `exec_fn` inside the image's text segment.
  - Make P7 required.

**F2. The chain walk reads `[layer+8]` on every layer, whatever its type.** The `+8` is a
hand-written offset in the .cpp. That contradicts the header, which says no offsets are
hand-written there. Only FPakPlatformFile's GetLowerLevel was verified to be
`mov rax,[rdi+8]`.
- Scenario: a wrapper layer whose +8 is not LowerLevel sends the walk into unrelated
  memory. Reads are bounded, so the likely result is a spurious `pak layer not found`.
- Fix: descend only if that layer's vtable slot +0x68 equals the pinned GetLowerLevel
  body (0xb3500 rebased).
- The mock chain is also upside down: physical on top, pak below. In the real engine the
  pak layer sits above physical.

**F3. Passing order -1 makes visibility depend on the file name.**
`GetPakOrderFromPakFilePath` returns 0 for a `/data/modbridge/...` path
(IPlatformFilePak.cpp:6384-6402). Mount adds +100 only when the file name ends `_P.pak`
(cpp:5953-5976). Base paks under Content/Paks get 3.
- Scenario: a mod pak not named `*_P.pak` mounts at order 0, below the base game. The
  PASS line prints, but the mod is invisible. A base game that ships `_P` patch paks
  (3+100=103) would also outrank the mod (100).
- Fix: pass an explicit order computed from data, above the highest `ReadOrder` in the
  live PakFiles list. Do not use a name constant: the team's own gate forbids those.

**F4. "Every failure is clean vanilla" is overstated.** Engine-side Mount and read paths
can stop the game outright: `LowLevelFatalError` for an unregistered compression method
(cpp:4228), and `Fatal` read errors (cpp:2786-2794). These happen inside the engine call,
after the unit's checks.
- Fix: say so in the launch card's Hard-FAIL section. Optionally add a Mac-side pre-check
  that the mod container's compression method names appear in the base game's containers
  (data from both files).

**F5. The bridge, the riskiest part, is not staged.** APPLY says to "reuse" runtime
pieces, but in the real candidate tree:
- `module_view_base` is a file-local `static` inside `namespace SeedArming`
  (UnrealInitializer.cpp:781, 822).
- `cached_code_signature` is a member of the admission-ops object (UnrealInitializer.cpp:924).
- `push_fstring` exists only as `push_fstring_to_lua` (LuaFNameBridge.cpp:148), not as an
  engine-FString builder.
- `from_fname` does not exist.

So `call_execute` (a raw indirect call) and `make_fstring` (UTF-16 TCHAR on this image,
engine allocator, Num including the terminator) are unwritten. A diff to
UnrealInitializer.cpp is needed. The producer of `req.pak_path_abs` (which file in the
mod root, which config key) is also unstaged. That shipping code needs this one review
pass extended to it before the build.
Separately, UnrealInitializer.cpp:1435 shows the image sha can be "unavailable". The unit
then refuses (fail-closed, good), but check that R11's `[Seeds][L18]` line shows
image-SHA256 filled in, or the launch is a guaranteed soft-FAIL.

**F6. Some tests and claims are weaker than stated.**
- The mock's `verify_code` ignores the address and the signature. The mock's
  `mounted_pak_count` ignores the layer, so it cannot catch B2. Nothing tests a failed
  count read.
- verify_pins.py does not re-derive everything it claims to. `init_fn` is a constant
  (`... if False else 0x1769ee0`). So are `ALLOC 0x70350` and the iface vtable 0x7fef570.
- The P8 check only asserts that some ".utoc" string exists in the image. It does not
  prove the Mount tail does pak + IoStore + store. Epic cpp:6041-6055 does support that
  claim.
- P3 and P4 get prologue-shape checks only.
- Reword NOTES' "re-derived straight from the local eboot" for these checks.

**F7. Launch card wording.**
(a) Tell the user that Leon will still look like a gray/white checkerboard even on PASS.
That is cap #8, and it counts as "mod visible". The user just reported exactly this.
(b) Add engine-side proof to the PASS checks: HandleMountPakDelegate prints
`Mounting pak file: <path>` through LowLevelOutputDebugStringf (cpp:6408), which should
reach klog. Log `ret`, `count_before` and `count_after` in the `[PAKMOUNT]` line.
(c) The pristine rollback from B1, with sha readback.

## NOTE

- **N1. Path correction.** The task text's Epic path (`l23-layout-generator/
  official-source`) does not exist. The source is at `l23-layout-generic/official-source`,
  which pins.json already cites correctly. `Misc/CoreDelegates.h` is not in the local
  source. The MountPak delegate type is inferred from BindRaw (cpp:5716) plus the
  HandleMountPakDelegate signature (cpp:6406). That inference is sound.
- **N2. User's "bright as day" report.** `PS5Engine.ini.diff` reproduces byte-for-byte
  from the two .ini files (sha 33865e44 / 94fc0da4). The config-mod explanation is
  solid (Mac-only). That the mod is still installed is console-proven only as of Oct 8;
  "never rolled back" since then is an inference. The fresh read-only LIST the team asks
  for settles it.
- **N3. Missing include.** `pak_mount_pins.inc` uses `std::snprintf` without including
  `<cstdio>`, so the integrating file must include it.
- **N4. Hard rules hold.** Generic (no game, mod or container names in logic). No detour
  and no code-page or file write. No SDK material. Mod files untouched. No console contact
  found in the deliverable.

## What unblocks

1. Fix B3. One line.
2. Pin the PakFiles count at +0x18 with its two sources (B2).
3. Add the pristine-state precondition, the zzz-trio removal and the mod-root upload to
   the launch card and `needs_from_integrator` (B1).

Strongly recommended in the same pass: F1 (check the call target), F3 (explicit order)
and F5 (stage the bridge diff). Rollback stays R11 bfd95db6.
