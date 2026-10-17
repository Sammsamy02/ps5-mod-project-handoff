# FC1-CHECKS — FName ctor call-site candidate 0x23d590 (runtime 0x63d590)

Worker: `worker-fname-fc1-materialize` (2026-10-04). Frozen capture:
`.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(194241424 B, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`).

Provenance of the call-site table (`calls_by_anchor.materialized.json`): offline read-only
port of the UNMODIFIED survey resolver contract
(`ue4ss-native-tick-relocation-resolver-1/crate/src/fname_call.rs` `resolve_with_ranges` +
`call_after`, `crate/src/xrefs.rs` `scan_xrefs`/`targets_with_indirection`), executed by
`fc1_offline_scan.py` (mmap PROT_READ, capstone 5.0.7, CPU-bounded). The native emitter
binary was NOT runnable this session: every launch (this worker's and sibling workers',
1:21PM–2:47PM local) wedged in-kernel inside `fname_call::resolve` under host swap
pressure (11.18/12.28 GB used) after printing "calling fname_call::resolve"; processes
stuck uninterruptible, SIGKILL-pending. This port reproduced the frozen survey result
(`fname_ctor_call_candidate status=0 linked=0x23d590`, run-release-1) and additionally
materialized the `calls_by_anchor` field that the survey FFI drops.

Mapping facts used (TRUSTED, per worker-mapping-recovery + worker-anchor-validation
`zcode_v3_MAPPING_BREAKTHROUGH` facts): uniform load bias `runtime = 0x400000 + vaddr`
for all segments; exec span `[0x400000, 0x787c000)` (exactly LOAD0); static
`file_offset = vaddr + 0x4000` for LOAD0.

---

## 1. The candidate function (FC1-A)

- vaddr `0x23d590`, file offset `0x241590`, runtime `0x63d590` (inside exec span).
- Expected first 32 bytes (exact equality; static AND live):
  `55 48 89 e5 53 50 48 89 f0 48 89 fb 48 85 f6 74 33 0f b7 38 48 89 c6 31 c9 66 85 ff 74 2a 66 90`
  = `push rbp; mov rbp,rsp; push rbx; push rax; mov rax,rsi; mov rbx,rdi; test rsi,rsi;
  je +0x33; movzx edi,word[rax]; mov rsi,rax; xor ecx,ecx; test di,di; je +0x2a; nop`.
- Optional strict 64-byte check (next 32 bytes):
  `0f b7 ff 09 f9 0f b7 7e 02 48 83 c6 02 66 85 ff 75 ee 31 ff 83 f9 7f 40 0f 97 c7 48 c1 e7 20 48`
- Matches `worker-expected-bytes/EXPECTED-BYTES.json` byte-for-byte (independent extraction).

ABI shape implied: RDI = output FName slot, RSI = wide-string input (null-checked first),
callee saves RDI in RBX and rewrites RSI — consistent with the caller-side contract below.

## 2. calls_by_anchor (materialized, 9 call sites)

All sites verified: byte[site] == `E8`, rel32 target == `0x23d590` (vaddr arithmetic;
identical at runtime under the uniform bias: target_runtime = site_runtime + 5 + rel32).

| anchor | call vaddr | call runtime | call bytes (E8+rel32) | window start vaddr | window start runtime |
|---|---|---|---|---|---|
| Engine | 0x2f07077 | 0x3307077 | `e8 14 65 33 fd` | 0x2f07060 | 0x3307060 |
| Engine | 0x3a17b91 | 0x3e17b91 | `e8 fa 59 82 fc` | 0x3a17b7e | 0x3e17b7e |
| Renderer | 0x174d6af | 0x1b4d6af | `e8 dc fe ae fe` | 0x174d69c | 0x1b4d69c |
| Renderer | 0x174d6d8 | 0x1b4d6d8 | `e8 b3 fe ae fe` | 0x174d6c8 | 0x1b4d6c8 |
| Renderer | 0x2f85635 | 0x3385635 | `e8 56 7f 2b fd` | 0x2f85625 | 0x3385625 |
| Renderer | 0x3a17bbc | 0x3e17bbc | `e8 cf 59 82 fc` | 0x3a17ba9 | 0x3e17ba9 |
| AnimGraphRuntime | 0x3a17be7 | 0x3e17be7 | `e8 a4 59 82 fc` | 0x3a17bd4 | 0x3e17bd4 |
| Landscape | 0x3a17c50 | 0x3e17c50 | `e8 3b 59 82 fc` | 0x3a17c3d | 0x3e17c3d |
| RenderCore | 0x3a17ca6 | 0x3e17ca6 | `e8 e5 58 82 fc` | 0x3a17c93 | 0x3e17c93 |

Note the registration-function cluster at 0x3a17b7e..0x3a17ca6: one function performs five
back-to-back FName constructions, one per anchor string, in resolver NAMES order
(Engine, Renderer, AnimGraphRuntime, Landscape, RenderCore) — this is why the resolver's
five-anchor intersection is exactly one address (uniqueness guard passed offline:
each anchor had exactly 1 string candidate and exactly 1 distinct call target).

