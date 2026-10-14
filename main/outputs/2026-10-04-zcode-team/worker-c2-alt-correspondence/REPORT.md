# REPORT — worker-c2-alt-correspondence (team 2026-10-04)

Deliverables in this directory: `CLAIM.json` (written first), `C2-ALTERNATIVE-SPEC.md`
(the C2(b) substitute design + evaluation rules), `candidate-ranges.json`
(machine-readable candidates + audit), this `REPORT.md` (written last).

Scope honored: offline only; no console, no network, no nested agents; writes
confined to this directory; the retained capture and all other outputs read-only.

## PART 1 — Candidate data segments (done, evidence-backed)

Program headers re-parsed from the retained capture (ELF64 x86-64, type 65040,
14 phdrs): 5 LOADs — exec 0x0–0x7479b1c (flags X), rodata 0x747c000–0x7eba918
(R), RELRO RW 0x7ebc000 (filesz 0x127e4e8), .data+BSS 0x913c000 (filesz
0x1cc5f2 / memsz 0x4f7418), aux file-backed 0x9633420–0xbc602c8 (flags 0x0),
plus PT_NOTE (build-id at file 0xB93B250, vaddr 0xBC5F250) and PT_DYNAMIC.
The on-disk shoff points beyond EOF — sections unusable; structure taken from
program headers + PT_DYNAMIC, all offsets byte-verified.

Relocation audit (the load-bearing exclusion): PT_DYNAMIC yields DT_RELA
(1,661,599 entries) + DT_JMPREL (1,997). All 1,663,596 r_offsets were scanned
from the capture: 100% fall in the RW/RELRO segments (phdr 3 and phdr 7 .data);
**zero in rodata, zero in the aux segment**. RELATIVE count (1,649,556) equals
DT_RELACOUNT exactly. Therefore non-relocated file-backed bytes in phdr 1/8
must be byte-identical live.

5 candidates selected (file offset ↔ vaddr ↔ rt = base 0x400000 + vaddr):
A GNU build-id note 36 B (unique), B L"UE5" 8 B (unique, pointed-to by the
verified accessor stub), C UTF-16 "BuildIdOverride" 32 B (2 occurrences, same
text; recorded), D "Current Changelist\0" 19 B (unique), E AES S-box 64 B
(unique). All in phdr 1/8, all non-zero, all immutable, none relocation targets.
Full table with expected hex in `C2-ALTERNATIVE-SPEC.md` §5 and
`candidate-ranges.json`.

Build-ID note readability: runtime address would be 0xC05F250. It lies in the
flags-0 LOAD; the loader itself must read that segment (its RELA/SYMTAB/STRTAB
live there), so readable is the only workable mapping — but no saved map
evidence covers it, so candidate A is gated on the same-invocation mapscan and
read last. (Also flagged: IDENTITY-RESOLUTION.md §4 summarizes the note header
as "namesz=3"; the bytes read namesz=4 — standard "GNU\0". Descriptor and all
other fields match that document exactly; raw bytes are authoritative.)

## PART 2 — Existing live data (enumerated; answer: NO)

Every live record in igf6-results.jsonl was enumerated (25 `exp` values across
6 builds; exactly ONE `"hex":"` dump exists). The igf4 GUA read (96 B @ rt
0x97ba110, prot 0x3) sits entirely in phdr 7 BSS (vaddr 0x93ba110–0x93ba170 >
filesz end 0x93085f2) — not file-backed, game-mutated, and written+restored by
igf4's own writeback. igf6's 3 records contain no byte dumps and its trusted
8685-span readable list was counted but NOT persisted. igf4's Tick/SCO anchor
records were refusals (no bytes); igf2's ELF-base scan parsed nothing.

**No existing record contains live bytes from any file-backed data segment.
The comparison CANNOT be evaluated from existing data — one new probe launch
is required.** The exact reads (8/32/19/64/36 B at 0x7C26782, 0x79CFFDA,
0x7F8845D, 0x81D2A40, 0xC05F250, order B→C→D→E→A, gated on the same
invocation's persisted mapscan) are specified in SPEC §7.

## PART 3 — C2-ALTERNATIVE-SPEC.md (written)

Covers: equivalence definition (unique-in-file data beats the shared 16-byte
code prologue in discriminative power; inference chain made explicit), the
geometry/relocation evidence, comparison method, PASS = 5/5 (bounded pass:
A+B+≥2 of C/D/E, zero mismatches), FAIL = any mismatch (contradicting
evidence, not weak evidence) or base ≠ 0x400000, exclusions (relocs,
runtime-modified, BSS/GUA, loader metadata, probe self-copies — the probe
must not embed expected bytes; comparison is offline), and probe safety
discipline (map-then-gate from the same invocation, write-through records,
zero exec derefs, zero mprotect/syscall, refusals recorded, A read last).

## PART 4 — Evaluation from existing data

Not possible (PART 2). No comparison number is claimed. This is the honest
C2-alternative status: the check is fully specified and its expected bytes are
precomputed and pinned to the capture sha256; the outcome requires exactly one
probe launch.

## Standing honesty bounds

- Tick/SCO correspondence remains UNRESOLVED; this check never reads their
  bytes and only establishes image-level correspondence, whose acceptance as
  the C2(b) substitute is the controller's explicit act. No >8/AAA/completion
  claims; nothing was executed; the igf6 crash records stand unchanged.
- Residual uncertainty stated in the spec: no saved evidence shows a
  successful deref of a prot-1 or flags-0 span (igf4's success was prot-3);
  a first-data-read crash would be a new fact class, not an igf6 replay.
