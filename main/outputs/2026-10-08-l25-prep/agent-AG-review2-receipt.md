# Agent AG — review-2 receipt + cover-doc update (2026-10-08)

Scope: record Claude's SECOND l25 review (post-Z11) as a durable receipt and
update the packet cover document in place. No other files touched; no
engine/source/console work.

## Files

1. WRITTEN — `claude-review2-20261008-l25.md` (new receipt, this
   directory). Faithful record of review 2 with all technical specifics:
   Z11 acceptance (mechanism = game allocator's own fatal check via
   AddUObjectDeleteListener → TArray::Append → FMemory::Realloc →
   GMalloc+0x40), the refinement (+0x10 = fatal logging path /
   FOutputDevice::Serialize-style; allocator REJECTED our Realloc, e.g. as
   an unrecognized block; NOT a wrong slot; slot-4 GetAllocatedSize fix
   stays), directives 1–4 (offsets 0x78/0x68/critical-section from engine
   code in the authenticated image; GMalloc variable 0x970add8 + vptr via
   GetDescriptiveName; all-growth audit between :528 and mod start under
   the B switch; A pre-Append logging Data/Num/Max + refuse-clean), B-first
   launch order (menu 90 s → save → 30 s → L3+R3; if menu opens: harvest,
   D-pad + one spawn same session, harvest again; A second with item-4
   logging), variant C (engine's own AddUObjectDeleteListener, locked,
   engine allocator; profile fix if item 1 proves wrong; packet + Claude +
   user OK; generic). Includes the dispatch mapping AA/AB/AC/AD/AE, marked
   as coordinator assignment, not review text.
2. EDITED (in place, not rewritten) —
   `packet/REVIEW-FOR-CLAUDE-l25.md`:
   - Intro: UPDATE pointer to review 2 + §5b, supersession rule.
   - §2 in-flight paragraph: Z10/Z11 report files now landed; Z11 accepted
     by review 2; Z10 verdict not summarized in the cover doc.
   - §3 story point 5 tail: "PENDING Z11" → Z11 landed, chain named,
     accepted by review 2.
   - NEW §5b "REVIEW 2 ADDENDUM": refined crash interpretation, items 1–4
     with agent IDs (AA offset-verify, AB gmalloc-audit, AC
     container-audit, AD item4-logging, AE variant-c design), B-first
     launch order, variant C.
   - §6 DECISIONS REQUESTED: (c) annotated SUPERSEDED by review 2
     (B first, A second; all other discipline unchanged); added (d)
     variant C adoption timing (after B result, packet + Claude + user OK)
     and (e) profile-offset correction conditional on AA.
   - §7 INDEX: claude-review2-20261008-l25.md added to the claude-review
     section.
   - §8 HONESTY BOX: former "Z10/Z11 PENDING" bullet updated — Z11 named
     the functions and was accepted/refined by review 2; Z10 landed but
     not summarized here; the older confounder bullet retained as
     review-1 context.

## Notes

- No claims beyond the relayed review text were added; every addition cites
  the new receipt or the landed agent files (verified present on disk).
- Review-1 safety invariants carried forward unchanged in the receipt.
