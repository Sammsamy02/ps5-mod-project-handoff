# REPORT — worker-ingame-probe (team 2026-10-04)

Role: build the decisive in-game experiment — a PRX module collecting loader
facts and running layout predicates inside the RUNNING Townfall process,
BEFORE any UE4SS initialization.

Status: MODULE BUILT AND LOCALLY VERIFIED. NOT RUN IN-GAME. No game launch
occurred, no console contact occurred, no in-game results are claimed. The
candidate awaits the user-coordinated launch in STAGE-PLAN.md.

## Deliverables

- `src/` — the module (C core + vendored predicates + boundary/import asm).
- `build_ingame.py` — pinned-toolchain build; produces a fresh `build-<id>/`,
  never overwrites frozen history; writes a full receipt.
- `build-f4a91d8123b0/` — candidate `modbridge_ingame_facts.prx`
  (32,077 bytes, sha256 `db8ec5505f6eb58fc26b8f6473ffbcc9ed03e86167e8aef36712acd83fdf5991`),
  artifact-checks.json, import-contract.json, package/build receipts.
- `STAGE-PLAN.md` — exact controller staging steps.
- `CLAIM.json` — claim recorded before any work.

## Design decisions

1. Packaging recipe replicated from the proven lineage
   (`native-full-runtime-aligned-entry-1`, checkpoint `native_aligned_entry`,
   receipts in `build-1/package-commands.json`): prospero-clang freestanding
   objects (identical flag set) -> pinned ld.lld -r (import_references.o
   FIRST so merged undefined symbols keep the .S FUNC types; this toolchain
   emits NOTYPE from C objects) -> derived C# Packager on the frozen
   SharpProspero assemblies -> frozen `finalize_sdk.py` SDK 0x11000000 stamp
   -> pinned SDK `make_fself.py` wrap -> local checks. The frozen lineage PRX
   hash was verified read-only (3b998f09...) before every build; two clean
   rebuilds reproduced the identical output hash (deterministic).
