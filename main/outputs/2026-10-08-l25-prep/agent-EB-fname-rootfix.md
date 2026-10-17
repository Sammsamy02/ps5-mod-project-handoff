# Agent EB — FName adapter decode ROOTFIX (l27): verdict, ABI truth, patch draft

Date: 2026-10-08 (late night). READ-ONLY analysis + patch draft. Written:
only `outputs/2026-10-08-l26-runtime/fname-rootfix/` (fname-rootfix.diff,
DECISION-MEMO.md, harness/) and this report. Binary ground truth re-derived
from bytes against the sha-pinned decrypted eboot
(3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc,
re-verified; image vaddr -> file +0x4000, runtime +0x400000 — every
RIP-relative constant lands exactly; no image discrepancy).

## HEADLINE: the assigned decode theory is REFUTED — the port's decode is byte-level CORRECT

CB's core finding is real (0x242470's raw RAX IS `id|(id<<32)`,
0x242555-66), but it is a mid-chain value only the outer ctor overloads
consume, and **every one re-normalizes to `{id, Number<<32}` before the only
result handoff** — the sret store `mov qword ptr [rbx],rax` @0x23d5f0. The
port reads exactly those 8 bytes. A decode fix would have shipped zero
console effect while "confirming" a wrong theory.

## 1. ABI truth per function (all [B], disassembled end-to-end)

| Function | Addr | Return ABI |
|---|---|---|
| ctor (slot 2) | 0x23D590 | **sret**: RDI=out->RBX @0x23d599; `mov [rbx],rax` @0x23d5f0 = ONLY handoff; epilogue rets **without reloading RAX** (register return = garbage by contract) |
| inner with-length | 0x23D600 | RAX = {id, Number<<32}: `shl r14,0x20/mov eax,eax/or rax,r14` @0x23d83f-45 (r14=0 for suffix-less; `_NN` parse adds +1 internal @0x23d6e1) |
| interning core | 0x242470 | RAX = id(id<<32) @0x242560-66 (the trap; normalized by all outer overloads: 0x23d954, 0x23dc76, 0x23e038, 0x23e0a7, 0x23e585, 0x23e682); len>=0x400+Find -> {0,0} @0x242505 |
| shard-Find | 0x23C340 | EAX = handle & 0x1FFFFFFF on hit @0x23c409; EAX = 0 on miss @0x23c3ec. **NO shift** |
| Store (Add) | 0x23C0B0 | EAX = handle on insert (block<<16 \| off16, @0x23c2ba); EAX = handle & 0x1FFFFFFF on probe-hit @0x23c31c. **NO shift** |
| allocator | 0x246090 | RAX = (CurrentByteOffset>>1)<<32 \| block (`shr ebx,1` @0x246122) |

**Id layout uniform everywhere: `tag(3)<<29 | block(13)<<16 | off16`,
off16 = entry byte-offset/2.** ByteProperty = block 0, byte 6 -> off16 3 ->
id 0x3 ✓ EName table ✓ console probe. The port's
`rc_seed::entry_location` (SeedValidation.hpp:92-97, `(id>>16,
(id&0xFFFF)*2)`) decodes **exactly** the same. **No `id<<16` transform
exists anywhere.** New finding: the ctor chain only NARROWS (mask
{00 02 04 06 08 0a 0c 0e} @0x7A41900); case-insensitivity lives in the hash
functions (0x239B00/0x23A2C0 lowercase in-register) and the final compares
0x8CDB0/0x8CE20 (**case-insensitive**, tolower table @0x7C26E30) — entry
data is stored ORIGINAL case, mixed-case queries hit.

## 2. The port's decode bug (there is none) — the audit

`NativeFNameAdapter.hpp:10,45-47` (Ctor ABI + RAX-ignored, both correct by
disassembly) -> `UnrealInitializer.cpp:959-968` (thunk returns
`FName{index,number}`, explicit ctor NameTypes.hpp:282 = engine layout) ->
`construct_with_string` NameTypes.hpp:239-250. The init Phase-4 probe
(UnrealInitializer.cpp:1123-1155) verifies id parity AND number==0
(NativeFNameChecks.hpp:55 refuses otherwise) AND pool-ToString parity — a
packed-return contamination is REFUSED at bind. **Decode chain: correct at
every link.**

