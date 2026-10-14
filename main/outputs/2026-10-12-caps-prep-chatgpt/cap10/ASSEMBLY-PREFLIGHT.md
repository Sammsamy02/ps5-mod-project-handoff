# Compiled probe metadata preflight

`assembly_preflight.py` reads a future compiled Probe assembly without loading it, using only Python's standard library. No assembly was compiled here. All current tests use artificial PE/CLI bytes. In particular, the fixture's exception-test body merely returns: its admission demonstrates why metadata matching cannot prove the required exception behavior or any managed execution.

The check follows PE32/PE32+ section-backed CLR metadata, refuses mixed/native/forced-32-bit entry modes, and bounds input to 1 MiB and metadata to 4,096 rows. It refuses overlapping, unaligned, duplicate, unknown or truncated metadata streams and unsupported table layouts. Supported table IDs are 0, 1, 2, 6, 8, 10, 12, 17, 26, 27, 28, 32 and 35. A different compiler's additional tables require an explicit reader extension; do not remove refusal to admit a DLL.

It locates the genuine `ModBridge.Cap10.Probe` TypeDef and its four owned MethodDefs. It checks public/static entry methods, private/static P/Invoke methods, exact scalar/by-reference signatures, parameter ownership and `out` metadata, bounded IL-body presence, and exactly two Cdecl imports carrying the actual ImplMap NoMangle/ExactSpelling bit0x1 from `__Internal` with the proposed native entrypoint names. It reports the assembly name, dependencies and metadata version. Those reported references are not corlib or host compatibility approval.

After the console owner has a compiled assembly and its build receipt, run from this prep directory, substituting the actual path and hash:

```sh
python3 -B cap10/assembly_preflight.py FUTURE-PROBE.dll --sha256 EXPECTED_COMPILED_SHA256
```

The command reads at most 1 MiB plus one overflow byte, prints JSON to stdout and writes no file. A mismatched hash or contract exits 1 with `REFUSED`. A match is only `METADATA_CONTRACT_MATCH`; execution, IL semantics, target ABI, full assembly validity, source-to-assembly provenance, dependency loading and runtime readiness all remain unproved. The original source-text consistency checker remains a separate check. Native host, transition/trampoline support, platform/GC/corlib and explicit import binding remain implementation prerequisites.

15 tests cover artificial PE32/PE32+ with both heap-index widths, method ownership/visibility, absent bodies, import duplication/library/convention/exact-spelling bit, reply signature/out metadata, unsupported tables, stream alignment/overlap, unbacked/truncated data, and hash-bound CLI admission/refusal preserving the original input. Red logs retain missing-reader and actual out/alignment refusal regressions; bridge-stage/ASSEMBLY-EXACT-SPELLING-RED.log records accepted missing NoMangle bit before the policy fix; `verification-current.log` is the final green run. No compiled C# or native test is counted.

Format references: [ECMA-335](https://ecma-international.org/publications-and-standards/standards/ecma-335/) Partition II §§22–24 describes CLI table/signature relationships; [Microsoft's published metadata partition](https://download.microsoft.com/download/7/3/3/733ad403-90b2-4064-a81e-01035a7fe13c/ms%20partition%20ii.pdf) §22.22 describes ImplMap; [Microsoft PE format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format) specifies optional-header bounds, data directories and RVA/file-backed sections. No source text was copied into the reader. The full PDFs were inaccessible to the browsing renderer; the published ImplMap search excerpt and PE documentation were accessible. This limited preflight is not a claim of complete ECMA conformance.
