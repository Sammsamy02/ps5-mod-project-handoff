# parked/NOTES-REGISTER.md — known-issues register (l25-prep wave)

One page per parked issue. Recorded, not fixed. Entries are appended,
never rewritten (raw log provenance preserved).

---

## KI-1: [EU-VMT] GameEngine vtable Tick pump arming refused — "non-text-entry-in-table"

- Recorded: 2026-10-10, agent-GY (carrier-drive task item (d)).
- Status: DEFERRED until AFTER the spawn launch, per Claude (explicit
  instruction in the GY tasking: do NOT fix before the spawn launch).
  Mod behavior is NOT blocked: the R3 FuncPtr-marshal event carriers
  are the live game-thread pump (see ../CARRIER-DRIVE.md).

### Exact evidence lines

Console session log (NUL-interleaved wide writer; grep -a required):
outputs/2026-10-08-l25-prep/evidence/claude2-menu-session-20261010.log

- Line 625: `[2026-10-10 00:53:06] [EU-VMT] arming refused: n^@o^@n^@-^@t^@e^@x^@t^@-^@e^@n^@t (R3 carriers remain the pump)`
  (raw bytes: the narrow reason string widened unit-by-unit by the
  broken PS5 short-wchar widen, and TRUNCATED — the full literal is
  `non-text-entry-in-table`.)
- Line 660: `[2026-10-10 00:53:08] [ST] pump vmt REFUSED (ufunc line pending first tick)`
  (the [ST] selftest echoing pump state; the FIX5 ufunc line never ran
  because the pump never armed.)

### What the refusal actually means (from code, NOT an "entitlement")

Suspected meaning in the tasking ("non-text entitlement segment?") is a
misparse of the truncated widen: **"ent" is the head of "entry"**, from
`VmtTickPump::FailReason::NonTextEntryInTable`
(src/VmtTickPumpCore.inc:79,97,285-293; src/UnrealInitializer.cpp
VmtPump::vmt_pump_arm at ~2311-2315 logs it). It has nothing to do with
PlayStation entitlements. The arming law is:

1. The GameEngine instance's vtable is copied from (vptr-0x10) through
   the LAST word in the pinned executable TEXT segment (bounded 512-entry
   scan; extent = highest in-text index + 1).
2. Every NON-NULL word INSIDE that extent must itself classify as text:
   `in_text(word)` = `rc_seed::classify_seed_address(word,
   SeedArming::module_view_base()) == CodeCallOnly`
   (UnrealInitializer.cpp:2179-2191) — i.e. a code pointer inside the
   authenticated MainExe LOAD/TEXT of the live module view (MainExe base
   0x400000, size 0xbc602c8 in this session, log ~line 770).
3. `NonTextEntryInTable` = at least one non-null vtable word inside the
   extent FAILED that class. The arm refused loudly, published nothing,
   and left the R3 carriers as the only game-thread pump.

### Suspected concrete causes (unverified — needs one readback probe when reworked)

- A UEngine-family virtual may resolve to a thunk/stub OUTSIDE the
  MainExe text view (shared-module import, PLT-style veneer, or a
  different LOAD segment), so classify != CodeCallOnly.
- The SELF-decrypted live image may place some vtable targets in a
  segment the pinned seed classifier does not grant CodeCallOnly even
  though executable (classifier was built for seed admission, not
  vtable walking).
- Rounding/extent effect: a trailing in-text-looking word extending the
  extent over words that are legitimately non-code (padding/data),
  which the law then correctly refuses.

### Rework sketch (when unparked, after spawn launch)

- One diagnostic: log the failing word's value + relative offset +
  classify class (data-only reads) — decides between the three causes.
- Candidate fixes: extend the classifier used ONLY for vtable words to
  "any PF_X segment of the authenticated image(s)", or clip the copy
  extent to the last entry before the first non-text word IF the Tick
  slot is still inside (needs a law change: current law refuses rather
  than truncates — VmtTickPumpCore.inc:154-161 comment).
- Host fixture: tests/actual_eu_vmt_pump.cpp already covers the core
  mechanics; a new case can encode the observed table shape.

### Impact while parked

None on current acceptance: the R3 event carriers (UserWidget:Tick /
Actor:ReceiveTick / AnimInstance:BlueprintUpdateAnimation override
UFunctions) demonstrably pump the game-thread queue at sub-second
latency in-world (CARRIER-DRIVE.md (b)). The VMT pump is the would-be
PRIMARY (genuine UEngine::Tick, state-independent); its absence is the
main-menu blind spot: no game-thread dispatch for the whole frontend
session (first fire only at world load, 00:54:02).

---

## KI-2: logwriter dual-path NUL-interleaving — clean-UTF-8 proposal (PROPOSAL ONLY)

- Recorded: 2026-10-08, agent GZ (log-decode task item 4).
- Status: PARKED — labelled-change proposal, nothing implemented.
  Consumer-side mitigation landed the same wave: decode-first tooling
  (harvest/decode_session_log.py + harvest.py/bind_launch.sh/fast_cycle.sh
  wiring; see ../LOG-REDECODE-VERDICTS.md).
- What: UE4SS.log is written by a dual-path writer (narrow UTF-8 +
  wide-doubled byte,NUL runs from the vendored codecvt unit-doubling;
  race interleaves messages mid-line). 9 retained verdicts changed under
  decode-first re-analysis; the plain-grep miss class is documented in
  LOG-REDECODE-VERDICTS.md.
- Proposal + console-verification plan:
  parked/KI-2-logwriter-clean-utf8/PROPOSAL.md (F4a LuaLibrary explicit
  codec + F4b sink-side NUL strip inside the F1/F3 device lock, with a
  one-shot `[ST] logwriter nul-strip active` receipt line).
