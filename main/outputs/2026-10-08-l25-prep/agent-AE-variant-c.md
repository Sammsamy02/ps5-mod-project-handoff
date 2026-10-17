# Agent AE — Variant C design (Claude's proper fix): call the engine's own AddUObjectDeleteListener

Date: 2026-10-08. READ-ONLY analysis + review-ready patch draft. No file
outside the two deliverable locations was modified; typescan-r4, CHANGES.json
and build scripts untouched.

## Deliverables

| Artifact | Path |
|---|---|
| Patch draft (unified diff, additive-only, 8 hunks) | `outputs/2026-10-08-l25-runtime/variant-c-proposal/variant-c-patch.diff` |
| Decision memo (feasibility/fallback/risks/dependencies) | `outputs/2026-10-08-l25-runtime/variant-c-proposal/DECISION-MEMO.md` |
| Annotated disassembly receipts (capstone, raw) | `outputs/2026-10-08-l25-runtime/variant-c-proposal/engine-listener-binary-receipts.txt` |

Analyzed image: `outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin`,
sha256 `3b6cfad4...` (the seed-table-pinned image; verified before analysis).

## 1. Task 1 — Existence + address: FOUND, and the family is LIVE

The "way the other bindings were found" splits in two: LoadMap /
InitGameState / ProcessEvent are VTABLE WALKS (CDO reflection → vtable slot —
`OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL`, UnrealInitializer.cpp:2181/2316),
which cannot apply to a non-virtual member function. The applicable way is the
SEED route (offline derivation against the sha-pinned image; runtime =
module base + file-image label), same as the FName ctor and
StaticConstructObject slots.

Method (full detail in DECISION-MEMO §1 / receipts file):
1. GUObjectArray (seed slot 0, file-image 0x93BA110) is BSS — so I harvested
   CODE references: exec-segment-wide scan for RIP-relative instructions
   targeting it → 1,565 xrefs; 148 are `lea rdi,[GUObjectArray]` + direct
   `call/jmp rel32` (this-call sites).
2. Target histogram → CoreUObject cluster; body analysis identified:
   - **Add @ file-image 0x4841A0 (runtime 0x8841A0)** — lock, append rsi to
     the TArray at this+0x68, engine-allocator growth, unlock. No Contains.
   - **Remove @ file-image 0x484240 (runtime 0x884240)** — lock, Contains scan,
     memmove swap-with-last, Num--, shrink, unlock.
   - Corroboration: shutdown-notify @0x4843A0 (slot-3 dispatch over both
     arrays), GC NotifyUObjectDeleted dispatch @0x484062 (slot-2 `[vptr+0x10]`
     over the 0x68 array under the same lock).
3. Presence is live: 14 engine call sites register listeners at boot (one
   registers a STATIC global listener), 69+ sites call Remove. Unlike Z6's
   dead-stripped diagnostics family, this family has shipping callers — it
   survived linking because the engine itself uses it.

## 2. Task 2 — Signature + call

`void FUObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener*)` —
PS5 x86-64 SysV member: **rdi = this = &GUObjectArray (seed slot 0, runtime
0x97BA110), rsi = listener, void return**. Verified at the 14 engine call
sites (`lea rdi,[GUO]; mov/lea rsi,<listener>; call 0x4841A0`). Port invokes
it as a plain `void(*)(void*, void*)` function pointer. Remove identical.

## 3. Task 3 — Lock + allocator (Claude's "locked, engine allocator"): PROVEN from bytes

- Lock: Add/Remove acquire the critical section at this+0x78 (mutex via PLT
  lock-primitive + pthread_self owner @0x80 + recursion count @0x88) before
  touching the array; GC dispatch and shutdown take the same lock. Mutex is
  RECURSIVE → our OnUObjectArrayShutdown self-Remove re-entry is legal.
- Allocator: growth = 0x298A0 (quantize) → 0x62FC0 (ResizeAllocation) →
  **0xBEF20 = engine FMemory::Realloc → engine GMalloc global at file-image
  0x9340950 → virtual [vtable+0x40]**. The engine's own GMalloc end-to-end;
  the port's GMalloc binding is bypassed (bonus: engine GMalloc slot + Realloc
  vtable offset now known — relevant to Z11's in-allocator-crash suspect).

## 4. MAJOR by-product — the profile's 0x78 is the LOCK, not the array (the AA answer)

Four independent engine functions prove: **create TArray @0x58, delete TArray
@0x68, UObjectDeleteListenersCritical @0x78 (24-byte recursive pthread mutex:
mutex/owner/count), PrimarySerialNumber @0x90, total ~0x98.** The profile's
create=0x68/delete=0x78/serial=0xB0/total=0xB8 assumed a 40-byte pthread mutex;
the real one is 24 bytes. Therefore the port's hand-rolled registration has
been reading/writing {mutex, owner, count} as {Data, Num, Max} — the l24
init-crash family's registration write was aimed at the WRONG OBJECT, and its
first growth allocation was the Z11-symbolized GMalloc abort. Variant C moots
this for registration, but the profile must be fixed first for O's
instrumentation and any future array reads (AA's report was NOT present at
writing; reconcile if it differs — receipts are attached).

## 5. Task 4 — Patch shape

`variant-c-patch.diff` against the CURRENT candidate (sha **4f4afddf**... —
the item-4 agent's edits landed mid-analysis; file was 8f45e6ce... at task
start; my diff already rebased once: refusal code **7** because item-4 took
6). Purely additive, 8 hunks, one file (`UObjectArray.cpp`):
- New compile switch `MODBRIDGE_L25_ENGINE_ADD_LISTENER`.
- TU-local resolver deriving the two labels from the seed-bound GUObjectArray
  pointer (module base = GUO − 0x93BA110; no naked text reads; alignment +
  null checks; clean refusal code 7 `EngineListenerCallUnavailable`).
- Add/Remove: `#if` engine-call path (resolve → call → duration log) with ALL
  existing instrumentation markers kept (same tags; add_done/remove_done now
  carry elapsed-ms, documented); `#else` keeps the entire hand-rolled path;
  variant-B switch untouched (composes at the call sites: B+C = skipped, A+C
  = engine-side registration — the recommended production shape).
- Supersession note added to the stale "not reachable from this build" block
  comment.

## 6. Task 5 — Verdict / fallback / risks (memo has the full list)

- **Feasibility: YES.** Present, located, live-called, locked, engine-allocator.
- Fallback (not needed here): Z11's deferred game-thread registration via the
  R3 marshal pump; remains the design for any future title that strips the
  family.
- Top risks: registration now ENABLES engine-side slot-2 dispatch into our
  listeners (needs the landed slot-4 vtable fix — in); engine Add has no
  duplicate refusal (our callers are once-per-process distinct objects);
  labels are pinned to sha 3b6cfad4 (re-derive per title); rebase hazard if
  item-4 keeps editing; seed-slot promotion + admission-time prologue
  verification left as the follow-up for Claude's decision.
