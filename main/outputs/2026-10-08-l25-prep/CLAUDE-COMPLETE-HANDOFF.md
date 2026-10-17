# COMPLETE HANDOFF FOR CLAUDE — 2026-10-09 late evening — MYSTERY SOLVED

## HEADLINE: the "dead code" was never in the binary. Build-staging race.

The LFB (Lua FName Bridge) session's paradox — "Bridge compiled in, on the
only registration path, but zero [LUA-FN] log lines" — is resolved. The
bridge was NOT in the PRX that ran on console. Tonight's offline forensics
(all directly re-verified against the shipped artifacts):

1. `objects/luamod.o` of the lfb build (l26-A.lfb-203415): ZERO LuaFNameBridge
   symbols, ZERO "[LUA-FN]" strings.
2. Dependency files: ZERO of the 199 compiled units reference
   `LuaFNameBridge.hpp` in its `.d` — no TU ever included it.
3. The final PRX (58a30cd1, the one uploaded and run): "[LUA-FN]" and
   "[LFB]" markers ABSENT in both UTF-8 and UTF-16LE (these PRXes store log
   strings UTF-16LE — cross-checked against "[FFO] query-fallback" which IS
   present as UTF-16LE in the seamfix PRX).
4. SOURCE-SELECTION.json of the lfb build pins candidate LuaMod.cpp at
   sha c2d09abf / 449,311 B. Current disk: 9fea2963 / 455,192 B (+5.9 KB
   = the bridge routing). EY's routing edits landed AFTER the build's
   source pins were taken — the classic parallel-agent race we hit several
   times tonight ("stale CHANGES rows").
5. Same story, earlier round: the seamfix build (190412) pinned LuaMod.cpp
   at 0c368454 — EV's helper fixes that DID land are in NameTypes/String
   code, but the Lua self-test markers "[ST] lua-ffo" were composed out of
   both binaries (FB's verify gate caught this class earlier tonight).

So the console evidence is fully consistent: [FFO] markers fire from the
OLD string path (UObjectGlobals.cpp MakeFindXOfClassName — that code WAS
in the binary), producing the same one-char indices ('I','P','C'), because
every seam fix from tonight's seamfix+lfb rounds never actually shipped.

## What is still TRUE and unanswered (the real review asks)

The string seam itself: Lua strings arrive at the FName constructor as ONE
character ("P" not "PlayerController"). Every host-side fix proves correct;
no binary containing a fix has ever run on console. Claude's review of the
FIX STACK (not the phantom) is what matters now:

1. EV's helper repairs (DecodeUtf8 resize+operator[]; String.hpp ptr,count;
   sized FName ctor) — correct on host, unverifiable on console until a
   build actually carries them.
2. EY's bridge (include/LuaFNameBridge.hpp): hand UTF-8→UTF-16 loop →
   bounded char16_t buffer → slot-2 ctor (0x23D590) DIRECT; reverse via the
   ER pool decoder → hand UTF-16→UTF-8 → lua_pushlstring; loud refusal on
   embedded-NUL/invalid/incomplete/non-scalar/overlong; [LUA-FN] first-20
   logging. Routed at LuaMod.cpp :1720/:1782/:1831/:1886 + 14 more sites
   (LuaUObject, LuaFName, RegisterHook, __index/__newindex).
3. Claude's Q1 from the dead-code summary (why do string-path FFO markers
   fire when the bridge calls the FName overload) is MOOT for that session
   (bridge absent) but stays relevant for the NEXT session as the
   pass/fail oracle: if [LUA-FN] fires AND [FFO] string-path markers STILL
   fire with one-char indices, then a second caller exists.

## Build-process answer (Claude asked "what takes so long")

Cycle anatomy 25-45 min: diagnosis 5-30 / code 5-15 / BUILD 12-18 /
package 3-5 / upload+prep 2-4 / harvest 1-2.
- 199 units × twin passes ≈ 800 compiles; header change cascades (one
  .hpp touches up to 196 TUs — String.hpp=196, NameTypes.hpp=168).
