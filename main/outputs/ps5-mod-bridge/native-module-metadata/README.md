# Current-process module metadata adapter

This isolated C library validates reported module metadata. It does not read the
reported addresses, identify a game or image, own mappings, grant read permission,
produce `NativeLoaderFacts`, or feed a scanner. No runtime entry point or console
deployment is included.

## API and ownership

`mb_module_metadata_capture(provider, snapshot)` accepts trusted synchronous
callbacks. Its provider list capacity/count are explicitly **element counts**.
At most 256 distinct handles are accepted in their reported order. A successful
capture contains caller-owned copies of names, fingerprints and up to four segment
descriptors for every listed handle. An error clears the entire output, including
any records accepted before the failure, and returns a status, original provider
code, and applicable handle/module/segment index. There are no allocations or
retained provider pointers. The caller must exclusively own output and keep it
disjoint from the provider and its state.

`mb_module_metadata_read(provider, handle, record)` uses the same validation for
one supplied handle. Handle zero is allowed. The provider receives a zeroed info
buffer with size 352. Successful responses must retain that size, contain a
nonempty NUL-terminated name within 256 bytes, and report 1–4 segments. Active
segments must have nonzero address/size and a representable unsigned 64-bit
exclusive end. These are this adapter's acceptance rules, not claims that other
descriptors cannot exist. Rejected data is reported as an error, never truncated
or skipped. Unused output bytes are zeroed. Names remain uninterpreted bytes;
protection values and fingerprints remain opaque reported metadata. In particular,
zeroed fingerprints are not promoted to identity and protection bits are not
interpreted as permission. Overlapping descriptors are preserved as metadata.

Callbacks must obey buffer bounds and return zero on success. They are trusted
native code, not a sandbox against an out-of-bounds provider write. List/info are
sequential calls: success does not prove a consistent loader generation or that a
module remains loaded after the call. There are no implicit retries.

## Native boundary and remaining gap

`native_provider.c` is a separate TU. `mb_module_metadata_native_read` wraps the
exact public `sceKernelGetModuleInfo` symbol/prototype and validates its returned
data. This is compiled locally; it has not been run on target hardware.

`mb_module_metadata_native_capture` returns `LIST_ABI_UNVERIFIED`, clears any
supplied output, and makes **no native API calls**. The public declaration of
`sceKernelGetModuleList` labels its middle parameter only `size`; its implementation
forwards that value. The checked historical example does not establish the unit.
Native enumeration therefore stays unavailable until a separately evidenced
byte-versus-element contract is established. There is no guessed default or caller
override. A null output instead returns `INVALID_ARGUMENT`.

The original MIT header is retained verbatim at `upstream/libkernel.h`; it is
provenance, not a build include. `prosperodev_abi.h` uses private type spellings,
preserving member types/order with LP64 size/offset assertions. Existing SDK headers
are not modified. See `provenance.json` and `LICENSE.prosperodev`.

- Source commit: `3cdd09cc86ba90ef371bd0a834e07cc3883c49e7`, October 30, 2022.
- [Complete public declaration and prototypes](https://github.com/prosperodev/prosperodev-libprospero/blob/3cdd09cc86ba90ef371bd0a834e07cc3883c49e7/libkernel_sys/include/libkernel.h).
- [Original example](https://gist.github.com/psxdev/1890b084465db150e27b60465117770c), October 22, 2022.

The public header postdates the example; its exact earlier SDK revision is unknown.
Neither source nor local compilation establishes firmware 13.60 or game-process
compatibility.

## Local verification

From the repository root:

```sh
python3 outputs/ps5-mod-bridge/native-module-metadata/verify.py
```

This runs 28 tests using owned injected data and link-local stand-ins for native
symbols, then compiles only the two production C TUs with the retained v0.43 SDK
wrapper and LLVM 21.1.8. It verifies AMD64 ELF relocatable outputs, the four intended
exports, the real info API import, and absence of a list API import, then archives
the objects as `libmb_module_metadata.a`. It does not link or run a target program.
No SDK, toolchain, or network download occurs in the verifier.

The archiver defaults to the existing LLVM 21.1.8 tool used by the fixed-dependency
rebuild recipe; the compiler's separate retained LLVM directory lacks `llvm-ar`.
Use `--host-only` for the portable tests alone, or supply `--sdk`, `--llvm-config`,
`--llvm-ar`, `--host-cc` and `--output` to reuse installed tools at other paths. Receipts record
source/tool/dependency hashes and exact commands in a fresh owned output directory.

Tests cover copies surviving fixture mutation; empty/exact-limit/oversized/missing
counts; duplicate handles; positive and negative provider failures; size mismatch;
empty/unterminated/max-length names; segment bounds; null/empty/overflowing
descriptors; representable address boundary; output wiping after partial success;
single-handle forwarding; and the native enumeration gate. These checks validate
adapter behavior, not hardware acceptance.
