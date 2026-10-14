# round5-staging/lua-alloc — [IS-LUA-ALLOC] staged patches (agent IS, 2026-10-12)

Analysis + patch prep only. NO live-tree edits were made: every base file was
copied into `staged/`, edited there, and verified. Nothing outside this
directory (and the two report .md files named in the task) was written.

Full analysis: `outputs/2026-10-08-l25-prep/IS-LUA-ALLOCATOR.md`
Compact report: `outputs/2026-10-08-l25-prep/agent-IS-luaalloc.md`

## Contents

- `patches/0001-lua-alloc-diagnostic.patch` — engine-side observability:
  counting allocator + `[lua-mem: ...]` note on every LUA_ERRMEM, 1/s
  `[MEM]` line (rides the existing queue-depth throttle, includes
  lua_gc(LUA_GCCOUNT) + one LUA_GCSTEP + allocator counters), new Lua
  binding `GetLuaMemory()`. ALSO carries the 0002 arena code COMPILED IN
  BUT OFF (byte-ident realloc path unless env enables it).
- `patches/0002-lua-arena-enable.md` — activation runbook + decision gate
  (not a code patch).
- `patches/0003-ueselftest-mem-hygiene.patch` — mod-side: `MEM before/after`
  lines bracketing each self-test run via GetLuaMemory(); once-only arming
  for the 8 no-unregister registration families in part9 (crash-resume can
  no longer stack duplicate permanent registrations).
- `staged/` — full staged files (bases listed below, edits applied):
  - `first/src/LuaMadeSimple/LuaMadeSimple.cpp`
  - `first/include/LuaMadeSimple/LuaMadeSimple.hpp`
  - `ue4ss/src/LuaMod.cpp`
  - `ueselftest/main.lua`, `ueselftest/part9.lua`
- `syntax-receipt-LuaMod.log`, `syntax-receipt-LuaMadeSimple.log` —
  prospero-clang++ -fsyntax-only receipts (exact frozen-build flag sets from
  l26-A pass1 command.json, staged file + staged-header shim): both rc=0.
- `arena-host-test.cpp` + run below — the arena allocator region extracted
  verbatim and fuzzed on the host under ASan+UBSan: PASS (200k churn ops,
  alignment/duplicates/resize-in-place verified, zero freed-block misses).

## Base provenance (sha256 of the UNMODIFIED bases)

| Staged file | Base (frozen l26-A SOURCE-SELECTION pin) | sha256 |
|---|---|---|
| first/src/LuaMadeSimple/LuaMadeSimple.cpp | .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/src/LuaMadeSimple/LuaMadeSimple.cpp | a89a0cdfed2bf01f3043530fa1b922d320c34db414ca6b97a0473fc5cd60b9e8 |
| first/include/LuaMadeSimple/LuaMadeSimple.hpp | .../port/first/include/LuaMadeSimple/LuaMadeSimple.hpp | 19ba8e3475ce5ecc923d6177c06d349b878a5ba4b633f95ee4e348efba20314d |
| ue4ss/src/LuaMod.cpp | outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp | bfc392c53f346c57dac9644148aafd0a62b9cdf4790a668d002653c9b7bd16a2 |
| ueselftest/main.lua | outputs/2026-10-08-l25-prep/ueselftest/main.lua | 237e930987b3b633823ab91322d6d6b97ddab0e2b539051fef26d5e7983d4ea8 |
| ueselftest/part9.lua | outputs/2026-10-08-l25-prep/ueselftest/part9.lua | 29274afe6ecafb3749344edbc354469fa9c7b24cf856a17620b96b162d1bd40e |

## Host arena test receipt

```
c++ -std=c++20 -O1 -fsanitize=address,undefined arena-host-test.cpp -o arena-host-test
./arena-host-test
PASS arena-churn blocks=0 live_kb=140407 arena_used_kb=55 ext_kb=78018 fails=0 allocs=200007
```

(live_kb is inflated by the test's shorthand osize=64 on free; the allocator
does not consume osize on free — counter-only artifact of the harness.)

## Apply order for the next build

1. Replace the three pinned engine sources with the `staged/` copies (or
   `patch -p0 < patches/0001-lua-alloc-diagnostic.patch` from the worktree
   root; the patch's `---` side names the pristine live-tree files), re-pin
   in SOURCE-SELECTION, and run the normal composition checks
   (import identity must stay 207: no new imports are introduced).
2. Deploy `staged/ueselftest/{main,part9}.lua` with the next mod push.
3. Do NOT set UE4SS_LUA_ARENA on the diagnostic launch. Decide after reading
   the `[MEM]` evidence, per patches/0002-lua-arena-enable.md.
