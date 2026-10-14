# agent-EV — Lua→C++ string seam: "PlayerController" arriving as "P"

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/` (consumption law honored; CHANGES rows synced).
Date: 2026-10-09. All paths below relative to that tree unless absolute.

## TASK 1 — the hop where 16 chars became 1 (hex-dump trace)

The three hops, with the exact lines:

**Hop A — Lua → narrow bytes (CLEAN, 16 bytes).**
`candidate/src/LuaMod.cpp:1800` (binding registered at :1781):
`Unreal::UObjectGlobals::FindFirstOf(ensure_str(lua.get_string()))`.
`Lua::get_string` (port `first/src/LuaMadeSimple/LuaMadeSimple.cpp:755-760`) is
`lua_tostring` + `lua_remove` returning a **narrow `std::string_view`** — 16 bytes
`50 6C 61 79 65 72 43 6F 6E 74 72 6F 6C 6C 65 72`. Nothing wrong here.

**Hop B — narrow → wide (CORRUPTION ENTERS; size stays 16, bytes go wrong).**
`ensure_str` → `to_wstring(std::string)` (`candidate/include/Helpers/String.hpp:188-205`)
→ `Unicode::DecodeUtf8` (`candidate/include/Helpers/Utf16Utf8.hpp:59-103`).
Pre-fix, DecodeUtf8 built its output with `basic_string<wchar_t>::push_back`, and
`to_wstring` used iterator-pair construction (`std::wstring{tmp.begin(), tmp.end()}`).
On Prospero (wchar_t = 2 bytes), any such member that resolves to the prebuilt
libc++ **extern-template instantiation compiled for 4-byte wchar_t writes 4-byte
strides**: the buffer bytes become
`50 00 00 00 6C 00 00 00 61 00 00 00 ...` while `size()` still says 16. Read as
2-byte units: `P, NUL, l, NUL, a, NUL, ...`. Host-reproduced under `-fshort-wchar`
(the 4-byte host wchar_t hides it — that is why every C++-literal init test passed).

**Hop C — the FName ctor's no-length NUL-scan (WHERE 16 UNITS BECOME 1 CHAR).**
Pre-fix `candidate/include/Unreal/NameTypes.hpp` `explicit FName(StringViewType, ...)`
forwarded `str_name.data()` into `construct_with_string(const CharType*, ...)`,
whose bounded NUL-scan (`while (Length < NAME_SIZE && StrName[Length] != 0)`)
stopped at unit[1]==0 → **Length=1** → `ConstructorInternal(Owned, FindType)` → the
engine ABI `(out, const TCHAR*, mode)` NUL-scans its input → the ctor literally
received `"P"` and correctly interned the real one-letter pool name "P"
(index 0x1DA13). NOT a decoder-side bug; the input string was already destroyed
at Hop B and silently truncated at Hop C.

## TASK 2 — the fix (single choke point + production-side prevention)

`candidate/include/Unreal/NameTypes.hpp`:
- **Choke point** `construct_with_sized_string` (:331-370): every typed caller now
  carries an explicit LENGTH; copies through a raw stack buffer with
  `FMemory::Memcpy` + `operator[]` only (never a possibly out-of-line wide-string
  member), writes its own NUL terminator, audits for **embedded NUL within the
  length** — the corruption signature — and REFUSES LOUDLY (`NAME_None` +
  `[FN] seam refused: reason='embedded-nul' len=N at=K` via
  `NotifyFNameSeamRefusedOnce`, `candidate/src/NameTypes.cpp:63-79`) instead of
  silently truncating. Overlong (≥ NAME_SIZE) refused the same way.
- StringViewType ctors (:427-438) forward `.data()/.size()` — never a NUL-scan.
  The literal `const CharType*` entry (:321-329) keeps engine-equivalent semantics
  via a bounded inline NUL-scan, then funnels into the sized path.
- **static_asserts** (:318-319): `sizeof(CharType) == sizeof(TCHAR)` and width ∈
  {2,4} — compile-time width contract standing in for deleted overloads (a wide
  buffer can no longer masquerade as a narrow one anywhere behind this seam).
- Hop-B prevention: `Helpers/Utf16Utf8.hpp` DecodeUtf8 uses resize+`operator[]`
  (no push_back); `Helpers/String.hpp` to_wstring/to_string use `(ptr,count)`
  construction. The ET fix's `NulTerminatedNameCopy`
  (`candidate/src/UObjectGlobals.cpp:195-199`) copies sized `(View.data(),
  View.size())` — narrow-slice safe.

## TASK 3 — host harness (results/ev-string-seam-verify/)

`tests/actual_ev_string_seam.cpp` + `tests/run_ev_seam.py`: compiled with
**`-fshort-wchar`** (wchar_t = 2 bytes, like Prospero) linking the **real Lua 5.5
interpreter**; pushes `'PlayerController'` from a real `luaL_dostring` chunk
through the verbatim `get_string` body, the REAL `ensure_str`/`DecodeUtf8`
candidate headers, the REAL `rc::fnameabi` adapter, and the VERBATIM
runner-extracted NameTypes/UObjectGlobals seam bodies (marker+sha pinned in
`EV-REGION-MANIFEST.json`) into a stub engine ctor that records the code units.

Result — **4/4 PASS** (`EV-STRING-SEAM.json`):
- green plain/asan-ubsan: `[ST] lua-ffo PlayerController PASS (16 code units at
  the ctor)` — all 16 units equal `u"PlayerController"`; plus the
  `/Script/Engine` 15-unit slice pin, a DecodeUtf8 regression pin, and the
  overlong refusal.
- red plain/asan-ubsan: the unfixed-library interleave (`P,0,l,0,...`) built
  explicitly is REFUSED LOUDLY (NAME_None + embedded-nul marker), never
  truncated — `ev-string-seam red: REFUSED LOUDLY (PASS)`.
- Runner bug found+fixed en route: the verbatim-region cut stopped at the first
  inner `}` (strip-match), truncating `MakeFindXOfClassName` mid-function; now a
  depth-aware `cut_function`.

## TASK 4 — Lua-originated self-tests at init

`candidate/src/LuaMod.cpp:116-160` (`L23GameThreadProbe::callback`, Prospero):
a tiny pcall'd Lua chunk runs on the game thread at init through the ACTUAL
registered bindings — exactly the seam the C++-literal battery missed:
`[ST] lua-ffo PlayerController`, `[ST] lua-ffo ItemDatabaseSubsystem`,
`[ST] lua-sfo /Script/Engine.PlayerController`,
`[ST] lua-sfo /Script/UMG.Default__WidgetBlueprintLibrary`
(PASS = call ok AND non-nil; plus `lua-seam probe chunk failed/exception`
failure markers). The host harness prints the same `[ST] lua-ffo` line.

## TASK 5 — FFO fallback compares the ORIGINAL query text

`candidate/src/UObjectGlobals.cpp`:
- `FindXOfQueryMismatch` (:842-855): round-trip compares `Name.ToString()`
  against the ORIGINAL query view — never the decoded constructed name.
- `LogFindXOfQueryFallback` (:803-841): now also logs `qlen=` and the first 4
  query units, so a corrupt QUERY is distinguishable from a corrupt ctor input.
- New query-side tripwire (:774-802, :943-947, :958-961, :1068-1071): an
  embedded-NUL query logs `[FFO] query-embedded-NUL (len=… at=… units=… …)`
  and refuses before any construction — the string walk could never match it.
- Fallbacks `FindFirstOfStringWalk`/`FindAllOfStringWalk` (:872-934) compare
  stored names against the ORIGINAL query (init-scan comparator shape).

## VERIFY

- **Prospero target compile** (`tests/run_ev_target.py` → `results/EV-TARGET-COMPILE.json`):
  the three Z13 recipes (ES unreal-nametypes, EU luamod, agent-CP FINDXOF
  unreal-uobjectglobals) re-run against the CURRENT seam-fixed sources — all
  exit 0. New durable overlay `results/headers-ev.json` additionally pins the
  two seam-fixed Helpers headers the older overlays never mapped (`.d` files
  prove `candidate/include/Helpers/{String,Utf16Utf8}.hpp` were the compiled
  ones). Only inherited upstream warnings.
- **Host suites** (`tests/run_ev_rerun.py` → `results/HOST-TESTS-EV-RERUN.json` +
  canonical suite files): custom 2×5, policy 2×17, getters 2×28, backend 2×18,
  eq-pinadmission 2×19, er-pooldecoder 4 suites × 2 modes — ALL PASS.
  `tests/run_et_verify.py`: 16 entries ALL PASS. `tests/run_ev_seam.py`: 4/4.
- **CHANGES.json synced** (`tests/run_ev_changes_sync.py`): the four seam rows'
  hashes were re-synced to the edited sources, and **two new replacement rows
  appended** — `candidate/include/Helpers/String.hpp` and
  `.../Helpers/Utf16Utf8.hpp` (originals: port `first/include/Helpers/`,
  copied_originals staged under `original/include/Helpers/`). Without these
  rows the parent build resolved `<Helpers/...>` from the unfixed port tree and
  only the NameTypes half of the fix would ship. All 32 rows verified exact.
- **NOT run / disclosed**: `test_scan.py` — a concurrent agent is actively
  editing `tests/actual_scan.cpp` (mtimes 19:03→19:08 today; it references
  guard members the unchanged Oct-8 header does not define). Racing their
  in-flight fixture would test their intermediate state, not the tree. Two
  PRE-EXISTING fixture-vs-rowed-source gaps were repaired (unrowed fixtures
  follow the rowed source): `actual_getters.cpp` stub FName gained the
  `ToString` the rowed l25 `.inl` requires; `actual_backend.cpp` gained the
  `bridge_image_range` stub (l23 module-info policy) the rowed l24+ `.inl`
  requires, and its fault-ledger assertion was updated to the rowed l24+
  contract (fault gate moved from PropertyReadBytes into the bridge class-range
  route — still asserted there). No rowed/production source was modified by
  these repairs.
- No console contact, no uploads, no commits. Gameplay/engine runtime behavior
  beyond the seam itself is not claimed.
