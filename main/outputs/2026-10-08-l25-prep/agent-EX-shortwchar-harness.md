# agent-EX — -fshort-wchar host harness for the Lua string seam bug (2026-10-09)

Harness: `/tmp/seam-harness/` (self-contained; fix-independent — the fix is a
compile-time switch, default OFF so the bug reproduces).
Source tree: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (the
current lineage the l25-prep reports CP/DA/EB/ER/ET/EV/EM built on).
Toolchain: Apple clang 17.0.0 (clang-1700.6.4.2), arm64-apple-darwin24.6.0.

## 1. Verdict up front

The bug class reproduces NATIVELY on the Mac host with one flag — no console,
no artificial shim, no fake ABI hack:

| config | wchar_t | codec | engine FName ctor received | expected | result |
|---|---|---|---|---|---|
| A_wide_default | 4 bytes | pre-fix (prebuilt codecvt boundary) | **16 units** "PlayerController" | 16 | PASS (bug hides on host) |
| B_short_prefix (`-fshort-wchar`, Prospero width) | 2 bytes | pre-fix | **1 unit 'P'** | 1 | PASS (**bug reproduces**) |
| C_short_fixedcodec (`-fshort-wchar` + staged fix) | 2 bytes | fixed (verbatim `RC::Unicode::DecodeUtf8`) | **16 units** | 16 | PASS (fix validates GREEN) |
| D_wide_fixedcodec (no-regression control) | 4 bytes | fixed | **16 units** | 16 | PASS |

All four also pass under ASan+UBSan (harness itself is UB-free; the corruption
is the real library boundary, not harness UB). Runner exits 0 only when every
expectation holds: `results/SUMMARY.txt`.

## 2. Why it reproduces natively (the mechanism, verified by hexdump)

The pre-fix tree widen was
`std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(input)`
(Helpers/String.hpp "was" notes :191-196/:270-274). Its conversion runs in
PREBUILT library code: `__codecvt_utf8_utf16<wchar_t>::do_in` is
`_LIBCPP_EXPORTED_FROM_ABI` — it lives in the system libc++ dylib, compiled
with the default 4-byte-wchar_t model. Under `-fshort-wchar` the app TU hands
its 2-byte-typed `wchar_t*` destination to that 4-byte-strided prebuilt code.
Identical raw bytes in both configs (probe evidence,
`results/probe_codecvt_{wide,short}.log`):

```
50 00 00 00 6C 00 00 00 61 00 00 00 79 00 00 00 ...   (P, l, a, y ...)
```

- wide TU: 16 units of 4 bytes each -> "PlayerController"; NUL-scan = 16.
- short TU: SAME bytes read as 2-byte units -> 'P',0,'l',0,'a',0,... and
  `to_next` reports 32 units for 16 chars (the documented "/2 to_next fixup"
  family). The engine-side FName ctor (`const CharType*`, NUL-scan law) stops
  at the first embedded NUL -> **receives 1 unit 'P'** — the console field
  shape '[FFO] query-fallback ts:P' and the '/Script/Engine' Find misses.

This is the same mismatch class as the PS5 (app built `-fshort-wchar`, SDK
prebuilt libc++ locale code built at the 4-byte model — Helpers/String.hpp
:15-23 staged-fix note): host "works" only because host app+library are both
4-byte.

Note for porters of this harness: **Apple clang 17's libc++ REMOVED
`std::wstring_convert`** (compile error: no member named 'wstring_convert').
`wstring_convert{}.from_bytes` was a thin wrapper over exactly the facet
`codecvt::in()` call, so the harness calls the facet directly — the same
prebuilt-library boundary. This is the only substitution in the seam (see
`to_wstring_pre_fix` in seam_harness.cpp); it is documented in-source.

## 3. What was extracted verbatim (sha-pinned, extracted/MANIFEST.txt)

| region | source (sha256 prefix) | lines | inc |
|---|---|---|---|
| FindFirstOf Lua binding (`lua.get_string()` + `ensure_str` + FindFirstOf chain) | candidate/src/LuaMod.cpp `c2d09abf…` | 1781-1814 | luamod_findfirstof_binding.inc |
| NulTerminatedNameCopy + seam-law comment | candidate/src/UObjectGlobals.cpp `aeebc94b…` | 184-199 | uobjectglobals_nulterminatednamecopy.inc |
| g_ffo globals + LogFindXOfAddFallback | same | 681-699 | uobjectglobals_ffo_fallback.inc |
| MakeFindXOfClassName (the FName construction chain) | same | 700-715 | uobjectglobals_makefindxofclassname.inc |
| FindFirstOf(const StringType&) — the overload the binding hits | same | 1038-1045 | uobjectglobals_findfirstof_stringtype.inc |
| engine NUL-scan law `construct_with_string` | candidate/include/Unreal/NameTypes.hpp `cebf6679…` | 321-329 | nametypes_construct_with_string.inc |
| fixed codec DecodeUtf8 | candidate/include/Helpers/Utf16Utf8.hpp `5cd6a92b…` | 59-103 | utf16utf8_decodeutf8.inc |
| String/StringType.hpp contract | port deps/first String `1e821d82…` | whole file | StringType.hpp (byte-exact copy) |

