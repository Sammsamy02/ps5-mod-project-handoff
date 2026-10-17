# Claude review 2 directive receipt — l25 (2026-10-08, post-Z11)

Provenance: faithful record of Claude's SECOND review of the l25 report —
i.e. the review issued after Z11's stack symbolization
(`agent-Z11-stack-symbolize.md`, this directory) landed — relayed by the
coordinating session. The original review text is not stored on disk here;
this receipt is the durable local record. Technical specifics are preserved
verbatim where they are load-bearing. Companion receipt:
`claude-review-20261008-l25.md` (review 1). Where the two reviews conflict,
review 2 supersedes — most importantly the LAUNCH ORDER (§4 below: B first,
A second; review 1 said A first). All review-1 safety invariants
(fake-sign + decrypt-OFF readback verify, one launch per explicit user OK,
no rapid-fire launches, 1947 B settings law, kstuffSharp quarantine, ploader
rules) carry forward unchanged.

## 1. ACCEPTED — Z11's symbolized stack settles the mechanism

Claude reviewed the l25 report and ACCEPTED Z11's symbolized stack as
settling the mechanism: **the crash is the game allocator's OWN fatal
check**, reached through the chain

`AddUObjectDeleteListener` → `TArray::Append` → `FMemory::Realloc` →
`GMalloc+0x40`

(the game's allocator code, entered from the modbridge UE4SS init thread's
registration call; per Z11 all six abort blocks are one identical
signature).

## 2. REFINEMENT — crash interpretation refined

- The in-allocator **+0x10 dispatch is most likely the engine's fatal
  LOGGING path** — an `FOutputDevice::Serialize`-style call made before
  abort. I.e., **the allocator REJECTED our Realloc** (for example, as an
  unrecognized block) — **NOT a wrong slot inside the allocator**.
- The slot-4 `GetAllocatedSize` fix **STAYS** (correct generic hygiene).

## 3. Directives — items 1–4

1. **Verify `FUObjectArray` listener offsets — 0x78 delete / 0x68 create /
   critical-section location — from ENGINE CODE in the authenticated
   image.** A wrong 0x78 means a non-heap Data pointer. Track B flagged
   `FUObjectArray` as platform-dependent.
2. **GMalloc**: confirm the call-time VARIABLE read (`0x970add8`) — not a
   cached instance; identify the instance class from its vptr via
   `GetDescriptiveName`; log both at Append.
3. **Audit ALL engine-container growths between `:528` and mod start.**
   Variant B skips only the delete listener — another growth would crash
   one line later. Gate/skip under the same B switch.
4. **Extend A's pre-Append logging**: Data/Num/Max, growth-required,
   GMalloc variable + instance vptr; refuse cleanly if growth required +
   header not verified sane.

Dispatch mapping (coordinator assignment, not review text): item 1 →
agent AA (offset-verify); item 2 → agent AB (gmalloc-audit); item 3 →
agent AC (container-audit); item 4 → agent AD (item4-logging); variant C
design (§5) → agent AE (variant-c design).

## 4. LAUNCH ORDER — B FIRST (supersedes review 1's A-first)

- **B FIRST** — path to first mod-start / pump / pad / menu look:
  menu 90 s → load save → 30 s → L3+R3. **If the menu opens: harvest,
  D-pad + one spawn in the SAME session, harvest again.**
- **A SECOND** — mechanism confirmation with the item-4 logging.

## 5. VARIANT C — the proper fix (after B)

- **Stop growing the engine array ourselves — call the engine's OWN
  `AddUObjectDeleteListener`** (locked, engine allocator); the address is
  found like the other bindings.
- **Fix profile values if item 1 proves them wrong.**
- Adoption requires **packet + Claude + user OK**; all changes generic.
