# R4 native owner-variant correction — local candidate

Status: Mac-only implementation and focused target compilation; independent
review in progress. No runtime package, upload, console contact or game launch.
The frozen R3 runtime and original Nexus mod are unchanged.

## Reproduced behavior

The actual selected FFieldVariant class and selected methods were extracted
without implementation rewrites into a host translation unit. Only engine
storage, engine-version services and UObject outer lookup are replaced.
Literal expectations follow the separately authenticated native evidence:
FField.Owner is eight bytes at +0x10, its low bit identifies UObject, and the
tag must be cleared before dereferencing the object. The selected port's local
wrapper remains16 bytes to retain pre5.3 representation.

The original source failed object construction and mutation, overwrote the low
byte of the adjacent Next pointer, and dereferenced a tagged pointer. Fatal
ASan also detected its16-byte copy from an8-byte native owner allocation.
RED-RECEIPT.json preserves13 failing command results (seven distinct cases).
This is an executed regression, not an inference from package structure.

## Candidate changes

- UObject constructors and Set encode the tag for5.3+; null remains invalid.
- Legacy Set changes its bool only for versions below5.3, never at native
  owner+8. An independent review exposed that the old4.25 lower guard also
  broke4.24 local-wrapper mutation; the candidate now covers that branch.
- GetFFieldOwnerUnsafe copies the version-selected native size, eight bytes
  for5.3+ and16 for older FField variants, into a local initialized wrapper.
- Owner traversal clears the UObject tag before calling its method.
- Null traversal returns an invalid owner, and GetOutermostOwner exits when
  the chain ends instead of dereferencing a null field.

The pre-review candidate is preserved separately. REVIEW_RED-RECEIPT.json
records eight failing command results for four follow-up cases before their
repair. All original-source pins still match SELECTED-SOURCE-PINS.json.

## Verification and limits

GREEN-RECEIPT.json:25 cases run in both plain and fatal ASan/UBSan modes,
50 case executions plus two compiles, all successful. Tests cover exact8-byte
reads, adjacent-pointer sentinels, real SetOwnerVariant wrapper calls, null
traversal,5.2 legacy storage,5.3 transition and4.24 local variants.
These are actual method bodies; they do not execute the real engine or prove
the below4.25 UField bridge on hardware.

TARGET-RECEIPT.json: actual FField.cpp and FFieldPathProperty.cpp compile with
the pinned PS5 SDK/compiler and candidate header. Both object undefined-symbol
sets are unchanged versus their frozen selected objects. This is focused
target compilation, not a linked/package import-census claim. An inline header
changed; every affected consumer must be rebuilt before full integration.

The fix does not establish a native ABI for every engine function accepting or
returning FFieldVariant, prove object liveness, authorize native data writes,
or resolve FField/FFieldClass exact totals. Global engine-version selection
must remain stable for the lifetime of these representations. Original PC Lua
mods remain byte-identical. No pad, profile-total, code-read or XOM-policy change
is part of this candidate.

Files: candidate/FField.hpp, candidate/FField.cpp; original-to-candidate patches
and before-review source snapshots are alongside this report.
