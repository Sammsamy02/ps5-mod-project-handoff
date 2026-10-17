# FC-1 PROBE SPEC UPDATE — worker-fname-fc1-prep (2026-10-04)

Amends `outputs/2026-10-04-zcode-team/worker-fname-construction/IN-GAME-EXPERIMENT.md`
FC-1 (a). Scope: byte-shape assertions at runtime `0x400000+0x23d590 = 0x63d590`
(proven LOAD0 correspondence: mapped_start(LOAD0)=0x400000, EXEC span byte-exact,
ZCODE-FORK-HANDOFF PROVEN SEGMENT FACTS). All constants below are materialized
from the frozen capture (sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`,
verified unchanged across every access this task).

## 0. What is newly materialized vs still pending

MATERIALIZED (read-only, offline, from the frozen capture):
- `0x23d590` lies in PT_LOAD[0] (vaddr 0x0, offset 0x4000, filesz 0x7479b1c, flags R+X) at file offset `0x241590`. Identity mapping file→memory holds, so in-place runtime bytes == capture bytes.
- Exact bytes at 0x23d590 (64 bytes):
  `554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a66900fb7ff09f90fb77e024883c6026685ff75ee31ff83f97f400f97c748c1e72048`
- First 24 bytes (entry prologue + UTF-16 scan setup):
  `554889e553504889f04889fb4885f674330fb7384889c631c9`
- Anchor-string candidate counts: exactly **1** occurrence each of UTF-16LE
  `\0Engine\0`, `\0Renderer\0`, `\0AnimGraphRuntime\0`, `\0Landscape\0`,
  `\0RenderCore\0` inside file-backed PT_LOAD ranges (re-run stderr markers).
  Consequence: a live LEA disp32 landing on the Engine-family anchor is
  unambiguous — no multi-candidate ambiguity exists for these five names.

PENDING (calls_by_anchor): the resolver binary wedge documented in
`CALLS-BY-ANCHOR.md` prevented materializing the per-anchor call-site table.
Step 3 call-site checks below are therefore SPECIFIED but their `c_i` list is
pending the controller re-dispatch decision. Everything in sections 1–2 is
executable NOW by the v4 probe and is sufficient to raise FC-1 above
"Inconclusive-nothing": code-shape now asserts EXACT bytes, not just
plausibility. Per the FC-1 pass rule, exact-byte code-shape ALONE still caps
the verdict at `Inconclusive`; `Validated` additionally requires the
all-five call-site rel32 re-validation of section 3.

## 1. Exact byte assertions at runtime 0x63d590 (assert, no tolerance)

- B1: bytes `[0x63d590, 0x63d590+24)` equal exactly
  `55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74 33 0f b7 38 48 89 c6 31 c9`.
- B2: bytes `[0x63d590, 0x63d590+64)` equal exactly the 64-byte hex above.
  Any first-divergence byte is recorded with its offset; any mismatch → FC-1
  `Refuted` (candidate address is not the surveyed code in the live image) and
  the ladder stops. No rescans.
- Rationale: the in-place LOAD0 mapping is byte-identical to the capture
  (fself mapped segments are not relocated in-place beyond base addend; ELF
  LOAD0 has no dynamic relocation applied to this code region in the frozen
  survey). If B1/B2 ever fail, that is itself a layout fact worth stopping for.

## 2. Structural (decode) assertions at runtime 0x63d590

Soft evidence recorded alongside B1/B2 (each is a substring of the exact bytes,
so they cannot pass if B2 passes; they make the Refute diagnosis diagnostic):
- S1: `55 48 89 e5` (push rbp; mov rbp,rsp) — the same entry-prologue
  plausibility `probe_code_shape` requires (equivalent of G6 for Tick/SCO).
- S2: UTF-16 character scan shape: `0f b7` movzx-r16 loads, `48 83 c6 02`
  (add rsi,2) stride-2 walk, `66 85 ff` (test di,di) terminator test,
  `75 ee` backward branch — i.e. the prologue walks a NUL-terminated UTF-16
  string by 2-byte units starting at RSI. This corroborates the
  `fname_native_constructor_abi` explicit-output contract from a third
  lineage (bytes now, classifier + port previously).
- S3: length clamp `83 f9 7f` (cmp ecx,0x7f) with `40 0f 97 c7` (seta dil)
  then `48 c1 e7 20` — a >127-char saturation flag consistent with FName's
  128-char pool limit. Recorded as evidence only; never used to infer widths
  (width rule D-1 unchanged: FName slot width remains unproven).

## 3. Call-site re-validation (FC-1 step 3, run when calls_by_anchor is materialized)

For every materialized call-site vaddr `c_i` (anchor order
Engine, Renderer, AnimGraphRuntime, Landscape, RenderCore; resolver
fname_call.rs:9 NAME order):
- T1: read 5 bytes at runtime `0x400000+c_i`; require `E8 rel32`
  (first byte 0xE8).
- T2: relocated target `0x400000+c_i+5+rel32(signed)` must equal `0x63d590`
  exactly. rel32 == `0x63d590-(0x400000+c_i+5)` truncated to i32.
- T3 (window walk, mirrors fname_call.rs:27-49): from the recorded window
  start `w_i` (runtime `0x400000+w_i`), decode forward with the same rules:
  first instruction must be `LEA RSI,[rip+disp32]` whose target equals
  `0x400000+<anchor string vaddr>` (exactly one candidate exists per anchor
  name — see section 0); subsequent instructions must be Next-flow only (no
  intervening call/jcc/ret/jmp), RSI must never be rewritten, RDI must be
  written exactly by `LEA RDI,[RSP|RBSP+disp]` (no index) = stack output slot,
  EDX must be written exactly by `MOV EDX,1` (mode = FNAME_Find), and the walk
  must terminate at the T1 call within 64 bytes.
- Count matches/misses over all `c_i`. FC-1 `Validated` iff B1+B2 pass AND
  every `c_i` passes T1–T3. Any T-miss → `Refuted` + STOP (existing rule).

## 4. Controller decision required (unchanged from census A-3 note)

`calls_by_anchor` materialization is BLOCKED by the wedge documented in
`CALLS-BY-ANCHOR.md`. Options: (a) re-dispatch the prepared builder
(`rerun-calls-by-anchor/`) on a host slot with memory headroom and a non-stuck
kernel state (its survey-identical build order is already in place); (b) extend
`ps_fname_ctor_call_file_image` (resolver crate lib.rs:303) to serialize
`Candidate.calls_by_anchor` — a one-line owner-approved change to the pinned
crate, then one survey re-run. Either unblocks section 3 only; sections 1–2
need nothing further and can ship with v4 immediately.
