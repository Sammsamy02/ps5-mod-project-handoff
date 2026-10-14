# SEGMENT-CORRESPONDENCE.md — Townfall eboot (decrypted-eboot.bin)

Worker: `worker-vaddr-semantics` (2026-10-04). Every number below is derived from
retained bytes of the pinned capture or from the survey's own resolver sources.
Capture: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` (re-verified this run),
size 194,241,424 (0xb93e390).

## 1. Container and header facts (parsed this run)

ELF64 little-endian, `e_type = 0xFE10` (SCE ET_SCE_EXEC), `e_machine = 62` (EM_X86_64),
ELF header at file offset 0, `e_phoff = 0x40`, `e_phentsize = 56`, `e_phnum = 14`,
`e_entry = 0x80` (offset-style entry, SCE convention), `e_shoff = 0x6f48a0a8` (≈1.87 GB —
**beyond the 194 MB file**, so the section-header table is unreadable; `e_shnum=58` is stale).
Program headers parsed at file offsets `0x40 .. 0x40+14*56 = 0x358`.

## 2. PT_LOAD table (the correspondence consumers must use)

Page size 16384 (0x4000); every `p_align = 0x4000`. "Rounded" = memsz rounded up to 0x4000.

| ph# | p_flags | p_offset | p_filesz | p_vaddr | p_memsz | vaddr range [vaddr, vaddr+memsz) | file range [offset, offset+filesz) | rounded size | expected mapping prot | anchors in it |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | PF_X (E only) | 0x00004000 | 0x07479b1c | 0x00000000 | 0x07479b1c | [0x0, 0x7479b1c) | [0x4000, 0x747db1c) | 0x747c000 | `--X` | **UGameEngine::Tick 0x2fac8c0**, **StaticConstructObject 0x490aa0**, fname_ctor_call 0x23d590, cfg alloc 0x483b40, cfg free 0x4840b0 |
| 1 | PF_R | 0x07480000 | 0x00a3e918 | 0x0747c000 | 0x00a3e918 | [0x747c000, 0x7eba918) | [0x7480000, 0x7ebe918) | 0xa40000 | `R--` | PT_GNU_EH_FRAME (ph2) 0x747c000+0x19fac (survey eh_frame function ranges) |
| 3 | PF_R\|PF_W | 0x07ec0000 | 0x0127e4e8 | 0x07ebc000 | 0x01280000 | [0x7ebc000, 0x913c000) | [0x7ec0000, 0x913e4e8) | 0x1280000 | `RW-` | PT_TLS (ph5) 0x7ebc000; PT_GNU_RELRO (ph4) covers this span |
| 7 | PF_R\|PF_W | 0x09140000 | 0x001cc5f2 | 0x0913c000 | 0x004f7418 | [0x913c000, 0x9633418) | [0x9140000, 0x930c5f2) | 0x4f8000 | `RW-` (filesz part) + BSS zero-fill (0x32ae26 tail) | **GUObjectArray 0x93ba110** (BSS, in-segment delta 0x27e110), gmalloc_deletefile_candidate 0x9350950 (BSS, delta 0x214950) |
| 8 | 0 (none) | 0x0930f420 | 0x0262cea8 | 0x09633420 | 0x0262cea8 | [0x9633420, 0xbc602c8) | [0x930f420, 0xb93c2c8) | 0x2630000 | loader-decided (p_flags=0; must be at least readable for the dynamic linker) | PT_DYNAMIC (ph10) 0xbc5f278; PT_NOTE ph9 0xbc5f250; symtab/strtab ph12 |

File<->vaddr identity for file-backed bytes of each segment:
`file_offset = p_offset + (vaddr - p_vaddr)`; conversely `vaddr = p_vaddr + (file_offset - p_offset)`.
Bytes at `vaddr >= p_vaddr + p_filesz` (within memsz) are BSS: **no file bytes exist** —
`file_byte_offset` must stay null for them (schema rule holds).

## 3. Verdict: the survey addresses are ELF virtual addresses (p_vaddr space)

0x93ba110 / 0x2fac8c0 / 0x490aa0 are **p_vaddr values**, not file-byte offsets. Proof chain:

1. Derivation code (receipt-hashed, `.superpowers/native-dependency-spike/ue4ss-native-tick-relocation-resolver-1/crate/src/`):
   - `lib.rs elf_load_spans` (the operative path — the section-header path is impossible because
     e_shoff > file size, and `object::File::parse`'s SHF_ALLOC path also labels with sh_addr):
     `let address=ph.p_vaddr(e); ... sections.push(OwnedSection{address,data})` — file bytes at
     `[p_offset, p_offset+p_filesz)` are presented to scanners at address `p_vaddr`.
     It keeps only spans with `p_flags & (PF_R|PF_X) != 0` and only `p_filesz` bytes
     (L0, L1, L3, L7-file-part; L8 skipped).
   - All target math is vaddr arithmetic: `MemoryTrait::rip4` = `address+4+disp32`;
     `object_cfg.rs:22` builds `Span{va: p_vaddr, mem, flags, data}`; `object_cfg.rs:67`
     `let ip=s.va+offset`; `elf_tick.rs:90` `let ip=s.va+at as u64`; `elf_tick.rs:85`
     `scan_pattern(..., s.va as usize, s.bytes)`.
2. Byte-level anchor verification against the capture:
   - UGameEngine::Tick 0x2fac8c0 -> file 0x4000+0x2fac8c0 = **0x2fb08c0**: bytes
     `55 48 89 E5 41 57 41 56 41 55 41 54 53 48 81 EC 88 00 00 00 4C 8B 2D ...`
     (push rbp; mov rbp,rsp; push r15/r14/r13/r12/rbx; sub rsp,0x88; mov r13,[rip+...]) — a
     textbook x86-64 prologue. Reading the same numeric value as a RAW file offset
     (0x2fac8c0) yields `... 38 03 DE AB 33` — mid-instruction/FPU garbage.
   - StaticConstructObject 0x490aa0 -> file **0x494aa0**: `55 48 89 E5 41 57 41 56 41 55 41 54 53
     48 81 EC 58 02 00 00 48 8B 05 ...` — full prologue. Raw offset 0x490aa0 gives
     `FF 48 89 95 ...` — mid-instruction.
   - GUObjectArray 0x93ba110 independently re-derived this run: scanning L0 file bytes with
     `file = vaddr + 0x4000`, 148 `lea rdi,[rip+disp32]` sites target 0x93ba110; sites
     0x373692 and 0x37372a are each followed within 0x11 bytes by `call 0x483b40` /
     `call 0x4840b0` — exactly the survey's CFG evidence (`alloc=0x483b40 free=0x4840b0 flags=63`).
3. Segment containment: 0x2fac8c0 and 0x490aa0 ∈ L0 (X); 0x93ba110 ∈ L7 (RW) — a global
   object array in a writable segment's BSS. Under the file-offset interpretation
   0x93ba110 would land in L8's file range, whose p_flags=0 segment cannot host the
   writable FUObjectArray; that interpretation also contradicts the derivation code above.
4. Checkpoint agreement: `worker-2-layout-facts` schema defines `elf_image_vaddr` as
   "section vaddr / PT_LOAD p_vaddr space. The only address kind the static survey produces"
   — consistent with this proof. This run supplies the previously-missing program-header
   derivation, so `file_byte_offset` MAY now be filled (with this artifact cited) for
   file-backed anchors: Tick 0x2fb08c0, SCO 0x494aa0; it must remain null for GUObjectArray (BSS).

## 4. Runtime translation

Generic (per-segment correspondence — REQUIRED given current evidence):

```
runtime_address(V) = mapped_start(seg) + (V - seg.p_vaddr)
  where seg is the unique PT_LOAD with seg.p_vaddr <= V < seg.p_vaddr + seg.p_memsz