Anchor string content vaddrs (UTF-16LE, the `lea rsi` targets): Engine `0x782706e`,
Renderer `0x799db12`, AnimGraphRuntime `0x75eefc0`, Landscape `0x763151e`,
RenderCore `0x79f02e2` (runtime +0x400000: `0x7c2706e`, `0x7d9db12`, `0x79eefc0`,
`0x7a3151e`, `0x7df02e2` — all beyond the exec span; see FC1-D gating).

## 3. FC-1 byte-shape checks (what the probe / composed runtime asserts)

Order matters: bytes first, then decode-window shape, then (gated) data checks. All code
checks are inside the trusted exec span and read-only.

### FC1-A — candidate prologue (hard gate)
At runtime `0x63d590`: `memcmp(32)` equal to the byte string in section 1.
Pass: equality. Any mismatch = candidate refuted at runtime (if Tick 0x33ac8c0 / SCO
0x890aa0 prologues still match, refutation is specific to the FName candidate, not the base).

### FC1-B — call-site window shape (per site, all 9)
Decode forward from each runtime window start (<= 64 branch-free bytes ending at the call):
1. First instruction: `48 8D 35 <disp32>` (`lea rsi,[rip+disp32]`), where
   `start + 7 + (int32)disp == anchor_string_runtime` for that site's anchor.
2. Within the window, exactly one write to RDI, of the form
   `lea rdi,[rbp|rsp + disp]` with no index — observed encodings here are all
   `48 8D 7D <disp8>` (`lea rdi,[rbp-disp]`, disp in 0x80..0xF0). (Generic mask also
   accepts `48 8D 7C 24 <disp8>` / disp32 forms.)
3. Within the window, exactly one write to RDX, of the form `BA 01 00 00 00`
   (`mov edx,1`).
4. No call, jump, ret, or write to RSI anywhere between `start` and the call
   (branch-free path, argument evidence preserved).
5. Terminator: `E8 <rel32>` with `call + 5 + (int32)rel32 == 0x63d590`.

Pass: all 9 sites satisfy 1–5 with their listed anchor. Reference exact window bytes
(start .. call+5, observed this session) for three representative sites:
- Engine1 (28 B): `48 8d 35 07 00 92 04 | 48 8d 7d e8 | ba 01 00 00 00 | 48 8b 03 48 89 45 f0 | e8 14 65 33 fd`
- Renderer2 (21 B): `48 8d 35 43 04 25 06 | 48 8d 7d 80 | ba 01 00 00 00 | e8 b3 fe ae fe`
- RenderCore (24 B): `48 8d 35 48 86 fd 03 | 48 8d 7d b0 | ba 01 00 00 00 | 48 89 c3 | e8 e5 58 82 fc`

A cheaper (weaker) byte-only variant of FC1-B for a constrained probe: at each runtime
site check `byte[site] == 0xE8` and rel32 arithmetic (FC1-B5) plus the first-instruction
bytes at the window start `48 8d 35`. Full shape checks are preferred.

### FC1-C — uniqueness guard
Exactly one address in the exec span is the rel32 target of branch-free windows from all
five anchors. Offline this passed uniquely (0x23d590). A runtime probe does NOT need to
re-derive this; it must FAIL CLOSED if the code bytes moved such that two or more sites
on the recorded list disagree (e.g., one site passes FC1-B and another fails).

### FC1-D — anchor strings (secondary, gated)
Only if FC1-A and FC1-B pass, and only behind a prot-readable gate (these vaddrs map
beyond the exec span, in the read/gap region — do not deref blindly): at each
anchor content runtime address, bytes must be UTF-16LE `Name` followed by `00 00`
(e.g. Engine: `45 00 6e 00 67 00 69 00 6e 00 65 00 00 00`).
Pass: match. Mismatch refutes the uniform `+0x400000` bias for the data segments
(it does NOT refute the code-side candidate, which FC1-A/B established independently).

### FC1-E — what would refute the candidate outright
- FC1-A mismatch while other trusted anchors (Tick/SCO prologues) hold.
- Any recorded site whose runtime bytes are not `E8` (mapping refuted) — distinguish
  from rel32 mismatch (base/candidate drift) by comparing against the static image at
  `vaddr + 0x4000` (expected bytes identical to the runtime expectations above).
- A window that decodes but lacks `mov edx,1` or the `lea rdi` output store — the call
  is then a different overload, and the FC-1 composition contract (RDI out, RSI anchor
  text, EDX=1) is void for that site.
- A second address satisfying FC1-B for all five anchors (ambiguity — none observed
  offline; would invalidate pinning the ctor by this anchor set alone).

## 4. Status

- Offline materialization: COMPLETE (9 sites, all revalidations true).
- Native resolver binary re-run: BLOCKED (documented above and in REPORT.md);
  `calls_by_anchor` here comes from the faithful offline port, not from a native run.
- Runtime/live verification of FC1-A..D: NOT performed (no engine execution this
  session). These checks are the acceptance contract for the anchor-validation probe /
  composed runtime.