## 3. Task 4 answer — Add vs Find is NOT an ABI difference

Byte-diffed the two arms through the same bound ctor: identical hash call,
shard, bucket, tag/header/probe material, Blocks access, and the same
`&0x1FFFFFFF` hit return. **The engine cannot return different ids for
Add(probe-hit) and Find of the same string.** The l25 record (Add-mode
FindFirstOf worked in-game while member-name FNAME_Find failed the same
session) therefore points OUTSIDE the engine chain. Surviving candidates,
evidence-ranked:
1. **Input-string conversion layer** — hard proof it breaks: v7 S1/S2/S3/U2
   all THREW `GetPackageNameFromLongName: Name wasn't long` for DOTTED
   paths ("/Script/Engine.PlayerController" cannot lack a dot unless the
   conversion mangled it — ToCharTypePtr family, candidate
   UObjectGlobals.cpp:316-331). A mangled Find query yields silent {0,0}
   or a wrong-but-valid hit on whatever it spells.
2. **DA's mismatch-detector ToString route** (Kismet Conv_NameToString;
   U2's CDO lookup threw through the same broken string layer) — a
   false-positive mismatch fires even with a correct id.
3. Wrong-but-valid ids from slot 2 itself: impossible (full
   case-insensitive compare precedes any hit).

Also validated: zero `[FFO] add-fallback` = real "names resolve" signal
(FileDevice `receive()` ignores log levels — no filtering exists).

## 4. The fix shape (fname-rootfix.diff — round-trip patch(1)-verified)

Zero decode behavior change (nothing to fix). Two hunks:
1. **Enriched `[FFO] query-fallback` marker** (inside DA's
   `__PROSPERO__` block, same dedupe/cap): `[n:{:X} num:{} ts:'{24-char
   ToString tail}']` — one line that discriminates the real root cause on
   the next authorized launch: `ts:'<other name>'` = corrupted INPUT string
   (ToCharTypePtr family); `ts:''`+plausible id = Kismet ToString route is
   the liar; `num:!=0` = the only decode-side shape (slot-2 rebind to the
   raw core). Uses only symbols already in that TU; NOT engine-compiled
   here — rebuild owner should run the Z13 syntax recipe before staging.
2. **Proof-lock comment** on NativeFNameAdapter.hpp with the disassembly
   receipt (comment-only; host syntax-checked) so the refuted theory can't
   be re-derived and "fixed".

**Predicted console effect (honest):** markers do NOT disappear from this
patch — no decode changed. The enriched line names the true bug family in
one launch; if the fallback stays silent while FindXOf returns nil, N1 is
falsified for that pool state and the walk/exclusion layer returns to rank 1.

## 5. Verification

`harness/fname-abi-harness.cpp` (+run-harness.sh): 18-check executable
spec — ByteProperty id 0x3 parity (`Equals(stored)` AND full operator==),
Add-probe-hit id == Find id, the core-packing trap reproduced AND proven
unreachable through slot 2 (and incapable of the all-nil signature —
Equals is index-only), id<<16 falsified, miss->Add insert semantics, and
case-insensitive hit. **ALL-PASS 18/0** (clang++ -std=c++20 -Wall -Wextra,
zero warnings).

## Provenance

Inputs consumed: agent-CB/CJ/DA/CP/CR/CX/EG reports, /tmp/l26v2+v6+v7
session logs, harvest-l25-B-final (§2.2 record), l26 candidate/original
trees, l18 units, the pinned eboot. Scratch scripts in /tmp/l26-eb-fname
only. No writes outside fname-rootfix/ + this report; no staging, no
CHANGES.json, no rebuild, no console contact.

— agent EB (ZCode), 2026-10-08.
