# REPORT — worker-vaddr-semantics

Settled, from retained bytes only, whether the Townfall static-survey addresses are ELF
virtual addresses or file offsets, and derived the runtime translation from actual segment
correspondence. Deliverables: `CLAIM.json` (written first), `SEGMENT-CORRESPONDENCE.md`
(consumer table), this report (written last). Call budget ≤35: used 24. No network, no
console, no nested subagents; capture read-only; no writes outside
`outputs/2026-10-04-zcode-team/worker-vaddr-semantics/`.

## Inputs of record

- Capture: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`,
  194,241,424 bytes, sha256 `3b6cfad4...622fc` re-verified this run (matches `capture.json`
  and the survey receipt's `capture_sha256`, `capture_unchanged: true`).
- Survey run: `.superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/run-release-1/`
  (`stdout.txt` produced `guobject linked=0x93ba110`, `static_construct_object 0x490aa0`,
  `gameengine_tick 0x2fac8c0`, plus cfg candidate `alloc=0x483b40 free=0x4840b0 flags=63`).
- Survey binary entry: same spike `src/main.rs` passes the whole file image to
  `ps_*_file_image` endpoints of crate `patternsleuth_native_engine_version` at
  `.superpowers/native-dependency-spike/ue4ss-native-tick-relocation-resolver-1/crate/src/`
  (all 32 sources hash-pinned in `run-release-1/receipt.json`).
- Checkpoint: `worker-2-layout-facts` (`REPORT.md`, `schema/unreal-build-profile.schema.json`,
  `profiles/townfall-ps5-ue56.profile.json`).
- v2 in-game facts: `outputs/2026-10-04-zcode-controller/console/ingame-v2-facts-2026-10-04.jsonl`
  (and v1 twin) — `g5_vmmap` spans and `v2_elf_base_scan`.

## Task 1 — Container + program headers (cited)

ELF64 LE; `e_ident` at file 0x0; header fields at 0x10: `e_type=0xFE10` (SCE EXEC),
`e_machine=62` (x86-64), `e_phoff=0x40`, `e_phentsize=56`, `e_phnum=14`;
`e_shoff=0x6f48a0a8` exceeds file size → section table absent/stale. Parsed the 14
program headers at file offsets `0x40..0x358`. Five PT_LOADs (full table in
SEGMENT-CORRESPONDENCE.md §2):

| ph | flags | offset | vaddr | filesz | memsz |
|---|---|---|---|---|---|
| 0 | E | 0x4000 | 0x0 | 0x7479b1c | 0x7479b1c |
| 1 | R | 0x7480000 | 0x747c000 | 0xa3e918 | 0xa3e918 |
| 3 | RW | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 | 0x1280000 |
| 7 | RW | 0x9140000 | 0x913c000 | 0x1cc5f2 | 0x4f7418 |
| 8 | (none) | 0x930f420 | 0x9633420 | 0x262cea8 | 0x262cea8 |

Non-LOAD notables: PT_GNU_EH_FRAME (ph2) at vaddr 0x747c000; PT_TLS (ph5) and
PT_GNU_RELRO (ph4) at 0x7ebc000; PT_DYNAMIC (ph10) at 0xbc5f278 inside L8.

## Task 2 — Definitive answer: they are p_vaddr values

- Code path that produced the numbers: `ps_*_file_image` → `OwnedImage::parse`
  (`lib.rs`). The SHF_ALLOC section path is impossible here (e_shoff beyond EOF), so the
  `elf_load_spans` fallback ran: it keeps `p_flags & (PF_R|PF_X) != 0` LOADs and pushes
  `OwnedSection{address: p_vaddr, data: bytes[p_offset..p_offset+p_filesz]}`. GUObjectArray
  additionally flows through `guobject_array::resolve_guobject_array` →
  `object_cfg::resolve_owned_file` (`object_cfg.rs:22` `Span{va: p_vaddr,...}`; `:67`
  `let ip=s.va+offset`), Tick through `elf_tick::resolve` (`elf_tick.rs:31/85/90`), SCO via
  `rip4` vaddr arithmetic. Every returned number is in p_vaddr space by construction.
- Byte proof (anchor check): Tick 0x2fac8c0 → file 0x2fb08c0 shows a complete x86-64
  prologue (`55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC 88 00 00 00 4C 8B 2D`);
  SCO 0x490aa0 → file 0x494aa0 likewise. The same numbers read as raw file offsets yield
  mid-instruction/FPU garbage — the file-offset interpretation is dead.
- Independent re-derivation: scanning LOAD0 (file = vaddr+0x4000), 148 `lea rdi,[rip+d]`
  sites target 0x93ba110; sites 0x373692/0x37372a are immediately followed by
  `call 0x483b40` / `call 0x4840b0`, reproducing the survey's CFG candidate evidence.
- Segment homes: Tick & SCO in L0 (X); GUObjectArray 0x93ba110 in L7's BSS (in-segment
  delta 0x27e110 ≥ filesz 0x1cc5f2 — hence no file bytes; `file_byte_offset` must stay null,
  consistent with the profile schema). Checkpoint `elf_image_vaddr` model confirmed correct;
  this run supplies the previously-missing program-header derivation, allowing
  `file_byte_offset` to be filled for file-backed anchors (Tick 0x2fb08c0, SCO 0x494aa0).

## Task 3 — Runtime translation

`runtime_address(V) = mapped_start(seg) + (V - seg.p_vaddr)` for the unique seg containing V
in `[p_vaddr, p_vaddr+memsz)`. Single-bias special case `runtime = B + V` is valid only if
every `mapped_start == B + p_vaddr` (probe must prove it). Worked examples (full table in
SEGMENT-CORRESPONDENCE.md §4):

- Tick: `mapped_start(L0) + 0x2fac8c0` = 0x400000+0x2fac8c0 = **0x33ac8c0** (validated: lands
  inside the observed EXEC span).
- SCO: `mapped_start(L0) + 0x490aa0` = **0x890aa0** (validated: inside EXEC span).
- GUObjectArray: `mapped_start(L7) + 0x27e110` — mapped_start(L7) currently UNKNOWN; the
  0x97ba110 single-bias guess is explicitly unvalidated.

## Task 4 — SEGMENT-CORRESPONDENCE.md

Written for the v3 probe + supplier adapter: per-segment {vaddr range, file range, sizes,
flags, expected prot, anchors}, the file<->vaddr identity, BSS rule, both translation
formulas with worked examples, and the L7/L8 shared-page (0x9633000) boundary caveat.

## Task 5 — v2 VMMAP cross-check

Observed clean spans: EXEC [0x400000,0x787c000) `--X` size 0x747c000; READ
[0x8018c000,0x100118000) `R--` size 0x7ff8c000 (both records' remaining entries are corrupt
zero/negative garbage; `v1_vmmap_page` parts hold no valid spans).

- Consistent: EXEC span == L0 at bias 0x400000, byte-exact on rounded size (0x747c000 =
  rp(0x7479b1c)) and prot — the one PROVEN segment correspondence, and it validates the two
  code-anchor runtime values above.
- Not consistent: no PT_LOAD is ~2 GiB, so the READ span is a foreign reservation; it starts
  inside L1's single-bias expected range [0x787c000,0x82bc000) (same prot, overlap) — a
  single global bias cannot explain the full mapping arrangement.
- Consistent arrangement (hypotheses for v3): L0 at 0x400000 plus L1/L3/L7/L8 mapped
  non-contiguously — in the [0x787c000,0x8018c000) gap (1.88 GiB; needed only 0x47e8000)
  or above 0x100118000 — with expected spans R-- ≈0xa40000, RW- ≈0x1280000, RW- ≈0x4f8000,
  L8 ≈0x2630000; or L1 at the reservation start with per-segment bias 0x7fd10000 and the RW
  segments elsewhere. Either way per-segment correspondence is mandatory. `v2_elf_base_scan`
  found no dynamic section (`chosen_index:-1`) because PT_DYNAMIC lives in L8, far from the
  exec span it scanned — the v3 probe must enumerate all spans and match §2 sizes/prots.

## Bounds honored

Read-only on all captures/spikes/checkpoints; writes confined to this worker dir; no
network/console; no subagents; 24/35 calls. Open item for the team: only L0's runtime
mapping is proven; GUObjectArray's runtime address must wait for a v3 span enumeration
(worker-mapping-recovery is recovering the corrupt span records; `v1_vmmap_page` parts
contain no usable spans in the retained v1/v2 jsonl).
