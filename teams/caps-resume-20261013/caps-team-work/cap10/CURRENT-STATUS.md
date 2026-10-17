# #10 current Mac result — 2026-10-13

The minimal C# Probe preparation now has a genuine target Mono static archive, matching static ELF corlib, compiled production owner/boundary and a successful target-format link. The actual updated production owner passes the full Run → deliberate exception → recovery flow on the Mac with static corlib. Claude still owns the combined mod-loader build/package and actual PS5 test. No target execution has occurred.

Start with **MAC-INTEGRATION-HANDOFF.md**. REVIEW-CLOSURE.md records the reviewer scope. Earlier JB and host-only status reports remain historical evidence; the former “target Mono port does not exist” status has been superseded by the real compile/link results.

| Work | Current evidence |
|---|---|
| Genuine Probe.dll | 3,584 bytes, SHA256 c328dca11447b2c41ab1580582b9576911b0291a4d14d89a09a708e69e1c108d; exact compiled image remains embedded |
| Current owner | SHA256 a73847833fd1621a0304641e5f6add267787223474bea0d95b4c9d0848e4f732; target static registration now occurs before initialization |
| Actual production static host run | ../cap10-static-corlib-host/evidence/PRODUCTION-STATIC-OWNER-CHECK.json: stage7, two managed runs, one exact exception, recovery, both native imports and clean cleanup; no corlib dylib used |
| Strict loader | ../cap10-mono-build/evidence/LOADER-PATCHED.json: all seven actual-runtime cases pass; genuine unpatched baseline reproduces the defects |
| Native target runtime | ../cap10-port-closure/: complete native core build, genuine aggregate Mono archive and link-only ELF; no fabricated runtime/API stubs |
| Target static corlib | ../cap10-cross-aot/: actual cross-AOT ELF object, exact matching IL, all 369 final target layout constants match |
| Exact owner target link | ../cap10-port-closure/evidence/TARGET-OWNER-LINK.json: actual production sources compile with warnings as errors and link to the genuine archive/static corlib; fixture was never executed |
| Main source patch | native/current-owner.patch: three exact anchors; current source input SHA256 44499a0aaeb617c80f9cee3ad62222cdffe9f45b4b71cc663fb25cce616150f1; applies/reverses exactly in owned storage, main untouched |
| Minimal native dependencies | ../cap10-native-deps/: real host flow does not load System.Native; complete Mono.Native is outside this minimal test's demonstrated needs |

The public linker emits ET_DYN/PIE link artifacts with entry zero. These standalone link results are evidence, not deployable executables or PRXs. Target runtime GC/stack/TLS/signal/context and AOT transition behavior remains for the real game test. Existing process/module diagnostic fallbacks remain unsupported/unproved on target. The full Unity/mod-compatibility proof remains later.

The source-built runtime and cross-AOT reports preserve exact pinned-source provenance, patches, real commands, failed attempts and reproducible recipes. The successful static corlib route avoids relying on a separately loaded corlib native image. The configured target headers and matching managed IL remain required.

No PS5 contact, main/candidate/frozen-build write, game PRX build/deployment, or #1/#3 kernel work occurred. Do not hold ready capabilities or Stellar Blade for #10.
