# FZ — Second-game profile dry-run (research, 2026-10-08)

Agent FZ next-track research item B. Static analysis only: Mac-local files, no
builds, no console, no edits outside `2026-10-08-l25-prep/`. All artifacts in
`FZ-GAME2/`.

## 1. Executable hunt results

Searched `outputs/` for game-sized executables (+10 MB, eboot/elf/self/bin) and
checked magics:

| Candidate | Bytes | Magic | Verdict |
|---|---:|---|---|
| `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin` | 194,241,424 | `7f454c46` plain ELF | **Townfall** (game 1; existing profile) |
| `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin` (+ toplevel, earlyload A1/A2) | 194,241,424 | `7f454c46` | Townfall copies (param.json titleName "SILENT HILL: Townfall") |
| `outputs/2026-10-06-recovery-fakesign/astro-eboot-faked.self` | 259,312,583 | `4f153d1d` fake-SELF | **Astro Bot — contains the decrypted inner ELF verbatim** (see §2) |
| `outputs/2026-10-06-recovery-fakesign/astro-eboot-PRISTINE-orig.self` | 259,312,827 | `5414f5ee` Sony-signed | encrypted original; not statically profileable |
| `outputs/2026-10-06-recovery-fakesign/tf-eboot.self` + 7 libs | — | `4f153d1d` | Townfall recovery baseline |

A decrypted **second-game** executable DOES exist on the Mac: Astro Bot's, as the
uncompressed verbatim payload inside the fake-signed SELF produced during the
2026-10-06 recovery. Stellar Blade (PPSA13197, ShadowMount `image_ro=PPSA13197.exfat`)
is confirmed Unreal-built from its `PAK v11`/`IoStore v3`/AMPR content
(`outputs/2026-09-30-stellar-blade-*.md`) but its eboot is NOT on the Mac.

## 2. Dry-run executed: Astro Bot static profile attempt

Tooling used (all existing, unmodified):

- Container: make_fself.py layout from
  `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/install_app/make_fself.py`
  (fake SELF = 8 B common + 24 B ext header + N×32 B entries, embedded original
  ELF ehdr+phdrs, then verbatim uncompressed segment copies).
- Axes classifier: `outputs/2026-10-04-zcode-team/worker-axes-detector/tools/ref_axes.py`
  over the patternsleuth marker table
  `.superpowers/.../ue4ss-native-patternsleuth-1/upstream/patternsleuth/src/resolvers/unreal/engine_version.rs`
  (30 versions / 244 markers — the same table that produced Townfall's
  54-marker/shipping-60/stats-12 classification).
- Anchor probes: GUObjectArray (9 patterns), GMalloc (5 patterns), FName ctor
  UTF-16 module strings — identical pattern syntax and sources as
  `outputs/2026-10-04-zcode-team/worker-required-slots-feasibility/offline_anchor_scan.py`
  (patternsleuth resolvers).

Script: `FZ-GAME2/astro_fakeself_dryrun.py`. Raw facts: `FZ-GAME2/astro-static-facts.json`.
Reconstructed image: `FZ-GAME2/astro-inner-decrypted.elf`.

### Results

Container parse (sha256 of `astro-eboot-faked.self` =
`6aed2e777e9a0e40b1db67d5ce1cf02d03e6ec848c36a77ded9b9660366b2b88`):
header_size 0x560, meta_size 0x510, file_size 0xf748e80, 12 entries, embedded
ELF header at file offset **0x1a0**.

Inner ELF: type `ET_SCE_EXEC_ASLR (0xfe10)`, x86-64, entry 0x70, phnum 14,
shnum 0 (zeroed by make_fself). Five PT_LOADs: RX `0x74bcc2c` @vaddr 0, R
`0x141da95`, RW `0x579490`, RW `0x5b4ba80`, and a no-perm `0xd2d700` load.
Reconstructed size 258,845,524 B, sha256
`7723d150328e8dd6dcf733731466390ebdab458918f5c75e6534ce7df442844b`.
Reconstruction gaps (bytes the fake SELF never stores, zero in the
reconstruction): PT_SCE_VERSION 15,687 B + a 96 B `0x61000001` segment + two
trailing PT_NOTEs — metadata only, no code/data impact; per-segment sha256s are
in the facts JSON. NOTE: this reconstruction is a *derived* view for analysis;
the console-authentic executable is the signed `.self` file itself.

Classifiers:

