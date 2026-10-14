# cap13 — PE analysis: design and test results (STEP 2)

Capability #13, step 2: "Add format-aware discovery for native Windows DLLs
and managed C# assemblies. Identify architecture, runtime/framework and
dependencies from the file contents. Keep the existing native PS5-module route
intact."

This tool is discovery/classification only. It reads bytes; it never executes
the analyzed image, never writes engine files, and does not touch the #9
native PS5-module route (`mbm_mod_entry_v1` admission is unchanged).

## Deliverables

| Path (relative to `cap13/`) | Purpose |
|---|---|
| `TOOL/pe_analyzer.py` | The format-aware discovery tool (stdlib-only Python 3.9+, ~950 lines) |
| `TOOL/make_pe_fixtures.py` | Deterministic hand-crafted PE fixture builder (no compiler) |
| `TOOL/run_tests.py` | Regression suite: 31 assertions over fixtures + 5 real images |
| `FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll` | x64 PE32+ DLL, UE4SS-native-mod-shaped (labeled fixture) |
| `FIXTURES/cap13-native-x86-FIXTURE.dll` | x86 PE32 DLL (unsupported-arch test) |
| `FIXTURES/NOT-A-PE-FIXTURE.dll` | Text with `.dll` name (unknown-classification test) |
| `REPORT/evidence/fixture-build-manifest.json` | Fixture layout (RVAs of entry/IAT/tables) + hashes |
| `REPORT/evidence/analyzer-test-results.json` | Full suite output incl. complete per-file reports |
| `REPORT/evidence/UE4SS.dll.analysis.json`, `REPORT/evidence/mscorlib.dll.analysis.json` | Full JSON analyses of the two largest real images |

## Tool design

### Parse pipeline

1. **DOS header**: `MZ`, `e_lfanew` at 0x3C; DOS stub ignored.
2. **COFF header**: machine, section count, timestamp, characteristics
   (`IMAGE_FILE_DLL` = 0x2000), size of optional header.
3. **Optional header**: magic 0x20B (PE32+) / 0x10B (PE32) with the two divergent
   field layouts; entry point RVA, image base, alignments, `SizeOfImage`,
   subsystem, DllCharacteristics (decoded to names: ASLR `DYNAMIC_BASE`, DEP
   `NX_COMPAT`, `HIGH_ENTROPY_VA`, `NO_SEH`, `GUARD_CF`, ...).
4. **Data directories**: `NumberOfRvaAndSizes` + 16 entries, named
   (EXPORT, IMPORT, ..., DELAY_IMPORT, CLR_RUNTIME_HEADER).
5. **Section table**: name/VA/VSize/raw-ptr/raw-size/RWX per section; used for
   RVA→file-offset mapping (headers region included; virtual-tail RVAs map to
   "unmapped" rather than garbage).
6. **Imports** (dir 1): walks IMAGE_IMPORT_DESCRIPTOR array until the null
   terminator; per-DLL function names from the Import Name Table (falls back to
   IAT when OFT is 0); ordinals (`Ordinal#N`), hint/name RVAs, PE32 and PE32+
   thunk widths; **delay-load imports** (dir 13, `IMAGE_DELAYLOAD_DESCRIPTOR`,
   RVA mode when `grAttrs & 1`, VA mode otherwise).
7. **Exports** (dir 0): self-name, ordinal base, EAT size, name-pointer count;
   per-name ordinals and RVAs; **forwarder detection** (symbol RVA inside the
   export directory → forwarder string).
8. **Managed detection** (dir 14): CLR header (`IMAGE_COR20_HEADER`):
   runtime version, flags (`ILONLY` → mixed-mode), EntryPointToken; then the
   ECMA-335 metadata walk: `BSJB` root, version string, stream headers,
   `#~` table stream (heap sizes, valid-table bitmask, row counts), full
   row-schema walk over all standard + Portable-PDB tables (40 schemas) to
   reach `Module` (name), `Assembly` (identity/version), and `AssemblyRef`
   (referenced assemblies with versions). A `#-` (uncompressed) stream is
   reported but not walked. An unknown table stops the walk with a precise
   diagnostic instead of guessing widths.
9. **Classification** (`classification` + `classification_reason`):
   | Case | Result |
   |---|---|
   | not `MZ` / bad PE sig / truncated | `unknown` + diagnostic |
   | AMD64, no CLR | `native-x64` |
   | AMD64 + CLR | `managed-dotnet` |
   | I386 + CLR IL-only | `managed-dotnet` (AnyCPU IL runs under a 64-bit runtime) |
   | I386 native or mixed-mode | `unsupported-arch` (32-bit cannot load into the 64-bit PS5 host) |
   | ARM64 / other machines | `unsupported-arch` (PS5 host is AMD64) |
10. **UE4SS report**: any import of `UE4SS.dll` (case-insensitive) with the
    imported function list; exports named `start_mod`/`stop_mod`; and a weak
    `looks_like_proxy_dll` hint (imports a commonly-proxy'd system DLL — note
    it also fires on UE4SS.dll itself because it imports `dbghelp.dll`; it is
    a hint, never a verdict).

### Output

- Human text (default) and `--json`; `--out FILE` persists the full JSON;
  `--summary-json` strips bulky header/section detail for triage at scale.
- Every report carries `sha256`, size, `diagnostics[]` (missing feature, e.g.
  "no CLR runtime header (native image)") and `warnings[]` (truncations,
  unmapped tables, string-index OOB) — per the acceptance rule that
  unavailable features produce a precise diagnostic, never a fake no-op.

### Format facts that mattered (all hit during validation)

- PE32+ `NumberOfRvaAndSizes` sits at optional-header offset **108**, data
  directories at 112 (PE32: 92/96). An off-by-8 here silently parses garbage.
