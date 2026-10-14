# STAGED CANDIDATE — luamod-lua-api-r1

- File: `LuaMod.cpp`, sha256 `c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0`
- Byte-identical to the frozen candidate
  `worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp` (c92c92ff); `diff -q` clean.
- Reason for zero diff: of the 36 audited Lua-API-family conversion sites, 0 are FUNCTIONAL
  in the current composition — every route resolves to the adopted Utf16Utf8-based
  `Helpers/String.hpp` conversions (verified per site; SITE-TABLE.md). Staging per-site
  widening here would duplicate a header-level fix, change no behavior, and force a declared
  comparator divergence for nothing.
- Proposal-only staging; not wired into any compose input. The compose input remains the
  l11-merged path until the controller admits a candidate.
