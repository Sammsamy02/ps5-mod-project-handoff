# IDENTITY-RESOLUTION — engine_revision and build_id for the staged pair

Worker: worker-layout-identity, session 2026-10-04-zcode-team.
Retained image: `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(194241424 bytes; sha256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc` = E-capture).
All offsets below are byte-verified against this capture. No console, no network, no live game reads.

## 0. Result (evidence-backed values)

| Field | Proposed value | Status |
|---|---|---|
| format | `1` | fixed by binding |
| target | `prospero-x86_64-sysv` | fixed by binding, ELF64 x86-64 confirmed |
| pointer_size | `8` | confirmed |
| engine_major | `5` | store-immediate, below |
| engine_minor | `6` | store-immediate, below |
| **engine_revision** | `5.6.1-0+++UE5` | **NEW — every component statically verified, section 3** |
| **build_id** | `04bb4330a7d71dcefe1cc2c4d19769d5` | **NEW — ELF GNU build-id note, section 4** |
| non_shipping / stats | out of scope here | separately fingerprinted (build_properties.rs), still unresolved in profile |

`engine_revision = "5.6.1-0+++UE5"` is UE's `FEngineVersion::ToString` shape
(`<major>.<minor>.<patch>-<changelist>+++<branch>`) assembled from five individually
verified components (major 5, minor 6, patch 1, changelist 0, branch L"UE5"). The
complete string does not exist verbatim in the image; UE builds it at runtime. This is
stated explicitly — no component is invented, but the assembled form is a semantic
reconstruction, so the controller owns the final choice (section 6).

## 1. Binding acceptance criteria (verified against pinned contract)

Pinned source: `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/src/NativeLayoutBinding.cpp`
(identical in `native-coherent-runtime-1/source-reconstruction-final-4/port/ue4ss/src/`).

- Stage gate, `StageNativeLayoutBundle` (lines 140-143):
  `format != 1 || target != "prospero-x86_64-sysv" || pointer_size != 8 ||
  engine_major < 4 || engine_revision.empty() || build_id.empty()` → refuse.
  That is the ONLY check on these two fields at staging: **non-empty**.
- Publish gate, `PublishStagedNativeLayoutMaps` (lines 175-182): compares ONLY
  `engine_major`, `engine_minor`, `non_shipping`, `stats` against resolved facts
  ("Native layout declarations disagree with resolved engine facts").
  **engine_revision and build_id are never runtime-compared — they are staging
  provenance tags.** Coordinator's understanding confirmed from the throw conditions.
- Parser (`ue4ss-native-layout-parser-1`): "Opaque nonempty printable-ASCII
  EngineRevision/BuildId values retain internal spaces, `#`, `;` and `=` literally;
  they are untrusted data, never instructions". ≤512 bytes/token. Numeric-converted
  values are NOT a mechanism: both fields are `std::string` in `Identity`
  (NativeLayoutFiles.hpp lines 8-13); any nonempty printable ASCII (0x20-0x7E) is
  accepted and no numeric form is required or special-cased.
- Both files of the pair must carry an identical `[NativeLayout]` 9-field header
  (Format, Target, PointerSize, EngineMajor, EngineMinor, NonShipping, Stats,
  EngineRevision, BuildId); parser refuses identity disagreement between the files.
- Assembler gate (`assemble_staged_pair.py` `build_identity`): refuses empty or
  non-printable values and comments: "these must come from the live in-process scan
  (controller), never defaults". The values below are evidence-derived scan facts of
  the retained image, offered as the controller's `identity_facts`; the controller
  still performs the live confirmation per its own contract.

## 2. Where UE sources these fields (semantics)

