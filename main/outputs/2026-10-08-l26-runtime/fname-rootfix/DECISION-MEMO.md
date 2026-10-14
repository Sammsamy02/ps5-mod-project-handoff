# DECISION-MEMO — FName adapter decode rootfix (agent EB, l27, 2026-10-08)

Scope: READ-ONLY analysis of the assigned theory (CB's decode theory: the
interning core 0x242470's raw return is `id|(id<<32)`, and a wrong decode of
the ctor's return reproduces the console symptom "Find-mode indices NEVER
equal stored NamePrivate indices in-world"), plus this patch draft and
harness. Nothing outside `fname-rootfix/` was written. Binary ground truth
re-derived from bytes: `outputs/2026-10-04-zcode-team/worker-game-version/
console-decrypted-eboot.bin`, sha256 3b6cfad47dd17fea17d429bce80ae374d1fd2ef
8894672cce63f5a26853622fc (re-verified). Mapping (verified against the
kPrefix admission bytes at 0x23D590 and every RIP-relative constant): exec
image vaddr -> file = +0x4000; runtime = image + 0x400000.

## 1. VERDICT

**The port's decode of the bound ctor's return is byte-level CORRECT. The
assigned decode theory is REFUTED. There is no decode bug to fix — and a
decode fix would have shipped zero console effect while "confirming" a wrong
theory.** The observed in-world signature (names resolve, FindXOf nil) is
NOT producible by the 0x23D590 call chain as bound; the surviving root-cause
candidates are the INPUT STRING layer (proven broken elsewhere by the v7
S-probe throws) and DA's mismatch-detector ToString route (Kismet
Conv_NameToString), both discriminated by this draft's enriched marker on
the next authorized launch (section 6).

## 2. ABI truth per function (task 1 — byte-level)

| Function | Image | Input ABI | Return ABI (byte-exact) |
|---|---|---|---|
| `FName(const WIDECHAR*, EFindName)` **ctor, slot 2** | 0x23D590 | RDI=sret out (saved RBX @0x23d599), RSI=UTF16 name, EDX=mode forwarded untouched | **sret**: `mov qword ptr [rbx],rax` @0x23d5f0 is the ONLY result handoff (8 bytes = the engine FName {ComparisonIndex, Number}); epilogue @0x23d5f3-f9 pops+rets **without reloading RAX** — the register return is the leftover inner value, garbage by contract |
| ctor internal (with-length wide) | 0x23D600 | RDI=chars, RSI=len\|wide<<32, EDX=mode | **RAX = {id, Number<<32}**: `shl r14,0x20 / mov eax,eax / or rax,r14` @0x23d83f-45 (r14 = `_NN` suffix Number, `+1` internal encoding @0x23d6e1; 0 for suffix-less @0x23d631/0x23d816) |
| interning core (FindOrStore dispatcher) | 0x242470 | RDI=chars, RSI=chars<<32\|len, EDX=mode | **RAX = id(id<<32)**: `mov ecx,eax / shl rcx,0x20 / mov eax,eax / or rax,rcx` @0x242555-66 — CB's packing CONFIRMED, but consumed only by the outer overloads, every one of which re-normalizes (0x23d954, 0x23dc76, 0x23e038, 0x23e0a7, 0x23e585, 0x23e682); len>=0x400+Find -> {0,0} silent @0x242505; edx==1 -> Store, else -> shard-Find |
| shard-Find (THE internal find) | 0x23C340 | RDI=shard, RSI=key struct {chars@0, chars<<32\|len@8, hash@0x10, tag@0x18, probe@0x1c} | **EAX = handle & 0x1FFFFFFF** on hit (@0x23c409 `mov r15d,0x1fffffff / and [bucket]`), **EAX = 0** on miss (@0x23c3ec `xor r15d,r15d`). **NO shift of any kind** |
| Store (Add) | 0x23C0B0 | RDI=pool, RSI=chars, RDX=chars<<32\|len | **EAX = handle** on insert (@0x23c2ba `shl ebx,0x10` \| off16 — the SAME id space), **EAX = handle & 0x1FFFFFFF** on probe-hit (@0x23c31c). **NO shift** |
| entry allocator | 0x246090 | RDI=pool, RSI=chars, RDX=len material | **RAX = (off16)<<32 \| block** with `off16 = CurrentByteOffset>>1` (@0x246122 `shr ebx,1`) |
| id-bounds getter | 0x23D570 | EDI=id | compares CurrentBlock+1 vs `id>>16` — the engine's own id decode |

**Id layout, uniform across every producer and consumer:**
`tag(3 bits, probe-hash)<<29 | block(13)<<16 | off16`, where
`off16 = entry byte-offset within the 0x20000 block, divided by 2`. Verify:
entry address = `Blocks[block] + off16*2` (find @0x23c3a8/ad, store
@0x23c27f/91 — identical); "None" occupies bytes 0..5 of block 0, so
ByteProperty sits at byte 6 -> off16 3 -> **id 0x3**, matching the pool EName
table and the console-verified probe. The port's own
`rc_seed::entry_location` (units/SeedValidation.hpp:92-97:
`(id>>16, (id&0xFFFF)*2)`) decodes **exactly** the same.

