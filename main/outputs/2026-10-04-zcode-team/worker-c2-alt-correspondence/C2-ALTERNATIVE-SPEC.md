# C2-ALTERNATIVE-SPEC — data-segment correspondence check for promotion criterion C2(b)

Worker: `worker-c2-alt-correspondence`, team 2026-10-04-zcode-team. Offline only:
no console, no network, no engine execution. Every number below was recomputed
directly from the retained capture or quoted from a cited record.

Retained image: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(194,241,424 bytes, sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`).
Live-record evidence: `outputs/2026-10-04-zcode-controller/console/igf6-results.jsonl`
(builds igf-f4a91d8123b0, igf2, igf3, igf4-4891869337b4, igf5, igf6-ad9bda1fc50a).

---

## 1. What this check is (and what it replaces)

C2(a) — direct 16-byte code-anchor reads at Tick/SCO — is DEAD ON THIS KERNEL:
igf6 (`igf6-ad9bda1fc50a`) emitted exactly 3 write-through records (entry,
startup, mapscan) and crashed on record 4, the first exec-span byte read at
Tick `rt 0x33ac8c0`. Reading exec-only (`prot 0x4`) mapped pages faults on the
PS5 regardless of the "NX gates exec, not read" rationale.

C2(b) substitute defined here: **an independent correspondence check that
verifies the live eboot mapping is the byte-identical image of the retained
capture using distinctive, immutable, file-backed DATA bytes** — never code
bytes — read only from spans proven readable by the same invocation's own
memory-map scan.

### 1.1 Definition of equivalence (why this is as rigorous as code-anchor comparison)

A code-anchor match at Tick would establish: "the live process's image at
base+vaddr is the retained file, therefore the file's Tick bytes are the live
Tick bytes." The inference chain is:

```
file bytes == live bytes at (base + vaddr)   for distinctive immutable content
        +  the address is inside a file-backed LOAD segment mapped from this file
        +  nothing rewrites non-relocated file-backed bytes after load
        ⟹  live image == retained image (at the granularity of the tested pages)
        ⟹  Tick/SCO bytes at their precomputed addresses are the file's bytes
```

The data check tests the same chain at more points, with BETTER
discriminative power than the code anchors had:

- The code anchors' 16-byte prologue (`554889e54157415641554154534881ec`) is a
  COMMON pattern — the same 16 bytes were expected at BOTH Tick and SCO, and
  EXPECTED-BYTES.json lists a third identical prefix at the FName call-site
  candidate. A prologue match has intrinsic false-positive mass.
- Every candidate below is **unique in the entire 194,241,424-byte file**
  (verified by full-file search, section 5) — a match has effectively zero
  false-positive probability, and a wrong-base or wrong-image live process
  would fail at least the spread of candidates across two different LOAD
  segments and ~72 MB of address distance.
- What the data check does NOT do: it does not observe Tick/SCO instruction
  bytes directly. Section 9 states the residual limits of that inference.

## 2. Load geometry — file vs live (all verified, none assumed)

ELF64 LSB, machine 62 (x86-64), e_type 65040 (0xFE10 Orbis/PS5), e_phnum 14
(e_phoff 0x40). The on-disk section header pointer (0x6f48a0a8) points beyond
EOF — sections are not usable in this capture; all structure below is from
program headers and PT_DYNAMIC.

| phdr | type | flags | file offset | vaddr | filesz | memsz | role |
|---|---|---|---|---|---|---|---|
| 0 | LOAD | 0x1 (X) | 0x4000 | 0x0 | 0x7479b1c | 0x7479b1c | code (exec-only live) |
| 1 | LOAD | 0x4 (R) | 0x7480000 | 0x747c000 | 0xa3e918 | 0xa3e918 | **rodata — candidate source** |
| 2 | GNU_EH_FRAME | 0x4 | 0x7480000 | 0x747c000 | 0x19fac | — | (inside phdr 1) |
| 3 | LOAD | 0x6 (RW) | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 | 0x1280000 | RELRO-covered reloc targets |
| 4 | GNU_RELRO | 0x4 | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 | 0x1280000 | (covers phdr 3) |
| 5 | TLS | 0x4 | 0x7ec0000 | 0x7ebc000 | 0x21c | 0x13b1 | per-thread, excluded |
| 6 | 0x61000001 | 0x4 | 0x913e488 | 0x913a488 | 0x60 | 0x60 | PS5 aux (vaddr = DT_32 value) |
| 7 | LOAD | 0x6 (RW) | 0x9140000 | 0x913c000 | 0x1cc5f2 | 0x4f7418 | .data + BSS tail (GUA lives in BSS) |
| 8 | LOAD | **0x0** | 0x930f420 | 0x9633420 | 0x262cea8 | 0x262cea8 | **aux file-backed — build-id note** |
| 9 | NOTE | 0x0 | 0xb93b250 | 0xbc5f250 | 0x24 | 0x24 | NT_GNU_BUILD_ID |
| 10 | DYNAMIC | 0x6 | 0xb93b278 | 0xbc5f278 | 0x1050 | 0x1050 | dynamic array |
| 11–13 | misc | — | — | 0x0 | — | — | excluded |

File↔vaddr deltas: phdr 0/1/3/7: `file = vaddr + 0x4000`; phdr 8/9:
`file = vaddr − 0x324000` (worker-layout-identity IDENTITY-RESOLUTION.md §7
table, re-derived here; note that document's phdr numbering "LOAD 5" = phdr 8
here).

Live correspondences already recorded (same source, igf6-results.jsonl):

- **Exec span** `0x400000–0x787c000 prot 0x4` (igf/igf2 g5_vmmap first entry)
  == phdr 0 mapped at **base 0x400000** (vaddr 0 + 0x7479b1c, page-aligned
  0x747c000, +base). Exact geometry match.
- **GUA read** (igf4 seq6 `gua_read`, rc 0): rt `0x97ba110` = 0x400000 +
  profile_vaddr `0x93ba110`; span `0x970c000–0x9a34000 prot 0x3` == the page
  tail of phdr 7 (memsz end 0x9633418 → page 0x9634000 → rt 0x9a34000).
  Second independent confirmation of base 0x400000.
- **GUA is BSS** (beyond phdr 7 filesz end 0x93085f2): its 96 live bytes have
  NO file backing — they can never serve a file comparison (see §8).
- igf6 mapscan (seq 3): sysctl KERN_PROC_VMMAP trusted (10047 records of
  kve_structsize 144, `insane:0, trusted:1`), **8685 readable spans counted
  but the span list was NOT persisted** — readable-mapping coverage of the
  candidate pages is therefore UNKNOWN in saved evidence and MUST be
  re-verified in the same invocation by any future probe (§7).

## 3. Relocation audit — what is relocation-dependent (all of it, precisely)

PT_DYNAMIC (file 0xb93b278, 0x1050 bytes) gives:
`DT_RELA = 0x9653430` (file 0x932f430), `DT_RELASZ = 0x2607ee8` (39,878,376 B
→ 1,661,599 entries, RELAENT 24), `DT_JMPREL = 0x96478f8` (file 0x93238f8,
`DT_2/PLTRELSZ = 0xbb38` → 1,997 entries), `DT_RELACOUNT = 1,649,556`.

Both tables were fully scanned from the capture (1,663,596 r_offsets total):

| r_offset segment | vaddr range | entries |
|---|---|---|
| phdr 3 (RELRO) | 0x7ebc000–0x913a4e8 | 1,661,610 |
| phdr 7 file part (.data) | 0x913c000–0x93085f2 | 1,986 |
| **phdr 1 (rodata)** | 0x747c000–0x7eba918 | **0** |
| **phdr 8 (aux)** | 0x9633420–0xbc602c8 | **0** |

Types: 8 (R_X86_64_RELATIVE) 1,649,556 — equals DT_RELACOUNT exactly; 1
(R_X86_64_64) 12,022; 7 (JUMP_SLOT) 1,997; 6 (GLOB_DAT) 21.

**Consequence:** no byte in phdr 1 or phdr 8 is a relocation target. After
load, file-backed non-relocated bytes are copied verbatim (no base adjustment
is written into them), so for candidates in those two segments,
`live bytes == file bytes` is the expected invariant.

## 4. Excluded regions (never candidates)

1. **Exec span / phdr 0** (0x0–0x7479b1c → rt 0x400000–0x787c000, prot 0x4):
   reading it crashes the module (igf6 record 4 never arrived). Never read.
2. **All 1,663,596 relocation targets** (phdr 3 and phdr 7 file part): live
   values differ from file by design (relative addend + load base; symbol
   resolutions). Includes .data.rel.ro, .got, PLT slots.
3. **Runtime-modified data:** phdr 7 BSS 0x93085f2–0x9633418 (contains the
   GUA at 0x93ba110 — igf4's 96 live bytes live here; the game mutates them,
   and igf4's own writeback record wrote/restored them); TLS (phdr 5);
   PT_DYNAMIC (linker may write DT_DEBUG).
4. **Loader metadata inside phdr 8** (DT_RELA/JMPREL/SYMTAB/STRTAB/DYNAMIC
   arrays): not needed for identity; some are runtime-touched. Not candidates.
5. **The probe's own copies of expected data:** the probe PRX must NOT embed
   the expected bytes at all (igf4/igf6 embedded `expected_hex` and computed
   `byte_match` on-console — that design is self-referential). The substitute
   probe emits RAW live bytes only; the comparison against the retained
   capture is done OFFLINE by the controller. No self-fulfilling comparison.

## 5. Candidate ranges (5 selected; file bytes byte-verified in this analysis)

Runtime address formula: `rt = 0x400000 + vaddr` (base established twice in
§2; any future probe re-derives base from its own same-invocation map as the
lowest `prot & 4` span start and must record it — it must equal 0x400000 for
the comparison to be valid).

| ID | what | file offset | vaddr | rt (base 0x400000) | len | expected bytes (hex) | uniqueness in file |
|---|---|---|---|---|---|---|---|
| A | NT_GNU_BUILD_ID note record | 0xB93B250 | 0xBC5F250 | 0xC05F250 | 36 | `040000001400000003000000474e550004bb4330a7d71dcefe1cc2c4d19769d500000000` | 1 occurrence |
| B | branch name L"UE5" | 0x782A782 | 0x7826782 | 0x7C26782 | 8 | `5500450035000000` | 1 occurrence |
| C | UTF-16 "BuildIdOverride\0" + 2B | 0x75D3FDA | 0x75CFFDA | 0x79CFFDA | 32 | `4200750069006c006400490064004f0076006500720072006900640065000000` | 2 occurrences* |
| D | ASCII "Current Changelist\0" | 0x7B8C45D | 0x7B8845D | 0x7F8845D | 19 | `43757272656e74204368616e67656c69737400` | 1 occurrence |
| E | AES S-box (64 B) | 0x7DD6A40 | 0x7DD2A40 | 0x81D2A40 | 64 | `637c777bf26b6fc53001672bfed7ab76ca82c97dfa5947f0add4a2af9ca472c0b7fd9326363ff7cc34a5e5f171d8311504c723c31896059a071280e2eb27b275` | 1 occurrence |

\* C's second occurrence (file 0x76CCE58) is the same byte window inside the
longer `bUseBuildIdOverride` string — both are the same text; the comparison
is at a fixed address, so non-uniqueness of the text does not weaken it, but
it is recorded for honesty.

Why each qualifies:

- **A — link identity.** The 20-byte descriptor `04bb4330a7d71dcefe1cc2c4d19769d5`
  is the ELF GNU build-id (worker-layout-identity §4). Unique per link,
  maximally distinctive, immutable (a note; no relocs — §3), in phdr 8.
  Corroboration: descriptor hex matches IDENTITY-RESOLUTION.md exactly.
  Honesty note: that document's §4 summary describes the record as
  "namesz=3"; the bytes at p_offset 0xB93B250 read namesz dword = 4 (standard
  NT_GNU_BUILD_ID: "GNU\0" = 4 name bytes). The record length (36 B),
  descsz (20), type (3), name and descriptor all match that document; the
  raw bytes quoted here are authoritative and re-readable from the capture.
- **B — build-specific string reached by verified code.** The branch accessor
  stub at vaddr 0xF0 (`lea rax,[rip+0x7826687]`) resolves exactly to this
  vaddr (IDENTITY-RESOLUTION.md §3) — the file's own code points here, so a
  live match also re-validates that static decode.
- **C, D — build-specific UE strings** in phdr 1 rodata (CVar/config
  machinery label; FEngineVersion dump label). Immutable (read-only segment,
  zero reloc targets), distinctive (no zeros-heavy or alignment patterns).
- **E — page-integrity constant.** Canonical AES S-box; high per-byte entropy,
  unique 64-B window; proves rodata content integrity deep in the segment
  (rt 0x81D2A40 is ~2.9 MB past C and ~2.3 MB before the segment end page).
  Not build-identifying by itself — it is the spread/guard candidate.

Spread argument: A sits in phdr 8 (different LOAD, different file-delta),
B/C/D/E in phdr 1; candidate addresses span 0x79CFFDA–0xC05F250 (~72 MB of
runtime address distance). A wrong base, wrong slide, or different image
cannot satisfy all five.

Readable-mapping status per candidate: B/C/D/E are in a `PF_R` LOAD — the
loader's contract is a readable mapping. A is in the flags-0 phdr 8 LOAD;
the loader must read this segment itself (its own RELA/SYMTAB/STRTAB live
there), so a readable mapping is the only workable policy — but the raw
phdr flags are 0x0 and NO saved map evidence covers rt 0xC05F250. A is
therefore gated and read LAST (§7). EXPECTED-BYTES.json's rendering of this
segment as "R--" is that document's interpretation, not a phdr fact; the
same-invocation map is the only accepted evidence of its live protection.

## 6. Comparison method

1. Offline (already done): expected bytes read from the retained capture at
   the file offsets in §5; capture sha256 pinned; this document is the
   expected-bytes evidence for the alternative check.
2. On-console (future probe): read raw live bytes at each rt address, emit
   records containing `rt_addr, vaddr, file_off, len, live_hex,
   span_start, span_end, span_prot` — span fields taken from the SAME
   invocation's mapscan (§7). No expected bytes in the binary; no on-console
   comparison; no byte_match field.
3. Offline (controller): `live_hex` vs the pinned capture's bytes at
   `file_off`, per candidate, after re-verifying the capture sha256 and that
   `rt_addr − vaddr == recorded base == 0x400000` and each record's span
   shows `prot & 1`.

## 7. Probe requirements (next launch only; this worker ran nothing)

Sequence (write-through synchronous records, igf4/igf6 record_sink discipline;
ordering is load-bearing for crash localization):

1. `seq1` entry marker.
2. `seq2` startup (post-init_array).
3. `seq3` mapscan: sysctl `{CTL_KERN,KERN_PROC_VMMAP,getpid()}` with the
   v3 structsize-respecting 144-byte walk (`kve_structsize==144` validated,
   igf6-proven: trusted=1, insane=0). **Persist the readable span list**
   (paged records) — this closes the igf6 gap where 8685 spans were counted
   but not saved. Derive and record `base := lowest prot&4 span start`
   (expected 0x400000) and the exec span extent (expected 0x747c000 bytes,
   == phdr 0 filesz page-rounded) as geometry cross-checks.
4. `seq4..seq8` candidate reads **in order B, C, D, E, A** (A last). For each:
   gate FIRST on the same-invocation map: covered by a span with
   `prot & 1`; else emit a refusal record (`refusal: covered_not_readable` or
   `uncovered`) and continue — never deref. If gated: read the §5 length at
   rt, emit raw `live_hex` + span fields. Read sizes ≤ 64 B; no loops, no
   scans, no writes, no mprotect, no syscall instruction anywhere (igf5/igf6
   lessons), zero exec-span dereferences.
5. `seq9` summary: base, per-candidate gated/refused/mismatch-possible flags,
   `exec_reads_attempted: 0`, write-failure count.

Hard rules inherited from the evidence: the map gates the deref (igf4's gate
refused prot-4 anchors and the module survived; igf6 ungated the same read
and died); every refusal is recorded; no permission state is ever altered.

Residual risk, stated: no saved evidence shows a successful deref of a
prot-1 (read-only) or flags-0 aux span (igf4's success was prot-3). If the
kernel faults on prot-1 reads despite the map, the crash localizes by record
absence exactly as igf6's did, with B first and A last minimizing loss; the
interpretation matrix treats "died at first data read" as a distinct outcome
(data-read faults are a NEW fact class, not an igf6 replay).

## 8. Evaluation against EXISTING live data — result: NOT POSSIBLE (new launch required)

All live evidence in saved records was enumerated (25 distinct `exp` values,
every `"hex":"` dump = exactly 1):

- igf4 `gua_read`: the ONLY successful byte dump — 96 B at rt 0x97ba110,
  span prot 0x3. Entire window (vaddr 0x93ba110–0x93ba170) lies beyond phdr 7
  filesz (0x93085f2): **BSS, not file-backed, runtime-written, and
  probe-written+restored by igf4's writeback. Zero overlap with any
  candidate. Cannot participate in a file comparison by definition.**
- igf4 `tick_anchor`/`sco_anchor`: rc 3, `refusal: covered_not_readable` —
  no bytes.
- igf6 (all 3 records): markers + mapscan counters only; no byte dumps;
  span list not persisted.
- igf2 `v2_elf_base_scan`: `chosen_index: -1`, no image parsed — no bytes.
- igf3's 1035 records are vmmap page dumps (map data, not candidate-span
  byte reads).