- **engine_revision** — UE's `FEngineVersion` (Runtime/Core/Private/Misc/EngineVersion.cpp).
  `InitializeVersion()` stores compiled-in Major/Minor/Patch, a changelist word
  packed as `CurrentChangelist | (IS_LICENSEE_VERSION << 31)`, and the branch name
  string, into the `FEngineVersionBase` singleton; `ToString()` renders
  `<major>.<minor>.<patch>-<changelist>+++<branch>`. The port's pinned resolver
  (`ue4ss-native-tick-relocation-resolver-1/crate/src/engine_version.rs`) finds this
  via four scan strategies; the one that matches this image is `scan_version_stores`
  (immediate stores to `FEngineVersionBase`), which requires exactly the store
  sequence found below and yields major/minor — matching the survey's
  `engine_version status=0 major=5 minor=6`.
- **build_id** — UE's per-module build identity is the UBT `BUILD_ID` define compared
  by `FModuleManager` (config override machinery `bUseBuildIdOverride` /
  `BuildIdOverride` IS present as UTF-16 strings in the image — see section 5 — but
  no `BUILD_ID`/`BuildId` string exists in narrow ASCII anywhere in the image, and no
  labeled UE build-id value string is embedded). The only build identity hash the
  executable itself carries is the **ELF `NT_GNU_BUILD_ID` note** (section 4).
- The profile (`worker-2-layout-facts/.../townfall-ps5-ue56.profile.json`) records
  engine_revision/build_id as null with "unknown (no evidence)". This document is the
  first evidence; the profile remains read-only for this worker.

## 3. engine_revision — static evidence (all offsets in the retained capture)

Decoded init sequence (UE 5.6 store site), file offset `0x1F37DE` (vaddr `0x1EF7DE`,
exec LOAD 1):

```
0x1F37DE: c7 03 05 00 06 00      mov dword [rbx], 0x00060005   ; Major=5, Minor=6
0x1F37E4: 66 c7 43 04 01 00      mov word  [rbx+4], 1          ; Patch=1  → 5.6.1 (NEW fact)
0x1F37EA: 44 89 7b 08            mov dword [rbx+8], r15d       ; Changelist field
0x1F37EE: 4d 39 f0               cmp rax, r14                  ; branch-string self-check
0x1F37F1: 74 42                  je +0x42
0x1F37F3: 4c 8b 7d c0            mov r15, [rbp-0x40]           ; BranchName.Data
0x1F37F7: 4c 63 65 c8            movsxd r12, [rbp-0x38]        ; BranchName.Num
0x1F37FB: 44 89 63 18            mov [rbx+0x18], r12d          ; FString num store
```

This is the exact `mov rm32,imm32 → mov rm16,imm16(patch,+4) → write(+8)` shape the
pinned `scan_version_stores` requires (engine_version.rs lines 115-191, incl. the
"`Changelist` follows shortly after" +8 write check).

r15d is built just above (file `0x1F37A2-0x1F37BC`), fully decoded:

```
0x1F37A2: e8 39 09 e1 ff   call → vaddr 0xE0      ; helper 1 (GetCurrentChangelist)
0x1F37A7: 41 89 c6         mov r14d, eax          ; r14d = changelist
0x1F37AA: e8 31 09 e1 ff   call → vaddr 0xE0      ; helper 2 (IS_LICENSEE_VERSION)
0x1F37AF: 44 0f b6 f8      movzx r15d, al         ; flag
0x1F37B3: 41 c1 e7 1f      shl r15d, 31
0x1F37B7: 45 09 f7         or  r15d, r14d         ; changelist | (flag<<31)
0x1F37BA: e8 31 09 e1 ff   call → vaddr 0xF0      ; helper 3 (branch name accessor)
```

Helper bodies (direct byte decode, deterministic under LOAD-1 mapping):

- vaddr `0xE0` (file `0x40E0`): `55 48 89 e5 31 c0 5d c3` = `push rbp; mov rbp,rsp;
  xor eax,eax; pop rbp; ret` → **returns 0**. Called twice → changelist = 0,
  licensee flag = 0.
