# Independent source review instructions

Review this unapplied source-only stage; no C# compilation, execution or real final symbols have been established. Do not apply it to originals or make the current B7 build depend on it.

1. Check `source-receipt.json` and `FINDING.md` against the retained generator, generated harness, DynamicWriter and ElfModel. Confirm `ld.lld -r` remains an input preparation step and DynamicWriter performs the final layout.
2. Review `stage.py` and the two files under `staged/`. Check the optional callback defaults to null, original WriteFile arguments are identical and returned finalBytes are unmodified when the sink is absent.
3. Check every admitted row is a retained, named, defined executable FUNC with nonzero size wholly within its retained input section; final address comes from SymbolValue and has an overflow guard. Check excluded absolute, discarded, undefined, no-bits and non-executable rows cannot enter.
4. Check the harness persists the exact returned bytes, then hashes that same buffer into the sidecar. Input object/contract and executing assembly hashes identify the producer. Existing outputs refuse overwrite. Consider failure after the bare output is written: this is not an atomic two-file publication.
5. Confirm producer addresses are explicitly final ELF virtual addresses, not assumed runtime offsets. The package binder is UNBOUND and absent. Do not promote these rows into a crash-symbol sidecar until real bare-original→bare-sdk→packaged executable bytes/VMA preservation and exact packaged SHA are validated.
6. Replay the commands in FINDING.md. Six tests verify source transformation/refusal properties; they do not validate C# compilation or actual final-link behavior. Stage receipt checks two hashes, seven unique edits, two inverses and unchanged originals.

Future owner integration must intentionally rebase these retained-source hash pins onto the real normal-build generator seam after patch_packager source generation/patching and before its usual build. No build is authorized in this audit.