**Case semantics (new finding):** the ctor chain only NARROWS UTF16->ANSI
(mask {00 02 04 06 08 0a 0c 0e} @0x7A41900; AVX2 `ff 00` @0x7C2F574;
`ff 00` byte-loop) — it does **not** lowercase. Case-insensitivity lives in
(a) the hash functions (lowercase computed in-register: 0x239B00 ANSI /
0x23A2C0 WIDE) and (b) the final compares 0x8CDB0 / 0x8CE20, which are
**case-insensitive** via a 256-byte tolower table @0x7C26E30 (ASCII only;
wide chars >0x7F mismatch). Entry data is stored in ORIGINAL case. A
mixed-case port query and the engine's stored entry therefore compare equal.

**No `id<<16` transform exists anywhere in the chain** (task 1 answer). The
`id<<16` hypothesis is falsified at every producer: find returns the raw
masked handle; store composes block<<16|off16 with off16 = byteoff>>1 (not
byteoff); the allocator hands out (off16<<32)|block.

## 3. The port's decode — audit (task 2)

The complete decode chain, all verified:

1. `units/NativeFNameAdapter.hpp:10` — `Ctor = void(*)(Name* out, const
   char16_t*, uint32_t mode)`; `Name{index@0, number@4}` 8 bytes align 8 —
   **matches the engine's sret contract exactly** (out in RDI, UTF16 in RSI,
   mode in EDX; engine saves RDI->RBX @0x23d599).
2. `:45-47` — `ctor(&r.value, text, mode)`; the comment "RAX is ignored" is
   correct-by-disassembly (0x23D590 never reloads RAX after `mov [rbx],rax`).
3. `typescan-r4/original/src/UnrealInitializer.cpp:959-968` —
   `native_fname_thunk` returns `FName{result.value.index,
   result.value.number}`; the explicit `FName(uint32,uint32)` ctor
   (port NameTypes.hpp:282) sets ComparisonIndex=+0, Number=+4 — identical
   to the engine's 8-byte form.
4. `NameTypes.hpp:239-250` — `construct_with_string` copies
   ComparisonIndex/Number from the returned FName. No double-unpack, no
   reinterpretation, no endianness game.
5. The init Phase-4 probe (UnrealInitializer.cpp:1123-1155) verifies BOTH
   id parity (`probe id == pool EName id 0x3`) AND `number==0`
   (units/NativeFNameChecks.hpp:55 refuses otherwise) AND pool ToString ==
   input — so a packed-return contamination (id in the high half) is
   REFUSED at bind time. Every failing session bound successfully.

**There is no mismatch. The decode reads precisely the 8 bytes the engine
writes.**

## 4. Why CB saw what he saw (the kernel of truth in the theory)

CB's §3 core-return finding is real — 0x242470 does leave `id|(id<<32)` in
RAX — but it is a mid-chain value only the OUTER overloads consume, and
they re-normalize before the sret store. His own §3 already documented this
correctly ("high half garbage, normalized only by the outer overloads");
the l27 brief's escalation of it to "a wrong decode of the ctor's return"
is the part that is false. The executable spec
(`harness/fname-abi-harness.cpp`, T3a-T3d) demonstrates both halves: the
core packing exists, AND Equals (index-only) would still match even if it
leaked — so a core-rebind cannot produce the all-FindXOf-nil signature
(only member-name `operator==`, the (b)-path, would break).

## 5. Task 4 — Add-vs-Find cross-check (why Add "worked" but Find "fails")

Byte-diff of the two dispatch arms through the SAME bound ctor:

- Key construction: core Find arm (0x242512-0x242548) vs Store prologue
  (0x23c0c4-0x23c11f) — identical hash call `hash(rdi=chars, esi=len)`
  (0x239B00 ANSI / 0x23A2C0 WIDE), identical shard
  `pool + (hashLo<<6) + 0x10040`, identical bucket `hashHi & shardMask`,
  identical tag/header/probe material from the hash's RDX, identical
  Blocks read `[shard+0x18]->pool; Blocks[block] + off16*2` (find 0x23c39c/
  a8; store 0x23c134 `mov rbx,r12` then 0x23c17e/a). Store's probe-hit
  return and shard-Find's hit return are the same instruction pattern
  (`& 0x1FFFFFFF`).
