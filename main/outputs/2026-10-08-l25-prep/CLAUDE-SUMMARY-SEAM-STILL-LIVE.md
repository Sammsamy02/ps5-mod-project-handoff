# Seam fix deployed but truncation persists — need path analysis

## What we deployed (PRX 58a30cd1, verified on console)
1. Helpers/String.hpp + Utf16Utf8.hpp fixes: (ptr,count) construction,
   resize+operator[] (replaces push_back/iterator-pair that resolve to the
   prebuilt 4-byte-stride libc++ on Prospero)
2. NameTypes.hpp fix: FName(StringViewType) now uses
   construct_with_sized_string (explicit length, no NUL-scan, embedded-NUL
   audit → loud refusal)
3. Build VERIFIED: the compiled luamod.o and uobjectglobals.o both consumed
   the FIXED candidate headers (dep files prove it)

## What the console shows (SAME as before the fix)
- [FFO] query-fallback [n:1DA13 num:0 ts:'P'] — the ctor STILL receives "P"
- Same three one-char indices as every previous build
- ZERO [FN] seam refused markers (the embedded-NUL audit never fired)
- All 19 C++-literal self-tests still PASS with full decodes

## The gap
The fix targets: ensure_str → Unicode::DecodeUtf8 → (ptr,count) wstring →
FName(StringType) → sized construction

But the Lua query still arrives as one char. Since:
- The Helpers ARE in the compiled objects (dep files confirmed)
- The same indices appear (the string was ALWAYS one char before the pool)
- The C++ literals work (they never cross the narrow→wide seam)

The Lua string must take a DIFFERENT path to the FName ctor than the one we
fixed. Candidates:
1. lua.get_string() returns std::string_view (narrow). Maybe the LuaMod
   FindFirstOf binding at :1801 converts it differently — not through
   ensure_str/DecodeUtf8 but through a direct reinterpret or a different
   overload that isn't in the Helpers we fixed
2. The NulTerminatedNameCopy in UObjectGlobals.cpp (~:195) — the ET fix —
   may itself be the truncator: it takes a StringType (wide) and copies
   into a buffer. If IT reads wide as narrow, it produces "P\0..." and the
   sized ctor would see 1 char with no embedded NUL (it starts narrow)

## What we need
A hex-dump at the EXACT point the Lua string enters FName construction —
in the FFO path specifically. The Lua-originated self-test ([ST] lua-ffo)
was NOT included in this build (the agent that wrote it also wrote the fix,
and the self-test lines are in LuaMod.cpp but may not have been composed
into the final PRX). The self-test would have shown the Lua path's behavior
at init before we wasted a console launch.