**Answer to PART 2: NO — no existing record contains live bytes from any
file-backed data segment (phdr 1 or phdr 8). The C2 alternative comparison
therefore CANNOT be evaluated from existing data; it requires exactly one new
probe launch per §7.** Exact reads required: B 8 B @ 0x7C26782, C 32 B @
0x79CFFDA, D 19 B @ 0x7F8845D, E 64 B @ 0x81D2A40, A 36 B @ 0xC05F250 —
each gated on the same-invocation mapscan.

## 9. PASS / FAIL / UNRESOLVED

- **PASS (substitute satisfied, recommend controller acceptance):** all five
  candidates gated-and-read, offline comparison `live_hex == file bytes` for
  **5/5**, base == 0x400000, every span field `prot & 1` from the same
  invocation. Minimum acceptable PASS if refusals occur (each justified by
  persisted same-invocation map evidence): **A and B both match, ≥ 2 of
  {C, D, E} match, and mismatches = 0**. A matching B without A is NOT a
  pass (the build-id note is the only candidate carrying link identity).
- **FAIL (substitute refuted; C2(b) cannot be satisfied this way):** any
  mismatch (`live_hex != file bytes`) on any read candidate — a mismatch is
  not missing evidence, it is CONTRADICTING evidence that the live module is
  not the retained image; promotion stops. Also FAIL-equivalent: recorded
  base ≠ 0x400000, or the probe dies at its first DATA read (new fact class —
  re-arbitrate before any reuse of this design).
