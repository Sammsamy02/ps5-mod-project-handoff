# APPLY — how the integrator picks up Team S3 (B4)

Mac-only. No console, no PRX build here. This changes only the Mac-side profile generator.

## What changed (branch base: `origin/claude/townfall-item-spawner-hyj9eg` @ 29608418)

Four files under `outputs/ps5-mod-bridge/` (full diffs in `evidence/generator-tools.diff` and
`evidence/generator-tests.diff`):

| File | Change | Size |
|---|---|---|
| `tools/profilegen/emit_profile.py` | **NEW.** Emits the Team S1 profile file; GMalloc base-slot fix; GMalloc vtable from the instance ctor; exact-length code-pin prefixes; absent-feature handling. | +~380 lines |
| `tools/profilegen/cli.py` | Wire `--emit-profile PATH`, `--profile-gmalloc`, `--exe-path`, `--profile-name`; pass the opened ELF; record a `profile` evidence file. | +~40 lines |
| `tools/profilegen/emit_ue4ss.py` | Add `("FProperty", "GetMinAlignment")` to `EXTENDED_VTABLES` (it was resolved but not emitted). | +1 line |
| `tests/profilegen_tests.py` | Update one golden (`TownfallValidation`) to include the now-emitted `GetMinAlignment=0x170`. | +1 line |

No detector, no `ORDER`, no runtime tree, no mod file, no frozen build was touched. `HardRuleTests`
(no per-game value in the generator source) still passes; the non-extended `VTableLayout.ini` sha
golden is unchanged.

## How to apply

Option A (recommended) — copy the two new/changed tool files and the two edits onto the branch:
```
emit_profile.py          -> new file
cli.py                   -> apply evidence/generator-tools.diff hunk
emit_ue4ss.py            -> apply the EXTENDED_VTABLES hunk
tests/profilegen_tests.py-> apply evidence/generator-tests.diff
```
Option B — `git apply` both diffs from a checkout of the branch (paths are repo-relative under
`outputs/ps5-mod-bridge/`). They were produced with `diff -ru branch/ generator/`.

## How to run it for a new game

```sh
cd outputs/ps5-mod-bridge
python3 -m tools.profilegen --eboot <decrypted eboot.bin> --param <param.json> \
    --out <profile-folder> --emit-profile <profile-folder>/profile.ini
```
Deploy `profile.ini` to the console as a `profile` loadlist row named `profile/<image-sha256>.ini`
(the loader matches by the authenticated sha; S1 §1). That is a console action and needs the user's OK.

Flags:
- `--profile-gmalloc base_slot` is the default (what THIS runtime's V1/V2 seed). `dispatch` only if the
  runtime owner changes the validator.
- `--exe-path <console path>` fills `exe_path` once item B8 (readable-copy location) is decided by the
  user. Until then it is omitted and the runtime takes it from config.
- `--load-base 0x...` after a console observation raises address confidence from medium to high.

## Verify (two-source, Mac-only)

```sh
python3 -I host-tests/check_equivalence.py --ini <profile-folder>/profile.ini \
    --oracle <sb-team-S1>/gen/townfall-expected.json
python3 host-tests/emit_profile_tests.py        # 11 tests (run WITHOUT -I so capstone is importable)
cd outputs/ps5-mod-bridge && python3 -m unittest \
    tests.profilegen_tests tests.profilegen_robustness_tests tests.profilegen_seedtable_tests
```
Expected: two-source `RESULT: PASS` (0 unexpected mismatches); 11 S3 tests OK; 330 branch tests OK.

## Depends on / coordinates with

- **Team S1** owns the format and the runtime loader (`ModProfileLoader.hpp`). This emitter targets the
  **draft v1** of `PROFILE-FORMAT.md`; reconcile if S1 revises it (see the reconciliation note in the
  handback / README).
- **Team S2** owns title detection (B1), image anchoring (B2) and the `exe_path` / B8 decision. The
  profile carries `image_base`/`image_extent` as the authenticated expectation; S2 produces the live
  observed values the gates compare against them.
- Closing the GAPS.md feature detectors (pak, listener, thread) and the missing vtable slots is further
  generator work, out of B4's emitter scope.
