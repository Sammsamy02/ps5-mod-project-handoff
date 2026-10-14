# Team S3 — the profile-file generator (plain words)

**What this is.** The PS5 mod runtime used to be built one-per-game: about 150 numbers for Silent
Hill: Townfall were baked into the program, so a new game meant a new build. Team S1 designed a single
**data file** (`PROFILE-FORMAT.md`) that the runtime can load instead. **Team S3's job (readiness item
B4): teach the automatic profile generator to write that file** — every value the runtime needs,
measured from the game's own executable, with nothing typed in by hand.

Everything here is **Mac-only**. Nothing was built into a PRX, nothing was sent to the PlayStation,
no console log was read.

## What was done

1. Added an **emitter** to the existing generator (`tools/profilegen/emit_profile.py`) that writes the
   profile file in Team S1's format.
2. Fixed the **memory-allocator (GMalloc) value**. The old generator wrote the wrong pointer for this
   runtime (the "dispatch" variable). The runtime's safety checks actually want the **raw allocator
   slot**, so the emitter now writes that one — the single most important fix (READINESS B5).
3. Taught it to find the **allocator's vtable** from a second, independent place in the code (the
   object's constructor), which the old generator never produced.
4. Made it read the **exact prologue bytes** each code pin needs straight from the executable.
5. For the things the generator genuinely **cannot** work out yet (the pak-mounting functions, the
   delete-listener functions, the thread-name lookup, three vtable slots), it does **not guess**. It
   writes them as "absent" and switches that one feature off, so the game still runs the rest. Missing
   values fail safe; they never make the runtime use a wrong number.

## Did it work? (the two-source check)

Yes. Run on the real Townfall executable, the generated file was compared, value by value, against
Team S1's file (which is read mechanically from today's compiled runtime). The result:

- **39 of 39** identity/build/pool/seed/pin scalar values **match exactly** — including the corrected
  allocator value `0x930add8` and the newly-found allocator vtable `0x7ec26e0`.
- **73 layout offsets match**, 0 disagreements.
- **0 unexpected mismatches.**
- The values the generator cannot derive yet are listed openly in `GAPS.md`.

Full report: `evidence/two-source-check.txt`. The generated file: `evidence/townfall-profile.ini`.

## How to run it

```sh
cd generator/outputs/ps5-mod-bridge
python3 -m tools.profilegen --eboot <decrypted eboot.bin> --param <param.json> \
    --out <folder> --emit-profile <folder>/profile.ini
```

Add `--exe-path <console path>` once the console copy's location is decided (that one field is Team
S2 / item B8, paused for your decision). `--profile-gmalloc dispatch` switches the allocator value
back if ever needed.

## Files here

- `generator/` — the generator with the new emitter (a full copy; diffs in `evidence/*.diff`).
- `host-tests/emit_profile_tests.py` — 11 tests (10 synthetic + the real Townfall two-source check).
- `host-tests/check_equivalence.py` — the field-by-field comparison tool.
- `GAPS.md` — every value not produced, and why. `APPLY.md` — how the integrator picks this up.
- `evidence/` — the generated profile, the comparison report, the test logs, the diffs.
