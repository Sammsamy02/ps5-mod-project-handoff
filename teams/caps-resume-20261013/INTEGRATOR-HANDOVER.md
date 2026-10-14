# INTEGRATOR HANDOVER — for Claude (2026-10-13, agent KC; updated 2026-10-13 by root after KG/KD/KH landed and the #1 LSTAR v1 fix was built)

Everything below is staged and hashed. Mac-only: nothing has been applied to
the integrator's main tree, nothing has touched the PS5. Absolute prefixes:

- `WT`  = `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013`
- `R5`  = `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep`

## 1. Exact staged files and SHA256

### A. The authoritative #7+#10 combined stage (integrate from THIS, not the older per-capability patches)

- Stage root: `WT/caps-team-work/cap10-integration-ready/derived/target-ready/`
- Master patch: `WT/caps-team-work/cap10-integration-ready/derived/target-ready/integration.patch`
  SHA256 `59e25e4b3ba0c813d79147851ac2a174e92fb47eb6f75237ed660891bd5765ac` (1,667,953 bytes)
- Per-file hashes for EVERYTHING in the stage (tools, metadata, mono-headers,
  runtime assets) are recorded in `WT/caps-team-work/cap10-integration-ready/HANDOFF-MANIFEST.json` —
  verify against it instead of trusting this list. Key binary inputs:
  - `cap10/lib/libmonosgen-2.0.a` — `24eebde5354ccf8ea3061d3a2bc47fd8eec48182934a91b304cd99b366d76a4f`
  - `cap10/lib/mscorlib.full-interp.ps5.o` — `a9f0a8564ac09739d03b2efd4d7f06fa54c003d24a060f72a7966811251840de`
  - `cap10/managed/Probe.dll` — `c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d`
  - `tools/cap10_integration.py` — `54040e1daef77bbeedda6e241a3d243fdfa12dd7f71fa07032b27a5697966583`
  - `tools/prepare_stage.py` — `4d3ceac12393d47ace522ca35f903394ac3e90f10388383328b8eedc916752fb`
  - `tools/finalize_handoff.py` — `cfd9fd9c9c3f8e2502e90111b6284466ab55ca679b1336ca8bd3132504f54bb7`
  - `tools/test_integration.py` — `f8c4b34e345f5e150fdfe1fa1fd2adb1c08abf469935a4118ceae85060cc8c03`
  - signal owner source `03edcc27dced6d2a1fa03162bbe3fbd74345e9d2207653688422a78d086c0bad`,
    header `8121c4ceb2c8bb6d3172c22abd7e969ecc0643ddd24cdcba1109ccf49bc169ed`
    (owner/runtime pair must be supplied together with `--owner-dir` / `--runtime-archive`;
    binding recorded in `INTEGRATION-INPUTS.json`).
- Stage verification (Mac, no compiler, no driver main):
  ```
  PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools -p 'test_*.py'
  PYTHONDONTWRITEBYTECODE=1 python3 tools/finalize_handoff.py
  ```
  (run inside `WT/caps-team-work/cap10-integration-ready/`)

### B. #7 step2 provenance (superseded as an integration source by A, kept for traceability)