```
BSS anchors use the identical formula (they are inside memsz; their pages are zero-filled
then written by runtime initializers).

Single-bias special case (only if the probe proves all segments share one bias B,
i.e. every mapped_start == B + p_vaddr): `runtime_address(V) = B + V`.

Worked examples:

| Anchor | vaddr | seg | in-segment delta (V - p_vaddr) | file offset | runtime formula | value under proven L0 mapping |
|---|---|---|---|---|---|---|
| UGameEngine::Tick | 0x2fac8c0 | L0 | 0x2fac8c0 | 0x2fb08c0 | `mapped_start(L0) + 0x2fac8c0` | `0x400000+0x2fac8c0` = **0x33ac8c0** (falls inside observed EXEC span — validated) |
| StaticConstructObject | 0x490aa0 | L0 | 0x490aa0 | 0x494aa0 | `mapped_start(L0) + 0x490aa0` | `0x400000+0x490aa0` = **0x890aa0** (inside observed EXEC span — validated) |
| GUObjectArray | 0x93ba110 | L7 | 0x27e110 | none (BSS) | `mapped_start(L7) + 0x27e110` | NOT derivable today — L7's mapped_start is unknown (see 5). Blind single-bias guess would give 0x97ba110; UNVALIDATED. |

For the supplier adapter: implement the generic formula over the table in section 2;
never assume `bias = mapped_start(L0)` applies to L7 unless the probe proves it.

## 5. Cross-check against the v2 in-game VMMAP early spans

Observed (only two clean entries; both records' remaining entries are corrupt zero/negative
garbage — `worker-mapping-recovery` is recovering them; `v1_vmmap_page` parts in both jsonl
files contain no valid spans):

| observed span | prot | size |
|---|---|---|
| [0x400000, 0x787c000) | `--X` | 0x747c000 |
| [0x8018c000, 0x100118000) | `R--` | 0x7ff8c000 (~2.00 GiB) |

- **CONSISTENT (proven correspondence):** observed EXEC span == L0 mapped at base 0x400000.
  Size 0x747c000 is byte-exact `rp(L0.memsz 0x7479b1c)` and prot `--X` matches PF_X-only.
  Hence `bias(L0) = 0x400000` and the two L0 anchors' runtime values in section 4 are real.
- **NOT consistent with a single global bias:** under bias 0x400000, L1 (R--) would occupy
  [0x787c000, 0x82bc000) and L3 (RW-) [0x82bc000, 0x953c000). The observed R-- span starts at
  0x8018c000 — inside L1's expected range with the same prot (overlap), and no ~2 GiB PT_LOAD
  exists (largest is L8, 38.2 MiB), so the R-- span cannot be an eboot segment; it is a foreign
  ~2 GiB reservation. Overlapping vm entries with differing prot (L3 RW- under the single-bias
  guess vs the R-- reservation's continuation) are impossible, so the single-bias model cannot
  explain the whole arrangement.
- **What WOULD be consistent** (hypotheses for the v3 probe, in preference order):
  1. L0 at 0x400000 (proven); L1/L3/L7/L8 mapped NON-contiguously — either inside the gap
     [0x787c000, 0x8018c000) (size ~1.88 GiB; the four segments need only 0x47e8000 ≈ 71.7 MiB
     rounded: L1 0xa40000 + L3 0x1280000 + L7 0x4f8000 + L8 0x2630000) or above 0x100118000.
     Expected observable spans, wherever they are: one `R--` ≈0xa40000, one `RW-` ≈0x1280000,
     one `RW-` ≈0x4f8000, and an L8 span ≈0x2630000 with loader-decided prot.
  2. L1 coincident with the start of the big R-- reservation (mapped_start(L1)=0x8018c000,
     i.e. per-segment bias 0x7fd10000 for L1 only) — then L3/L7/L8 CANNOT keep vaddr deltas
     (they would land inside the reservation with RW-) and must be mapped elsewhere; per-segment
     correspondence again mandatory.
- **Probe implication:** `v2_elf_base_scan` failed to choose a base ("no candidate image parsed
  a dynamic section", `chosen_index:-1`) precisely because it only considered RX span starts and
  spans adjacent to exec spans; PT_DYNAMIC (0xbc5f278, in L8) is far from L0. The v3 probe should
  enumerate ALL spans, match each against section-2 sizes/prots (16 KiB granularity), then apply
  the section-4 formula per segment.
- **Page-sharing caveat (L7/L8 boundary):** L7 ends at vaddr 0x9633418 and L8 starts at
  0x9633420 — both inside page 0x9633000. A loader that page-rounds cannot map them with
  independent protections on that shared page; expect L8's mapped start at 0x9634000
  (i.e. `mapped_start(L8) = mapped_start(L7) + 0x4f8000` under contiguous placement), with the
  first 0xbe0 bytes of L8's page effectively absorbed into L7's last page. Do not treat an
  observed L8 start of `mapped_start(L7)+0x4f7418` as a mismatch by itself.