Full SHAs and per-region content SHAs in `/tmp/seam-harness/extracted/MANIFEST.txt`.
Lineage note: the coordinator's "LuaMod.cpp ~:1651" is the l22-diag numbering
agent-CP pinned for the failing console path; the identical binding in the
current l26-r4 candidate tree sits at :1781 (sha `c2d09abf…`; ET-era sha
`d9d96e45…` predates the EI/EM-era edits). "UObjectGlobals.cpp ~:528" is
agent-CP's l22-diag line for the FindFirstOf(const CharType*) overload;
NulTerminatedNameCopy exists only in the current candidate (:195, extracted
with its law comment at :184-199).

## 4. Harness layout

```
/tmp/seam-harness/
  seam_harness.cpp        driver: stubs + verbatim includes + VERDICT print
  StringType.hpp          byte-exact port contract (CharType=wchar_t, STR()=L"")
  extracted/*.inc         verbatim regions (banner = only added text)
  extracted/MANIFEST.txt  source shas + line ranges + region shas
  extract.sh              re-pulls every region from the pinned tree
  build_and_run.sh        builds A/B/C/D, asserts expectations, SUMMARY.txt
  probe_codecvt.cpp       30-line standalone minimal reproduction (the
                          raw-bytes/NUL-scan evidence, no glue)
  results/                per-config logs + SUMMARY.txt + probe evidence
```

Stubbed ONLY the Lua state (`LuaMadeSimple::Lua` with a stack preloaded with
the narrow string "PlayerController"; `register_function` runs the binding
immediately) and the engine FName call (`construct_with_sized_string` is the
stubbed engine boundary; the VERBATIM `construct_with_string` NUL-scan above
it is the real law and performs the scan). The binding, ensure_str chain,
FindFirstOf(StringType) -> MakeFindXOfClassName -> NulTerminatedNameCopy ->
FName(StringViewType forwarding `data()` with NO length) all execute verbatim.
`FindFirstOfVerified`'s audit/mismatch walk is a documented stand-in (out of
scope for this seam; it keeps the verbatim construction chain).

## 5. Exact commands (what the fix agent needs)

Compile (single TU; `-D__PROSPERO__` follows the tree's own host-compile
recipe, cf. agent-ET's verify matrix):

    clang++ -std=c++17 -Wno-deprecated-declarations -D__PROSPERO__ \
        seam_harness.cpp -o harness_wide                      # config A
    clang++ -std=c++17 -Wno-deprecated-declarations -D__PROSPERO__ -fshort-wchar \
        seam_harness.cpp -o harness_short                     # config B
    clang++ -std=c++17 -Wno-deprecated-declarations -D__PROSPERO__ -fshort-wchar \
        -DSEAM_HARNESS_FIXED_CODEC seam_harness.cpp -o harness_short_fixed   # config C

Or everything at once (asserts all four expectations, exit 0 = all met):

    /tmp/seam-harness/build_and_run.sh

Sanitizer variant (any config): append `-fsanitize=address,undefined`.

## 6. How the fix agent uses this for validation

1. RED: run config B pre-fix — `received_units=1 first='P'` proves the
   reproduction of the seam class against their fix candidate's baseline.
2. Apply the fix inside `to_wstring_pre_fix` (or define
   `SEAM_HARNESS_FIXED_CODEC` to use the tree's own staged fix verbatim) and
   rebuild config C: must print `received_units=16`. Config D guards the
   4-byte build against regressions.
3. The harness pins TWO independent seams that compose at this boundary:
   the widen codec (the interleave source — fixed by the DecodeUtf8 route)
   and the no-length `data()` forwarding into the engine ctor's NUL-scan
   (armored by NulTerminatedNameCopy + the sized path in the current tree).
   With a corrupted buffer the owned copy alone does NOT restore the name
   (it faithfully copies the interleaved units and the engine still scans
   to 1) — i.e., the codec fix is the load-bearing one for this symptom.

## 7. Honest limits

- This is a host reproduction of the SEAM CLASS: the prebuilt-library stride
  mismatch is realized through the system libc++ dylib's exported
  `__codecvt_utf8_utf16<wchar_t>::do_in`, not the PS5 SDK's exact
  locale.cpp.o internals (those are only observable on console). The
  documented console observables (halved renders, '[FFO] query-fallback
  ts:P', '/Script/Engine' misses) all match this mechanism.
- `std::wstring_convert` itself no longer exists on this toolchain; the
  harness substitutes the facet call it wrapped (documented, same boundary).
- The name pool / FindFirstOf(FName) walk / verified-query mismatch machinery
  is stubbed (FindFirstOfVerified stand-in) — this harness validates the
  string->FName seam, not the GUObjectArray walk (covered by the tree's own
  fixtures, e.g. agent-ET's actual_et_nul_terminator.cpp).
- No console contact, no claim about any deployed package; /tmp artifacts are
  not in the port tree.
