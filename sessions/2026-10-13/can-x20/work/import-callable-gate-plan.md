# Built-in import callable gate

**Problem:** The current built-in row registry returns raw SysV function bodies that also require a hidden `WinApiShim::Impl*`. A PE import can be reported resolved without a guest-callable Win64 wrapper.

**Stage:** `cg-import-callable-gate-20261013-13`, copied from the isolated import registry stage. No shared candidate changes.

**Change:** Keep raw built-in bodies private as metadata. `resolve()` returns built-in pointers only after a caller explicitly publishes a non-null Win64 wrapper for a known row. Unknown rows, unpublished rows, and null wrappers refuse. Preserve registered-module lookup and lock-release behavior.

**Proof:** Test-first host checks must show raw rows refuse, only known published rows resolve, case normalization works, and recursive module lookup remains safe. Rebuild and run the existing synthetic PE fixture. This does not build the wrappers or claim Windows API semantics; it converts an unsafe apparent success to an explicit gate.
