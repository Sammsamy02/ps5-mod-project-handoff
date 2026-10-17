# #10 ordinary shared ELF readiness

The actual reviewed owner, invocation boundary, genuine target Mono archive and static corlib link successfully as an ordinary shared ELF. This closes the previous component shared-format gap. It does not build/package the game PRX or execute anything on the PS5.

`evidence/SHARED-OWNER-LINK.json` binds all source/object/archive inputs, exact real compiler/linker commands and outputs. The source-built Mono archive is not the SDK import stub. The existing unexecuted observer/provider fixtures supply only the test boundaries; all Mono, GC/rooting and AOT registration implementations come from the genuine source-built runtime.

- Result: `ORDINARY_SHARED_OWNER_LINK_PASS`.
- Output: `shared-owner.elf`, 45,202,664 bytes, SHA256 `facae77b9c8b676f1fb08dcf23f6e31ab8db359bdde8c1f11db0318d7b041ad8`.
- Actual shared ELF, without PIE or TEXTREL. Required Mono registration/invocation/rooting symbols remain local.
- Only `main` is dynamically exported, through the test-only version script. This establishes private-symbol shared binding; it is not the game's module entry/exit contract.
- 295 undefined dynamic entries: 289 strong imports from public SDK providers and six unresolved weak SDK hooks. The weak hooks are not provided implementations. Compared with the prior standalone PIE, six loader/memory-protection imports are additionally visible; the exact delta is recorded separately. Neither set is the actual combined game import contract.
- Dynamic relocations: 5,943 relative, 295 global-data, two absolute64 and one jump-slot relocation. No unresolved 32-bit absolute or PC-relative relocation remains.

The generated ELF and its link map remain local test artifacts and must not be deployed. Claude still validates the actual combined build's import identities, constructors, relocation handling, package manifest, module lifecycle and execution. A later owner lifecycle fix must be compiled and linked separately; this receipt retains the exact earlier owner hash rather than silently rebinding it.

Reproduction: run `python3 tools/link_shared.py` in a fresh copy of this component with the same frozen inputs. The script refuses to overwrite its completed output/receipt. It performs no compilation, console contact or PRX packaging.

`evidence/SHARED-IMPORT-BINDINGS.json` separates bindings. The inherited `kernel_mprotect` weak hook is called without a null check on SDK executable-protection paths; ordinary dl* wrappers refuse when their weak hooks are absent. Neither a link pass nor the allowed-weak list proves executable-memory or ambient dynamic-loading support. Use the static interpreter/corlib path and real owned provider, and leave any broader provider work with its actual owning capability.