- **Refusals** (map says not readable): neither pass nor fail; recorded; the
  PASS matrix above decides what remains achievable.
- **Tick/SCO status: UNRESOLVED.** This check never reads Tick
  (`0x2fac8c0`/rt 0x6fac8c0) or StaticConstructObject (`0x490aa0`/rt 0x890aa0)
  bytes. What PASS establishes is image-level correspondence, from which
  Tick/SCO correspondence FOLLOWS ONLY BY INFERENCE (file-backed exec pages
  are not relocation targets — §3 — and nothing in evidence writes to
  prot-4 pages; the sole exec-page deref attempt, igf6, wrote nothing before
  crashing). Whether that inference satisfies C2(b) "equivalent Tick/SCO
  correspondence" is the controller's explicit, named, signed acceptance per
  PROMOTION-DECISION.md §2 C2(b) — this spec does not claim it, does not
  re-rate any unchanged evidence, and does not touch the deferred signature.
  Tick/SCO remain UNRESOLVED in every artifact until that explicit
  controller act, and the direct-read C2(a) path stays dead per igf6.

## 10. Evidence index

- Capture + sha256: `capture.json` (same directory as the .bin).
- Program headers, PT_DYNAMIC, relocation scan, candidate bytes/uniqueness:
  recomputed for this document from the capture (methods in §2/§3/§5).
- Live records: `outputs/2026-10-04-zcode-controller/console/igf6-results.jsonl`
  (igf4 seq4/5/6/7/8/9/10; igf6 seq1/2/3; igf seq5 g5_vmmap; igf2 seq52).
- Mapping semantics/stride repair: worker-span-arbitration `VERDICT.md`;
  worker-mapping-recovery `ANALYSIS.md`/`vmmap-v3-records.jsonl`.
- Identity chain for A and B: worker-layout-identity `IDENTITY-RESOLUTION.md`
  §3/§4/§7 (with the namesz-summary discrepancy noted in §5 above).
- Anchor provenance being substituted-for: worker-expected-bytes
  `EXPECTED-BYTES.json` (Tick 0x2fac8c0, StaticConstructObject 0x490aa0,
  file = vaddr + 0x4000); worker-code-anchor-reads `REPORT.md`;
  PROMOTION-DECISION.md §2 C2(b).
