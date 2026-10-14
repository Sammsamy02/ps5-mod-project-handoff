# agent-FW — cleanup round: literal-strict markers, remaining-site routing, release mode

2026-10-10, agent FW, Claude cleanup directive (a)+(b)+(c). Tree:
`outputs/2026-10-08-l26-runtime/typescan-r4/` (tree header `include/LuaFNameBridge.hpp`,
candidate sources flat under `candidate/src/`, Lua-type headers under
`candidate/include/LuaType/`). Read first: `FL-CONVERSION-INVENTORY.md`,
`agent-FL-reverse-routing.md`. No `agent-FR-parity-revert.md` existed at start;
FR's `candidate/src/LuaUObject.cpp` untouched (only syntax-checked as a consumer
of my header). Mac on battery: NO builds, NO --execute — single-file
`-fsyntax-only` + one `-E` preprocess grep only.

## Files edited (ONLY FW-owned files touched)

| file | sha256 pre-edit | sha256 post-edit |
|---|---|---|
| typescan-r4/include/LuaFNameBridge.hpp | e47f3b3d8545c7911ffbfb17dd35e9779d432846a4bcde37118a71619a73a858 (= FL's post-edit sha; clean baseline) | 6b8130dc9b99621a700559a1005763b436d6ddccc4a387554ae5aeb56c4c5933 |
| typescan-r4/candidate/src/LuaFNameBridge.cpp | 94bc605702e326781dc65c5254b8f6df9e7b102caac1f60a77dfa9fad12d0c14 | 44ea01e94c123ef96117e4f8dce6c2568fed754c98e6f18c49557d9cc1ce685b |
| typescan-r4/candidate/include/LuaType/LuaUnrealString.hpp | 1702299ccf146b9c0411d90e804f6f66799964a221c76b5840e3b3c20b7053c9 | 09dad9ba65dda1227964c2ddfa7bc3a496c1a1a3bf750ec2bd9cb7a8d66392c0 |
| typescan-r4/candidate/include/LuaType/LuaUObject.hpp (ProcessConsoleExec site ONLY) | ebc9e94ba6f825ff0c784c07203a70df1074a747e591fa034a9f172e8d095e8a | 560eaf6d22e4714fb4c7958c25fb51a7de697036785b882c216acfd60d5606d4 |
| l25-prep/harvest/markers-declared.json | 59387f9880fe2d6191817232c93ee3ee4583f646f317bef702365f156ae43cab | 4c8080a0271b0f830c24579e4e9753019e61f709734ee20ec8ad1a62c65d9c71 |

NOT touched (per rules): `candidate/src/LuaUObject.cpp` (FR),
`candidate/src/NameTypes.cpp` (FK — see FK note below), drivers, `CHANGES.json`
(root syncs), LuaFName.cpp / LuaFText.cpp / LuaLibrary.cpp (FL round-3 files,
already compliant), `candidate/src/LuaUScriptStruct.cpp` (see determination).

## 1. LITERAL MARKERS (directive b) — every diagnostic line from ONE contiguous literal per family

`log_family_marker` previously snprintf-assembled the line from
`"[LFB-M] %s in='%.*s'/len=%llu ok=%s"` + `family_name(f)` — two literals joined
at runtime (why the round-3 gate had to declare composable fragments for
`fname-rev`/`print-widen`). That assembly is GONE. Two whole-literal tables now
live in `include/LuaFNameBridge.hpp`; each entry is passed WHOLE to snprintf —
prefix + family + field specs in a single narrow string constant, so the
rendered prefix lands contiguous in .rodata.

**Marker templates — `family_marker_template(Family)` (hpp:772-787), exact new literals:**

| family | literal (one contiguous constant) | line |
|---|---|---|
| fstring-fwd | `"[LFB-M] fstring-fwd in='%.*s'/len=%llu ok=%s"` | 776 |
| ftext-fwd | `"[LFB-M] ftext-fwd in='%.*s'/len=%llu ok=%s"` | 777 |
| fstring-rev | `"[LFB-M] fstring-rev in='%.*s'/len=%llu ok=%s"` | 778 |
| ftext-rev | `"[LFB-M] ftext-rev in='%.*s'/len=%llu ok=%s"` | 779 |
| fname-narrow | `"[LFB-M] fname-narrow in='%.*s'/len=%llu ok=%s"` | 780 |
| fname-param | `"[LFB-M] fname-param in='%.*s'/len=%llu ok=%s"` | 781 |
| fname-rev | `"[LFB-M] fname-rev in='%.*s'/len=%llu ok=%s"` | 782 |
| print-widen | `"[LFB-M] print-widen in='%.*s'/len=%llu ok=%s"` | 783 |
| (default) | `"[LFB-M] unknown-family in='%.*s'/len=%llu ok=%s"` | 784 |

Rendered line shape is UNCHANGED (`[LFB-M] <family> in='<payload>'/len=<N> ok=<0|1>`,
first-10 per family) — only the construction became one literal, so console
harvests keep matching. Consumer: `log_family_marker` (hpp:859-893) —
signature unchanged; all callers (LuaFNameBridge.cpp, LuaUObject.cpp:1927 FM,
LuaMod.cpp FM helpers) compile unmodified.

**Refusal templates — `family_refusal_template(Family)` (hpp:795-813), exact new literals**
(same whole-literal law; one per family; `fstring-fwd` shown, the other seven
differ only in the family name — ftext-fwd/fstring-rev/ftext-rev/fname-narrow/
fname-param/fname-rev/print-widen/unknown-family):

```
"[LFB-M] fstring-fwd refused reason=%s len=%llu at=%llu site=0x%03llX units=%s (yielding empty; no wide-libc fallback)"
```

(lines 798-807; fstring-rev at 801, default at 807). Consumer:
`notify_family_refused` (hpp:903-933) — signature and first-10 semantics
unchanged.

**Already contiguous, verified unchanged:**
- `[LUA-FN]` first-20 line (hpp:650): `"[LUA-FN] in='%.*s' len=%llu id=0x%llX decoded='%.*s' %s"` — was and remains ONE literal (not assembled); no change needed, now also release-gated (below).
- Bridge-forward refusal (hpp:583): `"[FN] seam refused: reason='%s' len=%llu at=%llu site=0x%03llX units=%s (bridge fwd; NAME_None produced; no wide-libc fallback)"` — one literal.
- Composition stamp (hpp:982): `"[LFB] lua-fname-bridge composed: hand utf8<->utf16, slot-2 ctor direct\n"` — one wide literal.

**FOR FK/ROOT (not edited by me):** FK's NameTypes.cpp:63-77 `[FN] seam refused:`
notice — I did not verify its literal contiguity; if FK's first-10 line is
snprintf-assembled from fragments, the same whole-literal rework belongs to FK.

## 2. REMAINING SITES ROUTED (directive a; FL inventory [ROOT]/unowned items)

1. **LuaUObject.hpp ProcessConsoleExec cmd echo** (inventory §3 hpp:735-760,
   was unrouted `ensure_str(lua.get_string())`):
   `candidate/include/LuaType/LuaUObject.hpp:747-780` — under `__PROSPERO__` the
   command now converts via the stack-state `LuaFNameBridge::Bridge
   cmd_bridge{lua.get_lua_state()}` (replicates `get_string()`'s
   lua_tolstring+lua_remove at idx 1 exactly; sequential parsing unchanged),
   logs the `fstring-fwd` family (line 759), and the engine call passes
   `FromCharTypePtr<Unreal::TCHAR>(cmd_bridge.units())` (line 780;
   units() is NUL-terminated — the same pointer contract `cmd.c_str()` had).
   Refused input -> empty command after the loud first-10 `[FN] seam refused:`
   bridge-forward notice. Host `#else` keeps `ensure_str` (upstream).
2. **LuaUnrealString.hpp FString userdata members** (inventory §2 rows 198-254,
   was [FM/ROOT], now FW's file):
   - `__tostring` metamethod — `LuaUnrealString.hpp:44-102`: attached to the
     registry-named metatable via raw Lua C API (`luaL_getmetatable` +
     `lua_pushcfunction` + `lua_setfield`, FL's FName/FText pattern) under
     `__PROSPERO__`. FString payloads ride
     `LuaFNameBridge::push_fstring_to_lua` (fstring-rev family, line 72);
     FUtf8String/FAnsiString are natively narrow and push their bytes via
     `lua_pushlstring` (no wide conversion exists to route — inventory
     trivial-narrow). This closes the "NO Lua type registers __tostring" gap
     for the three string userdata types; `str(fstring_userdata)` now returns
     the string, not `FStringUserdata: 0x...`.
   - `get_string_from_object` (FString) — `LuaUnrealString.hpp:301-305`: was
     `to_utf8_string(*str)` (Helpers seam); now
     `LuaFNameBridge::fstring_to_narrow_string(str)` — NEW exported bridge API
     (decl hpp:953-964, def `LuaFNameBridge.cpp:166-200`): hand UTF-16->UTF-8,
     kMaxPushUnits bound, `fstring-rev` family markers + first-10 refusals with
     site tag/hex units, empty string on refusal.
   - `append_string` (FString) — `LuaUnrealString.hpp:307-322`: was
     `ensure_str` + engine NUL-scan `Append`; now memory-source `Bridge` +
     `str.AppendChars(arg.units(), int32(arg.unit_count()))` ((ptr,count), no
     Strlen), `fstring-fwd` family.
   - `find_substring` / `starts_with` / `ends_with` (FString) —
     `LuaUnrealString.hpp:324-377`: same memory-source Bridge; the
     NUL-terminated `arg.units()` feeds the existing `Find/StartsWith/EndsWith
     (const CharType*)` overloads (pointer contract unchanged); refused/empty
     arg yields INDEX_NONE/false; `fstring-fwd` family. Host `#else` branches
     keep the Helpers codec (Claude item 9).
3. **LuaUScriptStruct.cpp — NOT edited (determination):** root's field-key fix
   is already in (`LuaUScriptStruct.cpp:211-220`, Prospero Bridge route). The
   only remaining conversions there are `to_string(...)` inside `throw_error`
   message text (lines 184-186, 201-203) — inventory class (c)-low
   (error-message text; Helpers retained per Claude item 9). No inventory item
   remains beyond root's fix.
4. **FR-owned leftovers (listed for FR/root, NOT touched):** inventory §3
   items living in `candidate/src/LuaUObject.cpp`: the `#else`-miss-branch
   `handle_property(field, to_utf8_string(field->GetName()))` conversions
   (LuaUObject.cpp ~815-821/931-937) and the class-(c)-low error-text/log
   conversions — FR owns that TU this round.

## 3. RELEASE MODE (directive c) — `MODBRIDGE_RELEASE_DIAG_OFF`

Compile-time define (documented in both bridge file headers, hpp:81-95 and
LuaFNameBridge.cpp:32-44). Default remains VERBOSE (current behavior).

- **COMPILED TO NOTHING** under the define (literals `#if`-ed out of the TU):
  the `[LUA-FN]` first-20 logger (`log_use`, hpp:599-675 — early return +
  `(void)made` in release) including its narrow fallback line, and every
  `[LFB-M] <family> in=...` per-call marker (`family_marker_template` +
  `log_family_marker` release early-return, hpp:759-787/859-893).
- **KEPT in release** (safety/behavioral evidence): `[LFB] lua-fname-bridge
  composed` stamp (hpp:982), the `[FN] seam refused:` bridge-forward first-10
  lines (hpp:583), the `[LFB-M] <family> refused ...` family refusal lines
  (family_refusal_template is NOT gated).
- **[FFO2] — NOT in my files:** the `[FFO2] q='...'` per-call diagnostics live
  in `candidate/src/UObjectGlobals.cpp:1038/1044` (outside my file list).
  GObjectGlobals' owner should gate those sends against the same define;
  flagged for root.
- **Manifest note (also written into markers-declared.json):** per-call
  entries (`[LUA-FN]`, `[LFB-M]` family markers) are guaranteed composed only
  in the default VERBOSE build; a release build intentionally omits them
  (refusal/composition markers remain), so the manifest gate targets the
  verbose artifact.
- **Proof (single-file, no codegen):** `lua-lfb` preprocessed twice with the
  exact unit flags (`-E`, `-ivfsoverlay` stripped):
  VERBOSE — `[LUA-FN] in='`, `[LFB-M] fname-rev in='`, `[LFB-M] print-widen
  in='`, `[LFB-M] fstring-fwd in='`, both refusal literals, `[FN] seam
  refused:`, and the composed stamp ALL present;
  RELEASE (`-DMODBRIDGE_RELEASE_DIAG_OFF`) — ALL per-call marker literals
  ABSENT, refusal/seam literals + composed stamp STILL present.

## 4. Syntax evidence (prospero clang++ -fsyntax-only, battery-safe)

Flags from `outputs/2026-10-08-l26-build-variants/l26-A/pass1/objects/<unit>.command.json`,
`-ivfsoverlay/-c/-o/-MD/-MF` stripped, `-fsyntax-only` added,
`LLVM_CONFIG=.../ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`.
All rc=0, 0 errors:

| unit | what it covers | rc |
|---|---|---|
| lua-lfb (VERBOSE) | LuaFNameBridge.cpp + tree header (templates, release gates, fstring_to_narrow_string) | 0 |
| lua-lfb (RELEASE, +`-DMODBRIDGE_RELEASE_DIAG_OFF`) | release path compiles | 0 |
| lua-luauobject (VERBOSE + RELEASE) | LuaUObject.hpp ProcessConsoleExec edit (FR's TU, header-level check only) | 0 / 0 |
| luamod (VERBOSE + RELEASE) | LuaUnrealString.hpp __tostring + FString helpers (LuaMod.cpp is the in-tree consumer) | 0 / 0 |
| lua-luauscriptstruct | root's fix coexists; header dependents intact | 0 |
| lua-luafname | FL's FName __tostring still compiles against reworked header | 0 |

Only warnings seen: pre-existing port-tree `Atomic.hpp` -Wswitch (lua-lfb) and
the deprecated `GetComparisonIndex` trace (lua-luauobject) — both flagged by FL
last round, lines untouched by FW.

## 5. Manifest changes (markers-declared.json, valid JSON, 15 markers)

- `fname-rev-userdata`: composable-fragment declaration REPLACED by one
  contiguous requirement `"[LFB-M] fname-rev in='"` (utf-8; source hpp:782) —
  the gate is literal-strict again for this marker.
- `print-widen-global`: same — `"[LFB-M] print-widen in='"` (utf-8; hpp:783).
  utf-8 only, matching what is emitted: the lines render narrow-first and are
  widened at runtime by the bridge hand loop, so no utf-16le literal exists.
- `lfb-m-marshaling` (the other [LFB-M] entry — CHECKED, still matches):
  required text `[LFB-M]` (either_encoding) is present verbatim inside every
  new per-family literal (utf-8, hpp:776-807). Source note tightened to the
  template tables; the old generic assembled template is gone.
- Refreshed source pointers for drifted line numbers in my header:
  `lua-fn-bridge-log` -> hpp:650 (plus release-define caveat), `lfb-composed`
  -> hpp:982 (plus not-release-gated note).
- FOR ROOT (not added, root owns manifest sync): FK's NameTypes.cpp refusal
  literal and the bridge-forward `[FN] seam refused:` variant remain
  undeclared entries; both are contiguous now and declarable if root wants
  them gated.

## Notes / interpretation decisions

- `family_name()` retained (hpp:722-736) for debug/tooling use; it no longer
  feeds any marker line.
- Release define chosen compile-time (`#if defined(MODBRIDGE_RELEASE_DIAG_OFF)`)
  per task instruction — simpler and provable (preprocess grep above) vs an
  ini/env runtime read.
- `widen_narrow_to_stringtype`, `push_wide_units_to_lua`, refusal channels:
  behavior and line shapes unchanged from FL's round 3; only literal
  construction + release gating changed, plus the new
  `fstring_to_narrow_string` API.
- LuaUnrealString FUtf8String/FAnsiString helper sets use no Helpers codecs
  (natively narrow `std::string` temp + reinterpret_cast) — inventory
  trivial-narrow, left as-is.
