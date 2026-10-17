# REPORT — worker-gmalloc-derivation (2026-10-04)

Status: **DELIVERED (offline derivation only; no console contact, no nested subagents,
no frozen/wiring file modified).** CLAIM.json written first; this report last.
Workspace: `outputs/2026-10-04-zcode-team/worker-gmalloc-derivation/`. ~20 tool calls.

## Verdict up front

**Slot 3 (GMalloc) is derivable — a new route the seed worker's exhausted anchors did
not cover — and is SEEDABLE NOW behind its data-side validator.**

| field | value |
|---|---|
| GMalloc global (seed value) | linked **0x930add8** → runtime **0x970add8** (phdr-7 BSS, 8-aligned) |
| FMallocBinned3 instance global | linked 0x93089b8 → runtime 0x97089b8 |
| FMallocBinned3 vtable | linked 0x7ec26e0 → runtime 0x82c26e0 (phdr-3 RELRO, readable in-game) |
| confidence | derived-static-strong; in-game unvalidated until the validator runs |
| verdict class | **launch-10 probe needed for the validator, then seedable** — NOT underivable, and NOT GMalloc-optional (see below) |

## The new route (why this succeeds where the seed worker's anchors died)

The seed worker's routes were string-idiom and call-shape routes targeted at *finding
the allocator by its code*. All were measured dead (0/14 patterns; both UTF-8 anchors
absent; DeleteFile route reaches only the stack-guard slot 0x9131f08). I used a
different closure — **identify the concrete allocator class, then walk structure to
the global**:

1. **Class anchors** — 4 UTF-16 `"FMallocBinned3"` literals exist (this is a UE5.6
   image; MallocBinned3 is the console allocator). Exact lea xrefs give the
   Malloc/Realloc/Free validation functions (roots 0xb2440/0xb2160/0xaf4e0/0xb1440).
2. **RELA reconstruction** — the decisive step. The file image stores NO absolute
   pointers in data (PS5 RELA model: 1,661,599 relative relocs at file 0x932F430),
   which is why raw vtable scans find nothing. Reconstructing `.rela.dyn` yields
   21,445 code-valued arrays; the one at linked 0x7ec26e0 contains 0xb1440 — the
   FMallocBinned3 vtable ([0]=complete dtor 0xb1070, [1]=deleting dtor 0xb1440,
   byte-verified: D0 calls D1 at 0xb1449).
3. **Ctor** — exactly 2 lea-to-vtable sites: the ctor 0xafd40 (stores the derived
   vtable into `[rdi]` after inlined base init) and the dtor.
4. **Factory** — function 0xb3f0 constructs the instance **in place as a global**
   (`lea rbx,[rip]->0x93089b8; call 0xafd40`) and carries MallocBinned3's
   distinctive address-space constants (movabs 0xc000000000/0x1000000000/
   0xbfc0000000/0x7fc0000000, pool params 0x12c00000/0x10000) — independent
   flavor corroboration.
5. **Bootstrap store** — `0xb660: mov [rip+0x92ff771], rbx` → **0x930add8**. Exactly
   ONE store image-wide (89-form and imm32 C7-form both checked).
6. **Consumers** — exactly 11 rip-relative loads, 0 lea; 10 decode as pure FMalloc
   virtual dispatch (`mov rdi,[rip+GM]; mov rax,[rdi]; call [rax+0x30|0x40|0x50]`)
   in 11 distinct FMemory thunk functions. The classic GMalloc fingerprint:
   one writer at init, many virtual-dispatch readers.

Because the instance is a global, at runtime `[0x970add8]` must equal `0x97089b8`
exactly and `[0x97089b8]` must equal `0x82c26e0` — a two-qword data-side validator,
code-read-free (V1 in phdr-7 BSS, same class as the proven GUA guard; V2 in RELRO
with the FC-1 mprotect-first discipline). Full details and the V4 negative control
are in `GMALLOC-CANDIDATES.json`. Methodology sanity: my stage-1 census reproduced
the seed worker's GUA count of exactly 1565 lea refs.

## Honest disclosures

- My automated shape census (stages 3/5) reported 0 targets due to an operator bug
  (compared `ins.address+ins.size` against the rip-relative *target* instead of
  re-deriving the disp — every valid site was skipped). Disclosed in the candidates
  file; not load-bearing — the derivation and the consumer census were done by direct
  per-site regex+capstone verification (`gmalloc_verify_refs.json`,
  `gmalloc_ref_sites.json`).
- Route (c) GUA-adjacency was measured and rejected with data: hot slots near GUA
  are GUObjectArray-family; the derived GMalloc sits 0xAF338 below GUA in a different
  TU cluster. Route (d): "FPageAllocator"/"FMemStack"/"TLsaAllocStack"/
  "GMallocCreate" strings are absent from this image; the flavor evidence is the
  FMallocBinned3 anchors + MallocBinned3 constants instead.

## Is a null-GMalloc port path viable?

**No, not as staged.** The port's only GMalloc consumer is
`PostInitialize` (staged `UnrealInitializer.cpp:387-392`): `if (!GMalloc) throw`
then one dereference for a log line. With slot 3 absent, the seed-table proposal
gates `config.gmalloc=false` and the loop is skipped, so the run ends at the loud
`"PostInitialize: GMalloc is uninitialized."` throw by design. Making the port run
GMalloc-optional would require editing that consumer (and accepting that every
engine allocation the probes trigger later goes through the 11 thunks above) — a
consumer-semantics change outside this worker's scope. With the seed armed behind
its validator, the question is moot: V1+V2 pass → seed `0x970add8`; any failure →
today's loud-throw behavior unchanged.

## What was NOT done / non-claims

- No in-game execution of anything; the validator is DESIGN ONLY. Until it runs, the
  candidate keeps the "identity and ABI unverified" provenance class, same as seeds 2/4.
- No staged-file edits; the integration shape (one constexpr + one assignment + the
  Diff-4-style guard) is a proposal that lands exactly where SEED-INTEGRATION.md
  reserved it.
- No claim that 0x930add8 generalizes beyond this sha-pinned image
  (3b6cfad4…622fc); the chain is reproducible from the committed scripts.
- All reads offline against the retained image; no unrelated files touched.

## Artifacts

`CLAIM.json`, `GMALLOC-CANDIDATES.json`, `REPORT.md`, `derive_gmalloc.py`,
`derive2_gmalloc.py`, `derive3_gmalloc.py`, `derive4_gmalloc.py`,
`derive5_gmalloc.py`, `gmalloc_stage1-5.json`, `gmalloc_verify_refs.json`,
`gmalloc_ref_sites.json`.