- `WT/caps-team-work/cap07-step2/derived/review-final/integration.patch`
  SHA256 `9239ed7f9251eb9d71a436a92365aef9e21f15abe0c3dfad34fdc108f8f0897d`
  (the reviewed 15-file patch; refreshed #7 metadata is already folded into stage A.
  Claude's later typescan metadata change is captured, not overwritten.)

### C. round5 runtime fixes (MAIN TREE area, staged, NOT applied, pins stale)

All under `R5/round5-staging/`:

| Patch | SHA256 |
|---|---|
| `lookup-speed/01-foreach-chunk-bounds.patch` | `8945c7890cf924e2d9287d74221b292a1037f4125b08d7aa840dd5f01fae7987` |
| `lookup-speed/02-staticfindobject-name-first.patch` | `c81f486f69f1075dced18370c6c0013ca8d0292e4d82f154bd38da81ed99d230` |
| `lookup-speed/03-findxof-walk-memo.patch` | `281f40cb40de6f060d758907d0a91ab4742da87ba18fbdacdcd346a0e1dcea79` |
| `lookup-speed/04-findxof-name-cache-classlistener.patch` | `9932c4b4d4b61807545aa099969b3bfe61bc8790b2dc112bb93bec350dacedfa` |
| `lookup-speed/04-findxof-name-cache-unrealinitializer.patch` | `412f787b07d243413253476e8e17cb170641bd83c43a6a8e09119e244595dea2` |
| `lookup-speed/04-findxof-name-cache-uobjectglobals.patch` | `b79a11f91328f33f2b9e4e1dd22c38d69d5e63f767a8e8c46ac923fd7519dce4` |
| `group-b/01-luamod-foreachuobject-fix.patch` | `af71868c842b1dbe40570d2d85ac4eb948c9d0ed157d59860df7618ccd80c2ad` |
| `group-b/02-luamod-iterategamedirectories-ps5root.patch` | `fbadfc5c9199e3ccd6c48b297743c24f79cd334fefd373ddab0cd068bcabeaca` |
| `group-b/03-luauenum-foreachname-diagnostics.patch` | `3b44a90c77c31d413f247d4f34f87d4fd144ef53d4fb472031ae170e5ca91315` |
| `shim-fix/01-luapathshim-writearea-fix.patch` | `ae8f23d3438ebe447fd9247b8bd269991e9e79b045cf5336fcf2f184b3d4956a` |
| `lua-alloc/patches/0001-lua-alloc-diagnostic.patch` | `f1175f8a8583e27a876d26c1ac3119bf22e8b05fcb8ba027747a42a18906c1ff` |
| `lua-alloc/patches/0002-lua-arena-enable.md` (env-gate DOC, not a code patch) | `89f589fc73d9a43f0d8b6cd78ffbe01226df53b5e835b19947585c714d1ec035` |
| `lua-alloc/patches/0003-ueselftest-mem-hygiene.patch` | `1d7e475501cc6b411955f2ee142a088f40fc1bfefbafae38cc2081e05f685ab2` |
| `R5/harvest/no_detour_gate.py` (build-time gate tool, run it — do not "apply" it) | `6048e0876b985e0f2f4495323da8ea4ba9846819a2247003f837099010a1039e` |

Round5 order/restore helpers: `R5/round5-staging/ROUND5/BUILD-CARD-ROUND5.md`,
`restore-round5.sh`, `plan-validate-round5.sh`. Unpatched pin hashes:
`lookup-speed/unpatched-sha256.txt`. WARNING (JD report): all round5 unpatched
pins are STALE against current tree bytes — re-anchor each patch before apply;
never force a stale patch with fuzz.

### D. #1 / #3 / #13 — proposal-only; NOT integration content

SHAs below were recomputed 2026-10-13 after KD's #3 bug fixes and the #1
LSTAR v1 fix; the two #3 source rows that previously listed `3a044c16…` /
`d46eaa53…` were STALE (pre-fix bytes) and are corrected here.

| Item | Path | SHA256 |
|---|---|---|
| #1 verdict | `WT/caps-team-work/cap01-03/PROVIDER-ROUTE-VERDICT-V2.md` | `27243cc398b0021f3751dbd49e94e8e278569e5289244b6f5ea8b9fe167cd973` |
| #1 one-answer | `WT/caps-team-work/cap01-03/KERNEL-READ-ONE-ANSWER.md` | `c290d6ac009182746b6401e2755aee76bde5ee3cc08a4a7c6521aec270d9396d` |
| #1 LSTAR diagnosis (probe bug; kstuff exonerated) | `WT/caps-team-work/cap01-03/probe/LSTAR-DIAGNOSIS.md` | `9db2571adf7d8aa598b325af1ffcae1904756cae22e18195a8bcd26bec8e20d7` |
| #1 probe v1 ELF (CF-flag fix; CURRENT console candidate) | `WT/caps-team-work/cap01-03/probe/cap01-kread-probe-v1.elf` | `2b89f99a9cd3f72f96dea8149854dbf47dec026d135df1b21a8f5238c88380c4` |
| #1 probe v1 source | `WT/caps-team-work/cap01-03/probe/KernelReadProbe-v1.c` | `40e22f47fbd096756278c15876163cd83ae261d208d55007ea21a4f78da841ea` |
| #1 probe v1 fix receipt (build + disassembly verification) | `WT/caps-team-work/cap01-03/probe/LSTAR-FIX-V1.md` | `2214aa3a96db8726fc9aaba74ffe604df7c2eb3f7fa7201ad7bf21379f4548b2` |
| #1 probe v0 ELF (provenance; console evidence 2026-10-13) | `WT/caps-team-work/cap01-03/probe/cap01-kread-probe-v0.elf` | `12ae76a08fd7afb2b6c0da06d6c51ccc34f9d9fc8726bd2fc7847a85c3192dcb` |
| Byepervisor HOLD | `WT/caps-team-work/byepervisor/ASSESSMENT.md` | `fcd871c050940326a418ce2ba918a5798286e483fb415f98443f5260dde52526` |
| #3 implementation | `WT/caps-team-work/cap03/src/CodePatch.hpp` | `6f43badea0846dd578ef36f29e8b0a17279b1fafee835539435cea3dbef3cf4d` |
| #3 implementation (KD-corrected 2026-10-13) | `WT/caps-team-work/cap03/src/CodePatch.cpp` | `4690de63eadc94b994f1cf361a374e3680040a354fd085802076f4faf40b4f07` |
| #3 diagnostic (KD-corrected 2026-10-13) | `WT/caps-team-work/cap03/src/PatchDiagnostic.cpp` | `a4dd0f5d53a598e1d10d22b27996adb6c76131e6edcab5da8316b1e70244819c` |
| #3 payload ELF (the ONLY tested build; 521,192 B) | `WT/caps-team-work/cap03/build/cap03-diagnostic.elf` | `35500cb7b901c283a8b06eae956e8c00cd0092279569ccc886dad3adaee550ae` |
| #3 package manifest (authoritative SHAs + flags + revisions) | `WT/caps-team-work/cap03/PACKAGE-MANIFEST.md` | `2bbc21e7840c6bc7c2a8643e03e5053d4bb37bd57f6f21bc6ca0a56708b8fed8` |
| #3 build receipt | `WT/caps-team-work/cap03/BUILD-RECEIPT.md` | `2ca33fed39ee482645bb8b38b4e303544788fe81ada27fcc71679d0ec7edc5da` |
| #3 single test card | `WT/caps-team-work/cap03/SINGLE-TEST-CARD.md` | `09e0f7e252fbd9ee7548d5e5c7f3bae8f616737d798cd9e48422733a46fede92` |
| #13 upstream-DLL path analysis (agent KH 2026-10-13) | `WT/caps-team-work/cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md` | `d72bc4ae972543c94e17014d24d4c7a374df9681fe7e29a53e892b85943784cb` |
| #13 native route patch (optional host routing; integrator decides) | `WT/caps-team-work/cap13/native-loader/patches/cppmod_pe_route.patch` | `ebce511b95a34c4d7f5c8a97438e855a13d591f899cae61ee1931106ad99b1a2` |

#13 one-paragraph state (from KH's report, supersedes earlier wording):
no genuine unchanged upstream UE4SS C++ DLL mod exists on this Mac — only
the framework itself (`UE4SS.dll`, sha256 `94c2ebd3…`, 4256 exports; a
loader stress input, NOT a mod) plus our own fixtures; every real
downloaded Townfall mod is Lua/pak/config, so no compatibility claim may
go beyond "dependency coverage." The decisive ABI gap is source-located:
the target mod's core call `?StaticConstructObject@UObjectGlobals@…`
(real export, ordinal 3562) is already implemented in the port runtime
(typescan-r4 `src/UObjectGlobals.cpp:146-197`, consumed at
`src/LuaMod.cpp:2949`) but `mbm_host_v1` has no construct row, and the
staged workaround `call_function(controller,"StaticConstructObject",…)`
deterministically returns `MBM_E_NOT_FOUND` (reflection-only resolution,
`NativeModEngineServices.inc:352-356`). Closing row (appended at table
end per the header's only-grow rule, `struct_size` bump, honest
state-change admission): `construct_object(ctx, mod, class_object,
outer_object, mbm_object* out)`. Mac PE-loader E2E (KF, in flight in
`cap13/e2e/`) precedes any console spend; J1 (executable anonymous memory
in the game process) remains the single console fact gating the native
route. Full analysis + difference table (reconstruction vs real
downloaded DLL) in the KH report above.

## 2. Console actions each capability needs

- **#7/#10 combined stage:** the real combined game build + package + launch on
  PS5 (Townfall first; #10 alone never holds Stellar Blade). #10 proof =
  interpreter startup, both owned exports, Run → exact managed exception →
  recovery → cleanup, in-game.
- **round5 fixes:** each item lands as its OWN labelled build, then a
  regression launch (existing game-launch ladder) + the item's specific check
  (lookup timing <10 ms, Lua slot stability, crash scenario gone, save/map-load
  soak with arena env-gate ON).
- **#3:** ONE `pldmgr /loadpayload cap03-diag.elf` run, no game running —
  see `WT/caps-team-work/cap03/SINGLE-TEST-CARD.md`. Use ONLY the packaged
  521,192 B payload (sha `35500cb7…`, PACKAGE-MANIFEST.md); the earlier
  129,344 B `-nostdlib` attempt (`cap03-diag-v0.elf`) is UNRESOLVED and is
  NOT this payload. USER OK + Phase B gate.
- **#1:** next authorized window: run `cap01-kread-probe-v1.elf`
  (sha `2b89f99a…`, procedure = TEST-CARD.md with the v1 filename/log path).
  Expected: all 4 checks PASS including LSTAR for the first time (v0's
  LSTAR FAIL was the probe's sign-test bug, not kstuff — LSTAR-DIAGNOSIS.md;
  fix verified in the v1 binary by disassembly — LSTAR-FIX-V1.md). v1 runs
  count fresh toward the 3-restart law (1 clean boot so far, on v0).
- **#13:** J1 probe (can game-process anonymous memory be mmap PROT_EXEC) is
  the single console fact that gates the native route — needs user OK; nothing
  else console-side until the Mac PE-loader E2E (KF, in flight) is green and
  #1-#12 are proven.

## 3. What can be applied WITHOUT console testing (Mac-side, still follow the gates)

1. Stage A composition into the build tree + stage verification commands
   (section 1A) — pure Mac work.
2. #10's one-line UE4SSProgram.cpp re-pin (`fe9fe7bc…` → current `ad3ae621…`)
   with a fresh derived sha — required BEFORE staging #10 (JD report).
3. round5 re-anchoring work (recompute pins vs current bytes) — Mac-side prep.
4. no_detour_gate.py wiring into the build pipeline (build-time static check).
5. cap03 real prospero-clang syntax/build of `cap03-diag.elf` — DONE
   (KD, 2026-10-13; 521,192 B, sha `35500cb7…`, zero errors/warnings,
   host test 98/98 — PACKAGE-MANIFEST.md). Remaining Mac-side: none for #3;
   the payload waits on the console window.
6. #1 probe v1 (LSTAR CF-flag fix) — DONE (2026-10-13): source
   `KernelReadProbe-v1.c`, built clean via the probe Makefile (prospero
   clang, `-Wall -Werror -O2 -g`), sha `2b89f99a…`; disassembly-verified.
   v0 source/ELF/evidence preserved byte-exact.
7. #13 `mbm_host_v1` `construct_object` row (KH's one-row fix) — Mac-side
   implementation is possible now (existing dispatcher +
   NativeModEngineServices.inc override pattern), but it is a host ABI
   change owned by the #9 team; integrator decides timing and the v1.1
   review. Do NOT rely on the staged
   `port_cheat_manager_enabler.cpp` call_function workaround — it
   deterministically fails (KH §3.5).

Everything that RUNS on the PS5 needs the user's OK for that step. The 3-restart
law applies to anything claimed as console-verified.

## 4. Build order (JD composition order, unchanged)

```
S0  Pins + snapshot of the current tree (verify #7 recipe pins bind exactly;
    snapshot for bisect)
S1  #7 read-only stage  (use stage A; current pins bind exactly per JD)
S2  #7 labelled build + package          <- no-detour gate + fs_throw_gate run here
S3  #10 one-line re-pin, then labelled build + package (fresh derived shas)
S4  round5 items, EACH as its own labelled build, re-anchored, in this
    order: lookup-speed -> GROUP B -> shim-fix -> lua-alloc (world-fix and
    the ROUND5 restore script prerequisites per BUILD-CARD-ROUND5.md if
    rebuilding from the round-4 base)
S5  #1/#3: NOTHING (selection stays empty; a fail-closed gate enforces it)
```

Notes:
- #7 and #10 are logically independent; serialization is per-file-chain plus
  the build lock (JD). Full builds only — no `--incremental` (the
  CodeImageCache kMaxPins 8→16 ABI hold requires the full header-consumer
  rebuild; #10's stage already carries the 16-slot table untouched).
- `fs_throw_gate` will flag #7's producer header (PakRequestProducer.hpp):
  use error_code filesystem forms or visible approved markers before --execute.
- Independent runtime review slots sit between each component's composition
  and its --execute.

## 5. What NOT to apply (blocked)

- **#1 kernel reads:** nothing to integrate into the game runtime — by
  design. The v1 probe is a standalone pldmgr payload, not runtime content.
  No kstuff modification, no new kernel code. Its console load is
  user-OK-gated per run; Phase B (KREAD sub-op) stays gated on the Townfall
  Item Spawner spawning + per-step user OK (`KERNEL-READ-ONE-ANSWER.md`).
- **Byepervisor Reborn:** HOLD — do not load, do not build (1-day-old repo,
  open 13.60 crash reports; `byepervisor/ASSESSMENT.md`).
- **#3 sources:** do NOT build into any game PRX or run the diagnostic without
  the user's explicit OK and the Phase B gate; the diagnostic is payload-only,
  no-game, one run per boot (`SINGLE-TEST-CARD.md`).
- **#13 native-loader and managed-loader sources:** staged, unbuilt, M0 tests
  pending; the Win64 bridge asm is UNVERIFIED. Do not compile into the runtime.
  The Mac PE-loader E2E (KF) precedes everything; the J1 probe answer gates
  the whole native route. Also: the staged #9
  `port_cheat_manager_enabler.cpp` workaround calls
  `call_function(controller,"StaticConstructObject",…)` which deterministically
  returns `MBM_E_NOT_FOUND` — do not treat it as the fix; the fix is the
  `mbm_host_v1` `construct_object` row (§1D).
- **`IP-shim-root/`:** explicitly forbidden by the JD runbook.
- **`KernelBridgeProbes.{cpp,hpp}`:** stays SET ASIDE; the restore refuses if
  found in the tree (Claude-reviewed gated build only).
- **Stale round5 pins:** never apply with fuzz/force; re-anchor first.
- **lua-alloc arena:** stays env-gated; do not enable unconditionally.
- **SceNet gap:** the seven unresolved `sceNet*` provider identities mean the
  combined build may stop at the provider gate — that is fail-closed working
  as designed (PROVIDER-READINESS.json is a component diagnostic, not a
  final import count). Supply genuine local identity evidence if they remain
  unresolved after the real combined build.

## 6. STALE INTEGRATION PINS (consolidated register — re-anchor before use)

Every pin below is known-stale against current tree bytes. Never apply with
fuzz/force; recompute anchors first.

1. **round5 staged patches (all 13)** — unpatched pins in
   `R5/round5-staging/lookup-speed/unpatched-sha256.txt` do not match current
   tree bytes (JD). Re-anchor each patch; see §1C.
2. **#10 UE4SSProgram.cpp re-pin** — `fe9fe7bc…` → current `ad3ae621…`,
   fresh derived sha required BEFORE staging #10 (JD; §3 item 2).
3. **KB's LuaMod.cpp citation drift** — KB's #13 feasibility cites
   `LuaMod.cpp:2303` from an earlier revision; in the current typescan-r4
   tree the StaticConstructObject Lua registration/call sits at
   `:2844`/`:2949` (KH re-located 2026-10-13). Same function, drifted lines —
   re-locate before citing.
4. **`UE4SS.dll.analysis.json` RVA fields** — the `rva` entries repeat one
   stale value across symbols; presence + ordinals are correct, but take
   RVAs from a direct export-directory parse instead (KH §6.1; verified
   values: StaticConstructObject ord 3562 RVA `0x1224524`, params ctor ord
   120 RVA `0x10c526`, FindFirstOf ord 1149 RVA `0x125d8a`).
5. **cap03 foreign artifacts** — `CodePatch.o` (39,144 B), older
   `PatchDiagnostic.o` (11,392 B) and `cap03-diag-v0.elf` at the cap03 ROOT
   are NOT the package (different bytes, unproven); only
   `cap03/build/ps5-obj/*` + `cap03/build/cap03-diagnostic.elf` are
   authoritative (PACKAGE-MANIFEST.md §6).

## 7. PRECISE NEXT CONSOLE TESTS (each needs the user's OK for THAT action)

Ordered by value per launch; one launch per test; PS5 currently unavailable —
this list is for the next authorized window.

1. **#1 probe v1 — LSTAR confirm (highest value, lowest risk).**
   Upload `cap01-kread-probe-v1.elf` (verify sha `2b89f99a…` by readback),
   `pldmgr /loadpayload`, read `/data/cap01-kread-probe-v1.log`.
   PASS criterion: `check=RDMSR_LSTAR value=0xffff… canonical_kernel=yes
   verdict=PASS` + the three v0-green checks still green + `overall=PASS
   exit=0`. Any FAIL line now self-documents (`rc=`/`cf=`). Counts as pass
   2 of 3 for the restart law (v0's clean boot was pass 1).
2. **#3 diagnostic — the ONLY tested #3 payload.**
   `cap03-diagnostic.elf` (521,192 B, sha `35500cb7…`), no game running,
   single run per boot, procedure + abort criteria verbatim in
   `SINGLE-TEST-CARD.md`. USER OK + Phase B gate. Do not run the v0 ELF.
3. **#13 J1 probe — only after Mac E2E green.** One mmap PROT_EXEC test in
   the game process; gates the entire native DLL route. Needs its own test
   card before the window opens (currently design-grade).
4. **round5 regression launches — only after re-anchoring + labelled
   builds** (§2 round5 bullet; per-item checklists in BUILD-CARD-ROUND5.md).

## 8. One-paragraph summary for Claude

Integrate from `WT/caps-team-work/cap10-integration-ready/derived/target-ready/`
(hash-checked via its HANDOFF-MANIFEST.json): re-pin #10's UE4SSProgram.cpp,
run the stage's two verification commands, then follow JD's S0-S4 order — #7
stage, #7 labelled build, #10 labelled build, then each round5 fix as its own
re-anchored labelled build with no_detour_gate in the pipeline (stale-pin
register: §6). Nothing touches the PS5 without the user's OK; combined
game/package/console tests are yours. #1's LSTAR failure was the probe's own
sign-test bug — v1 is built, disassembly-verified, and ready for the next
window (§7 item 1); #3's only tested payload is the 521,192 B
`cap03-diagnostic.elf` (sha `35500cb7…`, test card §7 item 2); #13 has no
upstream DLL on this Mac to run — its decisive gap is the one
`mbm_host_v1` `construct_object` row (§1D), Mac E2E (KF) in flight, J1
console-gated after that.