- Plan phase simulates the include graph for all units before compiling.
- NEW (agent EZ, landed tonight): `--incremental` + `--base-build DIR` —
  verified 20 fresh/354 retained/94 steps for .cpp-only changes (1-2 min
  vs ~15). Explicitly NOT upload-grade (warnings list skipped headers +
  ODR risk); full mode remains the only upload mode. Retained units are
  re-verified against the base build's own pins; drift forces rebuild.
- Lua-only changes: lua_fast_deploy.sh = 30 s, no PRX rebuild.
- verify_build.sh (agent FB): greps the PRX for composed markers BEFORE
  upload — THE gate that would have caught tonight's phantom build. It
  existed and correctly flagged OLD PRXes missing markers earlier tonight;
  the lfb upload went out without re-running it on the final artifact.
  HARD RULE going forward: no upload without a fresh verify_build.sh PASS
  receipt for that exact sha.

## Tonight's full lineage (for context)

- 11:32 stale-113244 (10b11e437f): EI bind + EB + EC + variant C + Z15.
- 15:36 session: Phase 4c ToString bind REFUSED (prologue wide-leak /
  known_fault paradox — fault ledger reported TRUE with zero recorded
  probes; EO's candidate mechanism: ledger saturation on invalid range),
  then SILENT init abort after [L15] → uninstall. REGRESSION vs 5ea2c845.
- 17:38 etfix (EM's refusal fix) + 19:24 seamfix (EV helpers) + 21:12 lfb
  (EY bridge) — all three rounds raced parallel edits; seamfix/lfb Lua
  paths did not compose (evidence above).
- NB control (agent EO, patches ready, NOT built): MODBRIDGE_L27_NO_
  TOSTRING_BIND single-delta test of the silent-abort suspect #1. Apply/
  build procedure: outputs/2026-10-08-l26-runtime/parallel-candidate/
  05-apply-and-build-procedure.md. Signature table in 04-*.md.
- Corrective action in flight NOW: CHANGES rows re-synced to disk →
  G2 green plan → full upload-grade rebuild → verify_build.sh hard gate
  on [LUA-FN] presence → then (only) prep+upload for the next authorized
  launch. zzdiag v8 stays resident (auto-probe); v9 available if wanted.

## Questions for Claude (real ones, post-solve)

Q1. The seam: given the wide-libc root cause (prebuilt libc++ 4-byte
    wchar out-of-line members vs -fshort-wchar TUs), is the bridge
    approach sufficient, or does FString/FText PARAMETER marshaling
    (addendum directive) need the same treatment before the mod can work?
Q2. Pass/fail oracle for the next launch: [LUA-FN] present + indices
    full-length = seam fixed at FName layer. If [LUA-FN] fires but
    FindFirstOf still returns nil, what's the next discriminator —
    GATE-1 pool admission (FNAME_Find misses) vs index-vs-name compare?
Q3. Tonight's 15:36 known_fault paradox (fault ledger TRUE, zero probes
    recorded): EO suspects CopyinSink saturation-on-invalid-range. Worth
    fixing before or after the seam round?
Q4. Build discipline: any additional cheap gates you'd add beyond
    verify_build.sh (marker grep) + G2 row pins to prevent the
    edit-race class? (Incremental is not upload-grade; we know.)

## Key files
- This dir: CLAUDE-SUMMARY-LFB-DEAD-CODE.md (pre-solve, superseded by
  this doc), CLAUDE-SUMMARY-*-20261009*.md (earlier rounds), STATE-L26-
  WAVE.md (comprehensive state), agent-EY/EV/EO/EZ/EI/EM/EQ reports.
- Build lineage: outputs/2026-10-08-l26-build-variants/l26-A.{seamfix-
  190412,lfb-203415}/ (SOURCE-SELECTION.json = the pins that proved the
  race), outputs/2026-10-08-l26-package-A.{seamfix-192411,lfb-211254}/.
- Tree (has the routing, sha 9fea2963): outputs/2026-10-08-l26-runtime/
  typescan-r4/candidate/{src/LuaMod.cpp,include/LuaFNameBridge.hpp}.