- vaddr `0xF0` (file `0x40F0`): `55 48 89 e5 48 8d 05 87 66 82 07 5d c3` =
  `lea rax,[rip+0x7826687]; ret`. rip-after = file `0x40FB` → target file
  `0x40FB + 0x7826687 = 0x782A782` (rodata LOAD 2). Bytes at `0x782A782`:
  `55 00 45 00 35 00 00 00` = wide **L"UE5"** (vaddr `0x7826782`). This corroborates
  the accessor: **BranchName = "UE5"**.

Call targets are rel32 (slide-independent); each was recomputed twice. The single
`C7`-family store with immediate `05 00 06 00` in the image is this site (scan over
the C7 modrm space and `41 C7` REX variants returned exactly one hit), consistent
with the resolver's `ensure_one`.

Note: the survey's "UE 5.6" was major/minor only. **Patch = 1 is new evidence** —
this engine is 5.6.**1**. The numeric identity fields stay `5`/`6` either way, and the
publish-time equality check covers only those two.

## 4. build_id — static evidence

ELF program headers (parsed from the capture): 14 segments; two `PT_NOTE`.

- File `0xB93B250` (vaddr `0xBC5F250`, inside LOAD 5's mapped range): note header
  `namesz=3, descsz=20 (0x14), type=3 (NT_GNU_BUILD_ID)`, name `"GNU"`,
  descriptor bytes:
  `04 bb 43 30 a7 d7 1d ce fe 1c c2 c4 d1 97 69 d5 00 00 00 00`
  → **build-id `04bb4330a7d71dcefe1cc2c4d19769d5`** (16 significant bytes; the
  trailing `00000000` is part of the declared 20-byte descriptor and reproduced
  verbatim).
- Second `PT_NOTE` (file `0xB93E348`, vaddr 0): six empty name/type-0 records, no data.

The hex string `04bb4330a7d71dcefe1cc2c4d19769d5` is nonempty printable ASCII,
unique to this link, and readable from the retained image. It is an ELF link
identity, not UE's `FModuleManager` build id — that distinction is preserved in
section 6.

## 5. Negative results (searched, absent — saves the next probe)

All scans against the full 194,241,424-byte capture:

- `++UE5+Release`, `UE5+Release`, `Release-5.` — 0 hits in ASCII and UTF-16LE.
  (`scan_branch_name` and `scan_engine_version_string` provably cannot match this
  image; the store scan is the only live strategy, as the survey's status=0 implies.)
- Wide strings starting `N.N.N-` preceded by NUL — 0. Wide strings starting `++`
  preceded by NUL — 0. (The two UTF-16 `5.6.` hits at file `0x77BA962`/`0x783FD82`
  are CVar help-text fragments, not NUL-preceded string starts — resolver-invisible.)
- Narrow ASCII `BuildId` / `BUILD_ID` / `buildid` — 0 hits. UTF-16 hits are config
  machinery only: `BuildIdOverride` (0x75D3FDA), `bUseBuildIdOverride` (0x76CCE50),
  `BuildIdOverride=` (0x793DF2C), `BuildId` (0x7A32F6C), `BUILD_ID` (0x7A3E4EC),
  `buildidoverride` (0x798EB54). No labeled UE build-id value string is embedded.
- `EngineRevision` string — absent in both encodings; `FEngineVersion` narrow-string
  hits are UE reflection/function names (`GetEngineVersion`, `LinkerLoad_EngineVersionIncompatible`,
  package-version message), not values. `Current Changelist` (0x7B8C465) /
  `Compatible Changelist` (0x7BD50F3) are FEngineVersion dump labels, not values.

## 6. Decision points for the controller (honesty boundaries)

1. `engine_revision = "5.6.1-0+++UE5"` — every component is a verified image fact,
   but the joined string is a semantic reconstruction (UE renders it at runtime).
   If the controller requires the literal in-process value instead, the live read is
   `FEngineVersion::Current()`'s singleton (address = `load_base` + singleton vaddr;
   singleton vaddr is caller-supplied `[rdi]`, not statically established here — a
   bounded caller trace from vaddr `0x1EF7DE` is the future work item, NOT a re-scan).
2. `build_id = "04bb4330a7d71dcefe1cc2c4d19769d5"` is the ELF GNU build-id. It is
   not UE's `FModuleManager` BUILD_ID (absent from the image, section 5). Mapping
   build_id := GNU build-id is a convention choice the controller must ratify; the
   alternative is leaving the field to a live in-process fact, for which no labeled
   UE build-id source exists in this executable.
3. Build axes `non_shipping`/`stats` remain unresolved here and still gate
   `static_layout_ready`; resolution path exists (pinned `build_properties.rs`
   fingerprint) but is another worker's field.

## 7. Read paths for the next probe

Segment mapping (from the capture's program headers; the "+0x4000" rule is
per-segment, not global):

| LOAD | vaddr range | file = vaddr + delta |
|---|---|---|
| 1 (R-X code) | 0x0 - 0x7479B1C | +0x4000 |
| 2 (R rodata) | 0x747C000 - 0x7EBA918 | +0x4000 |
| 3 (RW data) | 0x7EBC000 - 0x913A4E8 | +0x4000 |
| 4 (RW data) | 0x913C000 - 0x9633418 | +0x4000 |
| 5 (RO) | 0x9633420 - 0xBC602C8 | **+0x324000** |

Facts of record (file offset ↔ vaddr ↔ bytes):

| Item | vaddr | file offset | bytes |
|---|---|---|---|
| Version store site | 0x1EF7DE | 0x1F37DE | `c7 03 05 00 06 00 / 66 c7 43 04 01 00 / 44 89 7b 08` |
| return-0 stub (CL/licensee) | 0xE0 | 0x40E0 | `55 48 89 e5 31 c0 5d c3` |
| branch accessor stub | 0xF0 | 0x40F0 | `55 48 89 e5 48 8d 05 87 66 82 07 5d c3` |
| Branch L"UE5" | 0x7826782 | 0x782A782 | `55 00 45 00 35 00 00 00` |
| GNU build-id note | 0xBC5F250 | 0xB93B250 | hdr `03000000 14000000 03000000` + `GNU\0` + 20-byte desc |
| data slot deref'd by init | 0x9131F08 | 0x9135F08 | pointer (role not established; do not rely) |

- Static: read directly from the saved capture at the file offsets above (how this
  document's values were obtained; capture sha256 pinned in CLAIM/profile E-capture).
- Live: runtime address = `load_base` + vaddr. `load_base` is unresolved in ALL saved
  evidence (profile fact); the probe must establish it first. Every identity read is
  in mapped R or R-X spans → **direct reads; mprotect is NOT required** for any of
  them (mprotect would only be needed for writes/patching, which identity resolution
  does not do). The exec-span store site (0x1EF7DE) reads fine without elevation.
- The GNU note (LOAD 5) is mapped at `load_base + 0xBC5F250` (24-byte note header
  then name/desc); on-disk fallback is file offset 0xB93B250 of the decrypted eboot.

## 8. Acceptance matrix (binding criteria vs proposed identity)

| Binding condition | Proposed value | Verdict |
|---|---|---|
| format == 1 | 1 | pass |
| target == "prospero-x86_64-sysv" | fixed | pass |
| pointer_size == 8 | 8 (ELF64 x86-64) | pass |
| engine_major >= 4 | 5 | pass |
| engine_revision non-empty printable ASCII | `5.6.1-0+++UE5` (13 bytes, all 0x20-0x7E) | pass |
| build_id non-empty printable ASCII | `04bb4330a7d71dcefe1cc2c4d19769d5` (32 bytes hex) | pass |
| publish: engine_major/minor == resolved facts | 5/6 == survey `status=0 major=5 minor=6` | pass (corroborated) |
| publish: non_shipping/stats == resolved | unresolved axes — remains the blocking item outside this worker's fields | open |
| parser: identical [NativeLayout] block in both INI files | assembler emits one identity_text into both | pass by construction |
