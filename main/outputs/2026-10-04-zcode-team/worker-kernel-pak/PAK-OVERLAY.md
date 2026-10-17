# PAK-OVERLAY — PS5 UE4SS runtime port: PAK file mounting overlay

Worker: `worker-kernel-pak` (2026-10-06). Research + host-side prototype only.
No console contact, no game claims, no in-game results. Everything below is
either byte-proved against pinned local captures (marked **proved**) or clearly
labeled inference (**inferred**).

Inputs used (all previously captured, read-only):
- Decrypted eboot `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
  (sha256 `3b6cfad4…`, 194,241,424 B, ELF64 x86-64 PS5 SELF)
- Saved PS5 main pak `.superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak`
  (sha256 `ee145cbc…`, v11) and utoc captures `townfall-main-utoc-2026-10-01/`
- PC Leon mod trio `leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.{pak,utoc,ucas}`
- ModBridge staged diagnostic pak `townfall-extra-pak-test-2026-10-01/diagnostic-render-control/pakchunk9999-ModBridgeRenderDiag_a6d6f620-…_P.pak`
- repak-built spike pak `mount-research/artifact/original-asset-format-spike.pak`
- Prior worker evidence: `worker-pak-index/` (v11 index decode), `2026-10-01-townfall-mod-trial.md`,
  native iostore reader receipts.

New tools (this dir): `tools/pak_inspect.py` (host-side pak/utoc inspector),
`tools/eboot_xref.py` + `tools/eboot_disasm.py` (eboot string/code xref).
Evidence: `evidence/*.json`, `evidence/*.disasm.txt`.

---

## 1. How UE5 loads PAK files on this PS5 game (eboot RE)

All vaddrs are image vaddrs of the pinned decrypted eboot; fileoff = vaddr+0x4000.
Method: string survey → rip-relative LEA xref scan → capstone disassembly.

**The full stock PakFile module is present.** Strings: `FPakPlatformFile::Initialize`
(ref'd at 0x1769f01), `PakFile_LoadPrimaryIndex`, `PakFile_LoadPathHashIndex`,
`PakFile_LoadDirectoryIndex`, `PakFile_SerializeEncodedEntries`, `PakFile_LoadLegacy`,
stat group `Pak_Mount`, `QuickMountingPaks`, `MountPaksAfterEarlyStartupScreen`.
Feature strings (`-SkipStartupPaks`, `SkipOptionalPakFiles`, `SkipOnDemandPakFiles`,
quick-mount) pin this engine to the UE5.4+ generation (proved: strings + call sites).

**Boot scan chain (proved):**

| vaddr | Role |
|---|---|
| 0x1769ee0 | `FPakPlatformFile::Initialize` (assert string ref) |
| 0x1769df0 | GetPakFiles wrapper: skips all pak loading if cmdline `-NoPak`; builds folder list; scans `*.pak` |
| 0x1769c20 | folder builder: `FString::Printf("%sPaks/", <content dir>)` — pattern string **proved** at 0x7783520 |
| 0x1769b00 | per-folder-list iterator |
| 0x1769a20 | per-folder scan: honors `-SkipStartupPaks`/optional/on-demand switches, then `IPlatformFile::IterateDirectory` (virtual +0x1c0) with a visitor |
| 0x176b410 | FindPakFiles: appends `*.pak` (ascii 0x7b4f4ac, utf16 0x76d739e) to the folder, collects results, `Pak_Mount` stat scope |

**Paths scanned:** `<ProjectContentDir>Paks/` (i.e. the observed
`/mnt/sandbox/PPSA33286_000/app0/townfall/content/paks`) plus engine `Paks`
folders, each built from `%sPaks/` (proved pattern). The observed on-disc paks
directory matches the project-content folder exactly.

**Can additional PAKs be mounted at runtime?** The engine mounts everything its
boot scan finds in those folders — a `pakchunk9999-…_P.pak` staged into the
folder is inside the scanned folder (**proved** scan scope; **not proved** that
the engine accepted the staged artifact — the frozen trial honestly records
"engine mounting and mod acceptance remain unproved"). Runtime (post-boot)
mounting also exists as code: the DLC/on-demand mount loop at 0x1764de0 and the
shared mount helper at 0x176ec30 are callable paths (**proved present**;
**not proved invoked** by any runtime yet).

**Mount priority (proved shape, constants partial):** folder-index priority —
0x1770140 ranks a pak 4/3/2/1 by which `Paks` folder it sits in (project content
highest). The mount helper at 0x176ec30 explicitly tests `Contains("_P.pak")`
(ignore-case, utf16 string **proved** at 0x775a23a) — the patch-pak convention is
recognized in-engine. Exact `_P` numeric bump not extracted.

**Subfolder recursion (`~mods` convention): NOT settled.** The scan goes through
an `IterateDirectory` visitor whose vtable is BSS-initialized; file bytes cannot
prove whether the visitor descends into subfolders. Stock UE5.4-era engines do
scan recursively (community `~mods` behavior), but this specific retail build
needs a runtime observation. Do not design the product around `~mods` until
observed; **placing mods directly in `Paks/` with `_P` names is the proved-safe
surface** (the scan wildcard is folder-level `*.pak`).

**IoStore:** the game ships classic paks only as loose-file companions; the real
content is `.utoc/.ucas` triples per chunk (proved: on-disc listing +
`.res/.png/.ini` census below). UTOC discovery happens in the same folder
ecosystem; engine version = IoStore **v8** headers (both game and PC mod).

## 2. pakchunk naming convention — compatibility finding

Game on disc (**proved**, runtime LIST): `pakchunk0-ps5.pak` … `pakchunk12-ps5.pak`,
each with `.ucas/.utoc` companions, plus `global.ucas/.utoc`, plus empty 339-byte
stub paks for optional chunks. PC mods for this game family use
`zzz_Leon_Townfall_2K_P.pak` + `.utoc/.ucas` trio (**proved** bytes).

Verified structure across producers (tools/pak_inspect.py, all index SHA1s match,
all region SHA1s match):

| Artifact | ver | layout | entries | path-hash seed | flags |
|---|---|---|---|---|---|
| PS5 `pakchunk0-ps5.pak` | v11 | compact | 4699 | 0x25EB747C | 0xE0000000 |
| ModBridge diagnostic `_P.pak` | v11 | compact | 1 (`Townfall/Config/DefaultEngine.ini`) | 0x25EB747C (same as game) | 0xE0000000 |
| PC Leon stub pak | v11 | compact | 0 (IoStore companion) | 0x0 | 0x0 |
| repak spike pak | v11 | compact | 3 | 0xF376E23D (= repak `--path-hash-seed 4084654653`) | 0xE0000000 |

Key discoveries this session:
- **One unified v11 "compact" index layout** across game paks, PC mod stubs,
  repak output and ModBridge output: mount FString, count, **path-hash seed**
  (the field worker-pak-index labeled "lead u32 semantics unknown" is the seed —
  proved by matching repak's documented seed), two SHA1-verified regions
  (path-hash table + name directory; 40-byte descriptors, 96-byte table),
  `count*12`-byte entry array `[u32 off][u32 size][u32 flags]`.
- Game pak entry records carry flags `0xE0000000`; the ModBridge diagnostic pak
  wrote `0` (and game record-0 anomaly noted by worker-pak-index). Sampled
  original entries with `0xE0000000` are **plaintext** (mount-research payload
  receipts), so these bits are not an encryption bit — semantics still unlabeled.
  This flag difference is a concrete candidate explanation for the unproved
  mount acceptance of the diagnostic pak, and a concrete fix to test
  (write `0xE0000000` records).
- `pakchunk9999-<name>_<guid>_P.pak` sorts after every base chunk and carries
  the `_P` marker the engine recognizes — the correct naming for additive paks.

## 3. Game container structure + PC-mod compatibility assessment

**pakchunk0-ps5.pak census (proved, 4699 entries, this tool):** 3438 `.res`
(ICU i18n), 425 `.png`, 313 `.svg`, 233 `.uplugin`, 111 `.ini`, 90 `.ufont`,
24 `.xml`, 15 `.ttf`, … **Zero** `.uasset/.uexp/.umap/.ubulk/.ushaderbytecode`.
→ Classic paks hold loose files only; **all cooked content is IoStore**.

**utoc headers (proved):** game and PC mod are IoStore **v8**, 144-byte header,
12-byte chunk IDs, 64 KiB blocks, unencrypted, same 16-byte b"-==-"*4 prefix:

| utoc | entries | blocks |
|---|---|---|
| PS5 pakchunk0-ps5 | 59,478 | 1,998,522 |
| PS5 pakchunk1-ps5 | 197 | 1,128 |
| PC Leon mod | 162 | 2,948 (None compression, matches frozen native reader) |

**Compatibility verdict for PC UE5 mods on PS5 (this game family):**
1. **Container format: compatible.** Same pak v11 compact index, same utoc v8
   geometry, unencrypted, `_P` naming recognized. The Leon trio's 68/162 package
   IDs already match the installed target's indexes (prior trial evidence).
2. **Loose-file content (ini/locres/fonts/icons): deliverable as-is** through a
   classic pak overlay. This powers config/cvar-style mods.
3. **Cooked asset content (character models etc.): platform mismatch risk —
   the open blocker.** PC packages embed PC shader maps (SM5/SM6 D3D) and PC
   texture encodings; PS5 consumes GNF-formatted GPU data. Package load is
   expected to fail or misrender without recooking for the PS5 target
   (**inferred** from engine platform model; **not tested** — no console run).
   No PC→PS5 recooking path exists in this workspace; that is the honest limit
   of "drop a PC mod pak on PS5".

## 4. Overlay approaches

### A. Kernel-level file redirect (kernel module supplies virtual files)
- **A2 virtual enumeration (recommended kernel form):** when the game process
  enumerates/opens `<app0>/…/content/paks/*.pak` (and utoc), the kernel module
  injects `pakchunk9999-<mod>_<guid>_P.pak` entries backed by files on the mod
  partition. No disc mutation, engine-agnostic, no engine signatures.
  Requires the kernel infra already being built by `worker-kernel-build` /
  `worker-kernel-saferead` (their safe-read client/probe model extends to
  write-path interception with the same ownership/bounds discipline).
- **A1 whole-chunk spoof** (redirect `pakchunkN-ps5.pak` open to a repacked
  superset) is possible but heavy: must absorb 297 MB corpora + breaks on game
  updates. Only worth it for replacing an existing chunk's loose files when
  additive mounting is refused.
- Risks: path/process filtering must be exact; PS5 kernel work is the team's
  hardest track; sandbox FS layering (app0 is AMPR-backed) may complicate
  virtual entries — design doc needed with the kernel workers.

### B. Engine-level PAK registration (call the engine's own mount code)
- Target: mount helper at **0x176ec30** (FileExists check → 0x330-byte FPakFile →
  ctor 0x1771fa0 → validation flag [pak+0x2e1] → mount-point normalization →
  `_P.pak` handling) or the DLC mount loop at 0x1764de0. Image vaddrs are known;
  live base via kernel proc info; UE4SS-style signature verification applies.
- Requirements: engine-compatible FString args (engine allocator), System V
  x64 ABI, timing after `FPakPlatformFile::Initialize` and before package
  resolution — `QuickMountingPaks`/`MountPaksAfterEarlyStartupScreen` deferral
  makes "after Initialize" insufficient as stated; needs a settled hook event.
- Benefit: uses engine priority/registration semantics for both classic pak and
  (via the DLC path) associated containers; enables late/runtime mounting for
  mod menus. This is the UE4SS-proven pattern on PC.

### C. Physical staging (already demonstrated, partly)
- FTP staging proved the file lands in the game-visible paks dir and the game
  runs with it present; mount acceptance still unobserved (frozen trial).

### Recommendation (phased, evidence-gated)
1. **Phase 0 — settle mount acceptance with the engine's own format.** Rebuild
   the diagnostic pak with entry flags `0xE0000000` (byte-identical to game
   records) + a cvar with an observable engine-logged effect, and add an
   engine-level observation channel (engine log read or config-readback probe).
   This converts "unproved mount" into a fact before any overlay engineering.
   Also settle subfolder recursion (one observation of a `~mods`-style subfolder
   pak) to know whether `Paks/~mods/` is a valid surface on this build.
2. **Phase 1 — kernel virtual-file injection (A2)** for boot-time additive paks:
   lowest coupling, reuses the kernel track, no disc writes, per-game profile.
3. **Phase 2 — engine-level registration (B)** for runtime/late mounting and
   mod-menu flows, using pinned offsets + signature scan + allocator-correct
   FString marshaling; depends on Phase 0/1 learnings and the runtime team's
   calling infrastructure.
4. **IoStore asset mods:** delivery via trio (`_P.pak` stub + utoc/ucas) in the
   same folder. Treat PC-cooked payload compat as an open research item with a
   recook requirement; do not promise asset-mod parity.

## 5. Implementation plan for the host tool chain

`tools/pak_inspect.py` (delivered, pure Python, read-only):
- parses footer (magic-anchored) for v8–v11: version, index offset/size, index
  SHA1 (verified against bytes), index-encryption flag, compression slots;
- parses the unified v11 compact primary index: mount point, entry count,
  path-hash seed, both region descriptors with live SHA1 verification, and the
  12-byte entry records fully name-resolved via the SHA1-verified name
  directory (walk closes exactly — same invariant worker-pak-index proved);
- probes utoc headers (stock or b"-==-"-prefixed): version, header size, entry
  and block counts, chunk-id size, block size;
- emits a compatibility report (pak version match, encryption, compression,
  entry-type census, shader-bytecode/cooked-package warnings).
- Validated this session on all four pinned artifacts (see table above); every
  index and region SHA1 verified. Known gap: repak's per-entry variant in some
  older outputs is handled only via the shared compact layout (all local
  artifacts parse; a non-compact v8/v9 pak would need the classic walker
  extended — none exists in this workspace).

Next tool steps (not built this session): pak **writer** aligned byte-for-byte
with game records (`0xE0000000` flags, same seed handling, same footer shape)
for Phase 0; utoc directory-index name listing for mod package-ID checks.

## 6. Honest limits

- No console contact, no game process, no engine logs: nothing here proves an
  actual mount, load, or in-game effect on PS5.
- Subfolder (`~mods`) recursion: unresolved statically (BSS visitor vtable).
- `_P` priority bump constants: not extracted.
- PC cooked-asset loadability on PS5: unverified, expected to require recooking.
- Entry-flag bit semantics (`0xE0000000`): unlabeled by spec; sampled data is
  plaintext, so they must not be read as encryption.
