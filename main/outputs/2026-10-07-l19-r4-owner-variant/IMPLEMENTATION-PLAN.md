# Native owner-variant runtime correction

Goal: preserve the native tagged 8-byte UE5.3+ owner representation in the
runtime's existing version-aware FFieldVariant API, without editing mods or
guessing class totals. This work is local only and does not authorize deployment.

Source: frozen selected FField.hpp/FField.cpp in the coherent runtime.
Evidence: authenticated REFLECTION-FACTS.json describes an 8-byte owner at
FField+0x10, bit0 set for UObject, clear for FField, masked before dereference.
Frozen l19 R3 sources and receipts stay unchanged. Candidate sources live here.

Review focus: construction and Set must agree; native field writes must preserve
the adjacent Next pointer; native reads must not consume that pointer; traversal
must mask tags; legacy pre5.3 owner behavior must remain intact; null owners must
remain invalid; engine-version decisions must not be supplied by a test only.

- [ ] Extract actual selected class/method bodies into a host compilation with
  only engine/version/offset storage doubles. Literal fixture expectations come
  from native byte evidence, not a duplicate adapter.
- [ ] Capture RED failures for object construction, adjacent-pointer preservation,
  bounded 8-byte native reads and tagged traversal.
- [ ] Copy the selected source/header to this candidate directory; repair the
  version-dependent representation and exact-width copy only.
- [ ] Run all cases plain and fatal ASan/UBSan, including legacy branches.
- [ ] Compile both affected units with the pinned PS5 toolchain and actual
  headers; record source/evidence pins and compare imports.
- [ ] Obtain independent review before any integration into a new full build.

This is a source port fix, not a native ABI proof for all FFieldVariant-taking
engine calls. FField/FFieldClass totals and LoadMap/InitGameState slots remain
separate unknowns until independently established.