- **The engine cannot return different ids for Add(probe-hit) and Find of
  the same string.** The l25 console record (FindFirstOf(Add-default) found
  ItemDatabaseSubsystem in-game while the member-name FNAME_Find path
  failed in the same session) is therefore NOT an ABI/decode asymmetry. The
  surviving explanations, in evidence order:
  1. **Input-string layer corruption on the Find consumers' paths.** Hard
     proof this layer can break: v7's S1/S2/S3/U2 probes all THREW
     `GetPackageNameFromLongName: Name wasn't long` for dotted paths
     ("/Script/Engine.PlayerController" — a string that cannot lack a dot
     unless the conversion mangled it). The StaticFindObject path runs
     `ToCharTypePtr(InName)` (typescan-r4 candidate UObjectGlobals.cpp:316-
     331 family); the member-name Find path builds its UTF16 through a
     different conversion site. A mangled query string yields a shard-Find
     MISS (silent {0,0}) or a WRONG-BUT-VALID hit on whatever the mangled
     string spells — exactly the observed family.
  2. **DA's mismatch-detector ToString route** (Kismet Conv_NameToString
     via ProcessEvent, NameTypes.cpp ToStringInternalWrapper fallback):
     v7's U2 (GetKismetStringLibrary CDO) threw through the same broken
     StaticFindObject string layer — if the CDO never resolved or the
     ProcessEvent route degrades in-world, `FindXOfQueryMismatch`
     false-positives (its catch treats ToString failure as mismatch) even
     with a perfectly correct id.
  3. Wrong-but-valid ids from slot 2 itself: **impossible per section 2**
     (full case-insensitive compare of the full length precedes any hit).

## 6. The fix shape (task 3) + predicted console effect

Since no decode bug exists, `fname-rootfix.diff` changes ZERO decode
behavior and instead (a) enriches DA's `[FFO] query-fallback` marker with
the constructed Number and a 24-char ToString tail (`__PROSPERO__`-guarded,
inside DA's existing block, same dedupe/128-cap), and (b) proof-locks
`NativeFNameAdapter.hpp` with the disassembly receipt (comment-only; host
syntax-checked). Round-trip verified with patch(1) against both originals.

**Honest prediction:** the current markers will NOT "disappear" from this
patch — no decode changed. On the next authorized launch the enriched line
discriminates one-of-three:

| Marker shape | Meaning | Fix moves to |
|---|---|---|
| `ts:'<different real name>'` | ctor INPUT string corrupted pre-call | ToCharTypePtr/ensure_str conversion site audit (the v7 S-throw family) |
| `ts:''`/garbage, plausible `n:X` | id likely correct; Kismet ToString route broken | ToString binding (CJ's ToStringInternal@0x63FA00 slot recommendation) |
| `num:!=0` | the ONLY decode-side shape (slot-2 rebind to the raw core) | seed-table slot audit |

If DA's fallback stays SILENT and FindXOf still returns nil, N1 is
falsified for that pool state and the walk/exclusion layer (CR's E1/A1)
returns to the top of the queue. Zero `[FFO] add-fallback` lines remain a
valid "names resolve" signal: the FileDevice ignores log levels (no
level filtering exists — `receive()` formats identically for every level),
so Error-level markers do reach UE4SS.log.

## 7. Verification artifacts

- `harness/fname-abi-harness.cpp` + `run-harness.sh` — 18-check executable
  spec of section 2 (sret contract, normalization, core packing trap,
  Add==Find id parity, id<<16 falsification, miss semantics,
  case-insensitivity, ByteProperty id 0x3 Equals parity). Build+run:
  ALL-PASS, 18 checks, 0 failures (clang++ -std=c++20 -Wall -Wextra -O2,
  zero warnings).
- `fname-rootfix.diff` — applies clean (`patch -p1` from `what/`) against
  the 23:33 candidate UObjectGlobals.cpp and the l18 units adapter header;
  adapter hunk host-syntax-verified. The UObjectGlobals hunk reuses only
  symbols already in that TU (`ensure_str`, `FName::ToString`,
  `GetNumber`, `Output::send<LogLevel::Error>` — all present in DA's block)
  but was NOT engine-compiled here (no Prospero front-end run — the
  rebuild owner should run the Z13 recipe before staging).

## 8. What I did NOT do (honesty section)

- No writes outside `fname-rootfix/` + this memo's directory; no staging,
  no CHANGES.json edits, no rebuild, no console contact.
- The disassembly was cross-checked against CB/CJ's prior RE (prologues,
  PLT targets, call census: core 12 callers incl. 0x23D83A, shard-Find
  exactly 1 caller = the core, ctorW 14,401 e8 sites — matching CJ's
  14,401 upper bound). One arithmetic scare (a phantom "+0x200 rodata
  shift") was re-derived in Python and dissolved into exact hits on the
  narrow mask {00 02 04 06...}, the `ff 00` word mask, and the tolower
  table — every RIP-relative constant lands exactly; no image-level
  discrepancy exists.
- The in-world root cause is NOT solved here — it is BOUNDED: the decode
  and the engine chain are exonerated byte-for-byte, and the two surviving
  candidates are named with a one-launch discriminator shipped in the
  draft. Following the studio's evidence law, I did not "fix" a refuted
  theory to look productive.

— agent EB (ZCode), 2026-10-08. Sources: agent-CB-fname-machinery.md,
agent-CJ-engine-fname.md, agent-DA-verified-query-fix.md,
agent-CP-findfirstof-fix.md, agent-CR-object-reads.md,
agent-CX-v7-probe.md, agent-EG-getengine-anomaly.md, session logs
/tmp/l26v2.log, /tmp/v6-run.log, /tmp/v7-run.log, harvest-l25-B-final
(+ its §2.2 record), the l26 candidate/original trees, the l18 units,
and the pinned decrypted eboot.