2. Module entry follows the lineage contract exactly: exported
   `module_start`/`module_stop` with `force_align_arg_pointer` (ploader's
   nonstandard caller alignment was the lineage's decisive fix), a single
   entry guard, explicit DT_INIT/DT_FINI with loader-unowned init/fini
   arrays walked by the module itself (bounded 64, sentinel slot proves the
   walk ran; count surfaces in G7).
3. Quiesce (documented per brief): everything runs synchronously inside
   module_start; NO threads are ever created; the log channel is closed;
   module_start returns 0; the module stays loaded but inert; NO unload is
   attempted (sceKernelUnloadProcessModules not imported; the lineage never
   unloads; ploader mode 2 ties lifetime to the game). module_stop only walks
   the fini sentinel.
4. Records: one bounded (<=2 KiB) JSONL line per experiment to
   `/data/modbridge-ingame-facts.jsonl`, marker `modbridge.ingamefacts`
   schema 1 build_id `igf-f4a91d8123b0`, written through the only I/O route
   PROVEN in-game (sceKernelOpen/Write/Close, the lineage diagnostic
   channel's trio). Envelope adapted from worker-liveprobe's probe_record.
   On log-open failure module_start returns -22 (lineage precedent): an
   honest loader-visible failure rather than silent nothing.
5. Imports: 12 strong, 4 weak-declared. Seven identities copied VERBATIM from
   the frozen 191-import contract (__error, sceKernelOpen/Write/Close,
   sysctl, getpid, clock_gettime). Five DERIVED as libkernel/libkernel v1
   rows mirroring the frozen sceKernelOpen/sysctl pattern:
   sceKernelGetModuleList/Info, sceKernelGetExecutableModuleHandle, dlopen,
   dlclose. The frozen SharpProspero linker resolved all five to NID-encoded
   dynsym names at package time (12 encoded imports in artifact-checks).
   No libc imports at all: memset/memcpy/strlen are freestanding-local, so
   nothing else can be implicitly pulled in.
6. G2 fault containment (the brief's named risk): the KERN_PROC_VMMAP span
   snapshot is collected FIRST (before G1) and every foreign dereference —
   eboot ELF header/phdrs/dynamic walk, r_debug, each link_map node, each
   l_name string — is validated against readable (prot bit0) spans BEFORE
   dereference, with a dedup/cycle bound (64) and decreasing-length string
   copies; refused dereferences increment `validation_blocked_derefs` and
   stop the walk rather than fault. G6 predicates receive only readable
   spans, so the vendored pure predicates read exclusively validated memory.
7. G6 never guesses the base: candidates are G1 module-info seg0 (preferred),
   G4 eboot-named module seg0, G2 eboot link_map l_addr — each accepted only
   if its first 64 bytes are span-readable; otherwise the record says
   `needs-base` and predicates are skipped. All three candidate values plus
   the chosen one and its provenance string are recorded.
8. G4 uses the controller-RESOLVED 3-arg spelling only (4-arg returned
   0x8002003E EINVAL standalone), both size units (4N/N) at capacity 128,
   validates first candidates by the 352-byte echo oracle, scans <=128
   handles for the eboot entry, and reports first-8 module details.
9. G7: `kernel_get_fw_version` is crt1-provided and a PRX links no crt1, so
   it is NOT claimed; sysctl KERN_VERSION is attempted and recorded verbatim
   with that caveat, plus the summary fields.

## Bounds honored

- No network, no console contact, no nested subagents, <=55 tool calls.
- Existing files immutable: everything read-only outside this worker dir;
  frozen PRX hash-checked, never modified; vendored copies are origin-headed
  (layout predicates from worker-5-layout-validator, unchanged logic).
- Credentials (work/nexus-test.*) untouched.
- Honest records: local checks assert only locally verifiable facts; every
  in-game outcome field maps to what the OS actually returned or an explicit
  skip/needs code.

## Risks / open items

- Load-acceptance of the five derived libkernel identities (notably
  dlopen/dlclose) is provable only on console; STAGE-PLAN.md documents the
  fallback (revert to weak dl rows, rebuild; G3 then records dl_route=none).
- DynamicWriter drops unresolved weak symbols (verified: 0 weak dynsym
  entries), so dlsym/__dlopen/__dlclose/__dlsym bind statically to null;
  G3's route field makes this visible rather than assumed.
- G2 depends on G1 producing a validated eboot base; if G1 fails in-game,
  G2 records needs-base (-1) by design (the DT_DEBUG question stays open
  rather than being answered with a guess). Note our own PRX dynamic section
  DOES carry a DT_DEBUG slot (value 0 pre-load) — whether game rtld fills it
  for late-loaded PRXes is observable in a future probe via the G4-located
  own-module base (not in this build; recorded as future work).
- If Townfall's module count exceeds 128, G4 truncates at capacity and says
  so; vmmap spans cap at 1024 with an explicit truncated flag.
- The game's eboot may be `eboot.bin`-named differently in module info;
  G4's eboot match is a recorded heuristic (`contains "eboot"`), with the
  full first-8 table returned for controller judgment.

## Integration notes

- Controller stages per STAGE-PLAN.md; user launches; controller retrieves
  `/data/modbridge-ingame-facts.jsonl` and treats each line's rc/errno as
  ground truth for the S1 unknowns (exec-handle eboot case, in-game DT_DEBUG,
  NOLOAD of the game image, size-unit discrimination, mapping set, layout
  verdicts at the resolved base).
- The record schema is self-describing (marker/schema/build_id/seq/exp) so
  downstream parsing needs no module knowledge; `__trunc` flags bounded
  truncation.
- Rebuilds are deterministic (same sources -> same PRX hash); any source
  change produces a NEW build-<id>/ directory and build_id, keeping frozen
  history intact.