- ECMA-335 table ids: **MethodSpec is 0x2B, GenericParamConstraint is 0x2C** —
  easy to conflate; confirmed against Mono's own `blob.h` enum in
  `cap10-mono-build/sources/mono-.../mono/metadata/blob.h`.
- Coded-index size rule (ECMA-335 §II.24.2.6): 2 bytes only while the largest
  referenced table has ≤ `0xFFFF >> tag_bits` rows — **not** 65535. For
  mscorlib this changes HasCustomAttribute (5 tag bits, threshold 2047),
  HasConstant/HasDeclSecurity (threshold 16383), MemberRefParent and
  CustomAttributeType (threshold 8191), HasFieldMarshal (threshold 32767).
  Getting this wrong displaced the whole table walk by exactly 0xBA50 bytes in
  Mono's mscorlib — the delta that exposed the bug.
- Heap sizes bitmask 0x05 → 4-byte String and Blob indexes (Mono's mscorlib).

## Fixtures (hand-crafted PE, no compiler)

`make_pe_fixtures.py` builds byte-deterministic images with fixed placement:
headers 0x400; `.text` RVA 0x1000 (DllMain @0x1000, `start_mod` @0x1020,
`stop_mod` @0x1030); `.rdata` RVA 0x2000 (import descriptors @0x2000, export
dir @0x2040, strings/hint-names); `.idata` RVA 0x3000 (INTs/IATs). x64 code
uses `call qword ptr [rip+rel32]` into the IAT slots with displacement
computed from actual RVAs at build time. DllCharacteristics deliberately set
to `NX_COMPAT` only (no `.reloc` section, so no ASLR claim). `TimeDateStamp`
is 0; rebuilds are bit-identical (asserted in the suite).

Cross-validation: **LLVM objdump** (`xcrun objdump --private-headers`) on the
x64 fixture independently confirms machine coff-x86-64, PE32+, subsystem GUI,
`NX_COMPAT`, IAT dir 0x3020+0x28, imports `UE4SS.dll!is_ue4ss_initialized` +
`KERNEL32.dll!DisableThreadLibraryCalls`, exports `start_mod` (ord 1, RVA
0x1020) and `stop_mod` (ord 2, RVA 0x1030) — identical to the analyzer's
output. (`file(1)` calls the fixtures "MS-DOS executable" because its libmagic
rules match the non-standard DOS stub before the PE parser; llvm-objdump and
the analyzer both parse the full image, and the DOS stub is within bounds.)

Fixture caveat (honest scope): DllMain calling imports is loader-lock-hostile
in production; real UE4SS mods defer work to `start_mod`. The fixture exists
to exercise discovery, not loader etiquette — the report carries this note so
nobody copies the pattern into the real loader route.

## Test results (31/31 pass)

`python3 TOOL/run_tests.py` → `REPORT/evidence/analyzer-test-results.json`

| Group | Checks | Result |
|---|---|---|
| x64 fixture: classification, machine/format, subsystem/DLL flag, managed=no, UE4SS+K32 imports (exact function sets), export ordinals/RVAs, ue4ss report, section names | 9 | PASS |
| x86 fixture: `unsupported-arch` with reason, PE32 fields, imports still reported precisely | 3 | PASS |
| Non-PE fixture: `unknown` + precise diagnostic | 2 | PASS |
| Real `UE4SS.dll` (20 MB): `native-x64`, 4256/4256 exports parsed, 0 forwarders, 6 C-API names present, 29 imports incl. MSVCP140/VCRUNTIME140/api-ms-win-crt, no self-import | 5 | PASS |
| Real `dwmapi.dll` proxy: `native-x64`, `LoadLibraryW`/`GetProcAddress` pattern, full re-export surface (198 EAT entries, `DwmDefWindowProc`) | 3 | PASS |
| Real `Probe.dll` (cap10 C#): `managed-dotnet`, module/assembly/mscorlib-ref/`v4.0.30319` extracted, IL-only, `mscoree!_CorDllMain` | 4 | PASS |
| Real `mscorlib.dll` (Mono 4.5): `managed-dotnet`, full 30-table walk with zero warnings, identity `mscorlib 4.0.0.0` | 3 | PASS |
| Real `worker.dll` (spike fixture): `native-x64`, 2 exports | 1 | PASS |
| Determinism: rebuild reproduces byte-identical fixture | 1 | PASS |

Bugs caught by this validation loop (all fixed, kept here as evidence the
tests do real work): the PE32+ data-directory off-by-8; the
MethodSpec/GenericParamConstraint table-id swap; the coded-index tag-bit
threshold rule; Assembly-row version offsets; a fixture DOS-stub overflow that
shifted the PE signature by one byte (caught immediately by the analyzer's
signature check); three struct pack-format errors in the builder; and one
wrong test assumption (the dwmapi proxy does export a full function surface —
the analyzer was right, the assertion was not).

## Limitations (explicit)

- Discovery only: no loading, no relocation processing, no Bound-import
  parsing, no Authenticode/Security-directory verification, no checksum
  validation. `.NET` ReadyToRun/NativeAOT images classify as managed with
  their native code unexamined.
- Metadata walk stops precisely (with diagnostic) at any table outside the
  implemented schemas rather than guessing.
- x86/ARM64 classification reflects the PS5 route's 64-bit host reality;
  ARM64X, RISC-V and IA64 report their machine names for triage.

## How to re-run

```
cd .../cap13
python3 TOOL/make_pe_fixtures.py --manifest REPORT/evidence/fixture-build-manifest.json
python3 TOOL/run_tests.py
python3 TOOL/pe_analyzer.py FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll
python3 TOOL/pe_analyzer.py <any.dll|.exe|.sys> --json --out report.json
```