| Probe | Townfall (reference behavior) | Astro Bot result |
|---|---|---|
| engine_version markers (244) | 54 found → build-config/stats axes | **0 found** |
| GUObjectArray anchors (9 patterns) | multi-hit pin candidates | **0 hits** |
| GMalloc anchors (5 patterns) | multi-hit pin candidates | **0 hits** |
| FName ctor UTF-16 module strings (Engine/Renderer/…) | present | **absent** |
| ascii `GUObjectArray`/`LogUObject`/`/Engine/`/`UnrealEngine` | present | **0** (4 incidental `UObject` ascii bytes only) |

### Conclusion

**Astro Bot is not an Unreal Engine game — per-game profile generation correctly
REFUSES.** No pin candidates exist to emit; confidence per pin: not applicable.
The refusal is the expected, correct dry-run outcome and it validates the
machinery end-to-end on a second, never-profiled executable (container unwrap →
identity → axes → anchors), with zero console contact. Astro Bot remains in the
Phase B regression-launch list only as a launch-health check.

## 3. What a REAL second UE game needs (pipeline, from the l2x→l26 lineage)

For Stellar Blade (the available UE second game), the existing pipeline stages are:

1. **Image identity** — decrypt-on FTP download of `eboot.bin` (see §4); sha256 +
   size + extent bound into `image:{sha256,size,extent,base}` (schema:
   `worker-2-layout-facts/schema/unreal-build-profile.schema.json`, instance:
   `outputs/2026-10-08-l23-runtime/profile/profiles/townfall-build-facts.json`).
2. **Axes** — `ref_axes.py` (or the C probe unit) over the marker table →
   build_configuration + stats gates (Townfall: shipping, stats off).
3. **Anchor pin candidates** — patternsleuth resolvers (GUObjectArray, GMalloc,
   FName ctor slot, vtables) scanned via `offline_anchor_scan.py`-style code;
   every address classified by PT_LOAD span; multi-pattern agreement = candidate.
4. **Member/vtable offsets** — static-layout window scans (STATIC-FACTS /
   REFLECTION-FACTS / FIELDC / UCLASS / CLASS-SIZE evidence families in the l23
   profile) — two-source agreement required; unknown/mismatch/conflict = refusal.
5. **Unit emission** — `outputs/2026-10-07-l19-local-profile-r4/profiles/generate_profile.py`
   generates `RuntimeBuildProfile.hpp`, `CompiledNativeLayoutDeclarations.hpp`,
   `JoinedLayoutPreflight.hpp` from the build-facts JSON (Townfall today: 69+5
   required members, 24 required vtables, image sha `3b6cfad4…`).

Caveat carried from Townfall: static labels are image-relative; the live base
(Townfall 0x400000, proven in-game) must be observed at runtime — Astro's
`ET_SCE_EXEC_ASLR` type confirms PS5 eboots are ASLR'd. A game-2 profile must
bind image sha + extent and resolve base live before any pin is used.

## 4. Acquisition procedure for Stellar Blade's decrypted eboot (NOT performed; console off)

Per the decrypt-off readback law (POSTMORTEM rules 1-5) — note the asymmetry:
for PROFILING we want the decrypt-on view; the law exists to prevent
re-uploading it:

1. Connect ftpsrv :2121. Default connection already returns the DECRYPTED view
   (and fakes LIST/SIZE to decrypted size) — download
   `/data/Stellar Blade 01.014.001 PPSA13197/eboot.bin` on a default connection;
   expect magic `7f454c46`, size ≈ file-sized ELF.
2. Provenance cross-check on a SECOND connection with `-Q "SITE SELF"`:
   decrypt-off readback head must show the real on-disk magic (`4f153d1d`/`5414f5ee`)
   and the true signed size; record both sizes + sha256s.
3. Record sha256 of the decrypted image; stage it under a NEW
   `.superpowers/native-observations/stellarblade-static-executable-<date>/`
   dir exactly like the Townfall one; never write into live profile trees.
4. Run stages §3.1-§3.3 dry (static) and file the results under
   `2026-10-08-l25-prep/` or its successor before ANY runtime/console step.
5. NEVER upload any downloaded/modified executable back (POSTMORTEM rule 2);
   modified executables need the make_fself fake-sign pipeline + decrypt-off
   readback verification before any launch (rules 3-4).

## 5. Answer to the task question

- A decrypted second-game executable existed on the Mac (Astro Bot, inside
  `astro-eboot-faked.self`) → dry-run WAS executed; result: profile generation
  refused — Astro is not Unreal (0/244 markers, 0 anchor hits). No pins emitted.
- The viable UE second game (Stellar Blade) has no local executable; its
  acquisition procedure is documented in §4 and intentionally not performed.
