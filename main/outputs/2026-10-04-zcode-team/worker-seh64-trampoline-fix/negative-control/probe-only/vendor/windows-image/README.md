# PE image preparation

`pe_image.c` is a portable C11 preparation stage for AMD64 PE32+ DLLs. It places
headers and section bytes in caller-owned **ordinary data storage**, zeroes gaps
and virtual tails, applies explicit DIR64 rebasing, and writes import values
supplied by an explicit caller binder. It exposes entry/export RVAs and remaining
execution requirements. It does not allocate memory, change page permissions,
load libraries, call DLL instructions, or access any game/process memory.

A successful result means **prepared bytes, not execution-ready code**. The same
is true when every import receives a numeric binding. Fixture addresses are data
used to verify IAT writes; they do not demonstrate usable Windows APIs or ABI
compatibility. The existing native CppMod Windows-PE rejection is unchanged.

## API and ownership

Include `pe_image.h` and compile `pe_image.c` as C11. No external dependency beyond
`memcpy` and `memset` is required.

- `mb_pe_inspect` validates supported file structures and returns image size,
  preferred base, entry RVA, section/directory metadata and counts.
- `mb_pe_prepare` takes a logical base (a number, independent of the buffer's
  actual address), caller image storage/capacity, and optional binder. Before
  copying or invoking the binder it additionally refuses overlapping relocation
  destinations, import slots, and IAT terminators. It uses the image buffer as a
  temporary byte map, then clears it before materializing the image.
- `mb_pe_visit_exports` validates first, then reports named entries in name-table
  order, followed by unnamed entries in ordinal order. Aliases are separate
  records. Zero EAT slots are skipped; `export_count` still includes these holes.
  Forwarders remain bounded text metadata; they are never resolved or called.

The complete input must remain immutable throughout each call. Inputs, full image
capacity, and output structures must be disjoint. The caller retains all buffers;
string views in records/errors borrow the original file. Callback record pointers
are transient. Callbacks must not mutate/reenter using these buffers or escape via
exceptions/longjmp. Callers supply valid, suitably aligned C output objects.

The binder receives the exact library, name/hint or ordinal, and destination IAT
RVA. `MB_PE_BIND_BOUND` supplies an explicit 64-bit value; the component cannot
validate that value as an address or implementation. `MB_PE_BIND_UNRESOLVED`
leaves the original file's IAT value and continues enumeration. A null binder
resolves nothing. `MB_PE_UNRESOLVED_IMPORTS` retains prepared image bytes and
counts; the error identifies the first missing import. A binder that records
unresolved callbacks can retain the complete unresolved set. Any other callback
result returns `MB_PE_BINDER_ERROR`, preserving the failing import identity.

Both lookups and relocation source values come from immutable input, including
`OriginalFirstThunk == 0`; previously written output is never a lookup source.
The image header's preferred ImageBase is preserved. The selected logical base
is returned separately. `relocation_count` counts DIR64 entries; ABSOLUTE padding
is skipped. `relocations_applied` is zero at the preferred base.

Invalid/overlapping argument ranges return before output writes. Other early
failures clear info/report and leave image storage unchanged. Once temporary
write marking or materialization starts, a fatal failure zeroes exactly
`image_size` bytes and leaves info/report cleared. Capacity beyond that size is
untouched. Success and unresolved status publish `image_prepared = 1`, which is
only a data-preparation state. A visitor failure can occur after earlier callbacks;
callback side effects are owned by the caller and cannot be rolled back.

## Supported subset and outstanding execution work

Format interpretation follows Microsoft's [PE/COFF specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
The repository's existing `src/inspection.cpp` was a parsing reference; this is
an independent C implementation, with no borrowed Windows headers or per-mod
hardcodes. It accepts executable-image DLL characteristics, AMD64 and PE32+ only.
It enforces section/file alignment, nonoverlapping raw and virtual section spans,
bounded directory/table/string extents and arithmetic, and zero-terminated import
and IAT tables. Security-directory addresses are file offsets; GlobalPtr's
specified zero-size form is recognized. Metadata tables/strings must be contiguous
inside one backed section or the headers; RVA gaps are not mapped regions.

Policy limits are explicit: 1 GiB file/image, 96 sections, 16 directories,
4,096 import descriptors, 262,144 nonpadding relocations/imports/export table
entries, 4,096-byte strings, and 64 MiB aggregate string traversal per pass.
Export traversal uses a fixed 32 KiB stack bitmap; other internal storage is
bounded stack data. No heap or image-sized auxiliary buffer is allocated.

Only ABSOLUTE and AMD64 DIR64 base relocations are supported. Other relocation
types, rebasing a stripped/missing relocation table, and overflowing/underflowing
rebased values fail explicitly. Duplicate or partially overlapping writes are
refused even at the preferred base. Bound imports with no original lookup table
are unsupported; existing bound values are not guessed into symbol names.

Directory presence and exact RVAs/sizes remain available for TLS, AMD64 exception
metadata, delay imports, load configuration and CLR. This component does not
execute TLS callbacks, allocate Windows TLS, install Windows unwind/SEH handling,
resolve delay/API-set/forwarded imports, implement loader-lock/DllMain ordering,
apply load-config/CFG/security-cookie requirements, run CLR, or supply Windows
calling conventions and services. Those are remaining execution backend work;
metadata presence is not validation or satisfaction of those requirements.

## Reproduce local checks

```sh
python3 outputs/ps5-mod-bridge/windows-image/verify.py
```

Each invocation creates a new receipt directory beneath
`.superpowers/native-dependency-spike/windows-image-preparation-1`. It records
commands, results, source/dependency hashes and object hashes. It runs 34 byte
semantics groups, creates a genuine compiler/linker Windows PE fixture with named
and ordinal imports and a DIR64 relocation, and verifies its prepared bytes on
the host. Generated PE instructions are never executed. The only host executables
run are the C tests and local compiler/reader tools.

The script then compiles the **same production C TU** with the retained v0.43
`prospero-clang`, `-std=c11 -Wall -Wextra -Werror -O2 -fPIC -c`, using the retained
LLVM 21.1.8 path through `LLVM_CONFIG`. It checks AMD64 ELF `ET_REL` output and
limits undefined symbols to `memcpy`/`memset`. This is a target object compile,
not a linked/running PS5 loader. `--sdk`, `--llvm-bin`, `--lld-link`, `--host-cc`
and `--output` permit explicit existing tool paths. No tools are downloaded.
`--host-only` skips the PS5 compile, while retaining the compiler-built PE check.

Optional comparison of the original retained UE4SS archive, using the separate
independent LLVM harness:

```sh
clang -std=c11 -Wall -Wextra -Werror -dynamiclib \
  outputs/ps5-mod-bridge/windows-image/pe_image.c -o /your/new/output/pe_image.dylib
python3 outputs/ps5-mod-bridge/windows-image/tests/compare_originals.py \
  --archive /your/original-ue4ss.zip --reader /your/llvm-readobj \
  --library /your/new/output/pe_image.dylib --output /your/new/comparison
```

That harness extracts the two known original DLLs unchanged, compares all image
bytes and metadata against LLVM-derived facts at preferred/higher/lower bases,
and preserves unresolved identities. It is developer validation only. Original
DLLs, generated modules/objects, local compiler outputs and captures are excluded
from source-only handoffs.
