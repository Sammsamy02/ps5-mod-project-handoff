# PS5 Mod Bridge — per-game runtime PROFILE FILE format (Team S1, B3)

**Status: Mac-only. Draft v1, 2026-10-13.** This is the contract the generator
team (B4) must emit and the runtime loader (`ModProfileLoader.hpp`, this team)
must parse. It replaces the ~11 compiled-in per-game headers listed in
READINESS §2 A–D with ONE data file, so one runtime build serves every Unreal
game. Nothing here is console-proven.

The guiding rule: **the file changes only the SOURCE of the per-game values.
Every existing gate/check keeps its exact behaviour** (whole-image sha, prefix
bytes vs the authenticated image, pool chain, ByteProperty probe, GMalloc
V1/V2, layout preflight). The runtime still re-proves every value against the
live authenticated image before use.

---

## 1. File name, location, selection, loadlist

- **One file per executable**, named by the image it describes:
  `profile/<image-sha256>.ini` under the title root
  `ROOT = /data/modbridge/roots/<TITLE>/`
  (`<image-sha256>` = lowercase 64-hex of the plaintext executable, the same
  hash the startup code-cache streams — READINESS A7).
- **Role / preload.** The file is a loadlist row with the NEW role `profile`:
  `file<TAB>profile<TAB>profile/<sha>.ini<TAB><size>`. `mfa_activate`'s default
  preload roles become `{lua_mod, config, script, profile}` so the bytes are
  read into owned memory during `module_start` — the only moment `/data` is
  readable from the game process (READINESS B3; `modforge_activation.h`).
  The file is therefore size-exact and sha-pinned by the loadlist row exactly
  like every other preloaded file (this is integrity source #1).
- **Selection.** The runtime does not trust the file name. After the startup
  code-cache authenticates the running image and has its real sha
  (`codecache::Metadata.sha256`), the loader picks the preloaded `profile` row
  whose `[profile] image_sha256` **equals that authenticated sha**. Exactly one
  must match.
- **No match → fail closed.** If no preloaded `profile` row matches the
  authenticated image sha, the loader writes the image identity (sha, size,
  extent, build id, engine version string if known) to `runtime-status.txt`
  via `mfa_status_*` and the runtime **runs with no mods** (seeds never arm).
  That run is the "identification launch" (READINESS B3). This is NOT an error
  return — the game still runs, unmodded.

Interface to Team S2 / the bridge (defined so our designs meet):

```c
// Provided by the activation / bridge AFTER the code cache authenticates.
struct ProfileSelectionInput {
    const char*   authenticated_sha256; // 64 hex + NUL (codecache::Metadata.sha256)
    uint64_t      authenticated_size;   // codecache::Metadata.size
    uint64_t      authenticated_extent; // codecache::Metadata.extent
    uint64_t      authenticated_base;   // codecache::Metadata.base (Team S2 image anchoring)
    // preloaded profile rows (mfa role "profile"): bytes + length, NUL-safe
    struct Row { const unsigned char* data; uint64_t size; const char* path; };
    const Row*    rows; uint64_t row_count;
};
```

The loader never opens a file itself (it only parses bytes handed in), so it
adds **no new import** and does no `/data` I/O after `module_start`.

---

## 2. Syntax

- UTF-8 / ASCII, LF line endings. `;` or `#` starting a line is a comment.
- `[section]` headers; `key=value` within a section. Keys are
  case-sensitive, lowercase. Leading/trailing ASCII space trimmed.
- **Integers**: decimal, or `0x` + hex (lowercase digits). Written as the
  struct's natural width; the loader range-checks.
- **Booleans**: `0`/`1`.
- **Byte arrays** (`*_prefix`, `proof*_bytes`): lowercase hex, no `0x`, no
  separators, even length, e.g. `5548 89e5...` is written `554889e5...`. The
  loader records the byte count; it must equal the declared `*_size`.
- **Strings**: raw to end of line; the loader stores them NUL-terminated. A
  string may not contain a control char (the generator sanitises).
- **Multi-line text blocks** (`[layout.members]`, `[layout.vtables]`): begin
  with `<<<` on its own line and end with `>>>` on its own line; every line
  between is stored verbatim (this is the exact `MemberVariableLayout.ini` /
  `VTableLayout.ini` text the runtime already stages).
- **Unknown section or key → the loader records it and FAILS CLOSED** (a
  profile the runtime does not fully understand is refused; forward-compat is a
  schema bump, never a silent skip). Every REQUIRED key below must be present.

---

## 3. Digest and the two-source rule

Three independent integrity facts, none invented by the loader:

1. **Loadlist sha + size** of the whole file (deploy-tool-produced, verified by
   the row-check before launch). This is tamper-evidence for the file bytes,
   the property the compiled-in PRX sha gave before.
2. **`[profile] digest`** — the generator's own evidence/profile digest
   (today's `kDigest`, e.g. `d648e85e…`). The loader (a) stores it where the
   runtime uses `kDigest` (build-facts digest, native-layout `profile_digest`,
   thread-name `layout_digest`), and (b) re-derives a **body hash** = sha256 of the whole file with the single
   `body_sha256=<...>` line (and its trailing newline) removed, everything else
   verbatim, and refuses if `[profile] body_sha256` disagrees. Both `digest`
   and `body_sha256` are REQUIRED. (The loader hashes the two byte ranges around
   that line with an injected incremental sha256 — no scratch copy, no new
   import: the runtime wires its existing `codehash` sha, host tests wire a
   minimal one.)
3. **Live re-proof.** Every value is re-proven against the live authenticated
   image before use, exactly as today: code prefixes via the code-cache
   (`code_identity` + `cached_code_signature`), the pool via the chain +
   ByteProperty probe, GMalloc via V1/V2, the layout via the preflight. A
   wrong value in the file still fails closed at the live check; the file can
   only make the runtime refuse, never run on a wrong value.

Per-field "second source" (the two-source rule each shipped value needs) is the
generator's responsibility and is recorded in the per-field `*_derivation`
and `*_source` strings (carried verbatim from the generator's envelope
`method`/`evidence`). The loader does not grade confidence; it refuses any
REQUIRED value whose `present` flag is `0` (fail closed), mirroring the seed
table's all-or-nothing arming.

---

## 4. Sections and fields

Each row: **key — type — which compiled struct/field it fills — notes.**
"(S2)" marks a field Team S2 owns the sourcing of; it still lives in the file
so one file is complete, but S2's live image anchoring produces the observed
value the gate compares against it.

### `[profile]` (identity, REQUIRED)

| key | type | fills | notes |
|---|---|---|---|
| `schema` | uint | `buildfacts::Profile.schema`, loader schema | must be `1` |
| `name` | string | `buildfacts::Profile.name` | e.g. `townfall-ps5-ue56-shipping-stats-off` |
| `digest` | 64hex | `kDigest` (buildfacts.digest, layout profile_digest, thread layout_digest) | generator evidence digest |
| `body_sha256` | 64hex | loader integrity check | canonical body hash (see §3) |
| `image_sha256` | 64hex | **lookup key**; SeedTable `kImageSha256`, buildfacts/codecache `.sha256`, thread `kImageSha`, pak `image_sha256`, listener/tostring `kImageSha256` | |
| `image_size` | uint | buildfacts/codecache `.size`, thread `kImageSize` | bytes of the plaintext executable |
| `image_extent` | uint(hex) | buildfacts/codecache `.extent`, thread `kImageExtent`, pak `image_extent`, `rc_seed::kImageFileExtentBytes` (shared w/ S2) | `max(p_vaddr+p_memsz)` |
| `image_base` | uint(hex) | SeedTable `kBaseObserved`, buildfacts/codecache `.base` | observed load base (A5); re-checked live |
| `exe_path` | string | `codecache::Profile.path` | **(S2 / B8)**: where the readable copy lives on the console; carried as config |

### `[build]` (REQUIRED) → `buildfacts::Profile` + `[NativeLayout]` identity + settings (B7)

| key | type | fills | notes |
|---|---|---|---|
| `format` | uint | buildfacts/layout Format | `1` |
| `pointer_size` | uint | buildfacts/layout PointerSize | `8` |
| `engine_major` | uint | buildfacts.major, layout EngineMajor, settings version (B7) | ≥4 |
| `engine_minor` | uint | buildfacts.minor, layout EngineMinor | |
| `target` | string | buildfacts.target, layout Target | `prospero-x86_64-sysv` |
| `engine_revision` | string | buildfacts.engine_revision, layout EngineRevision | e.g. `5.6.1-0+++UE5` |
| `build_id` | string | buildfacts.build_id, layout BuildId | ELF NT_GNU_BUILD_ID hex |
| `build_configuration` | uint | buildfacts.build_configuration | 1=Shipping 2=Debug 3=Test |
| `build_source` | string | buildfacts.build_source | measured-config provenance |
| `stats` | uint | buildfacts.stats | 1=off 2=on |
| `stats_source` | string | buildfacts.stats_source | measured-stats provenance |
| `non_shipping` | bool | layout NonShipping | must equal `build_configuration!=1` |
| `stats_layout` | bool | layout Stats | must equal `stats==2` |
| `fname_case_preserving` | bool | fnameprofile `kCasePreserving`, buildfacts.fname_case_preserving (fname_case_known=1) | |
| `fname_output_bytes` | uint | fnameprofile `kOutputBytes`, buildfacts.fname_bytes | `8` (checked vs the port ABI constant) |
| `fname_output_alignment` | uint | fnameprofile `kOutputAlignment`, buildfacts.fname_alignment | `8` |

### `[pool]` (REQUIRED) → `rc_seed::kPoolLayout` (SeedValidation.hpp, B3/S3)

| key | type | fills |
|---|---|---|
| `current_block_offset` | uint(hex) | `.current_block_offset` (`0x8`) |
| `current_byte_offset_offset` | uint(hex) | `.current_byte_offset_offset` (`0xc`) |
| `blocks_offset` | uint(hex) | `.blocks_offset` (`0x10`) |
| `none_id_offset` | uint(hex) | `.none_id_offset` (`0x14040`) |
| `byte_property_id_offset` | uint(hex) | `.byte_property_id_offset` (`0x14044`) |
| `block_offset_bits` | uint | `.block_offset_bits` (`16`) |
| `max_blocks` | uint | `.max_blocks` (`8192`) |

The live pool-chain check (None/ByteProperty, CurrentBlock invariant) stays the
second source (S3); these constants are the first source.

### `[seeds]` (REQUIRED) → `SeedTable.h`

| key | type | fills |
|---|---|---|
| `generated` | bool | `kGenerated` (`1`) |
| `required_set_complete` | bool | `kRequiredSetComplete` |
| `fname_pool_label` | uint(hex) | `kFNamePoolLabel` |
| `guobject_array_label` | uint(hex) | `kGUObjectArray.label`, also listener `kGuObjectArrayLabel` |
| `fname_ctor_label` | uint(hex) | `kFNameCtorWchar.label` |
| `gmalloc_label` | uint(hex) | `kGMalloc.label` |
| `gmalloc_instance_label` | uint(hex) | `kGMallocInstanceLabel` |
| `gmalloc_vtable_label` | uint(hex) | `kGMallocVtableLabel` |
| `sco_label` | uint(hex) | `kStaticConstructObject.label` |
| `guobject_array_present` / `_derivation` | bool / string | `kGUObjectArray.present` / derivation JSON |
| `fname_ctor_present` / `_derivation` | bool / string | `kFNameCtorWchar.*` |
| `gmalloc_present` / `_derivation` | bool / string | `kGMalloc.*` |
| `sco_present` / `_derivation` | bool / string | `kStaticConstructObject.*` |

`heap_lo`/`heap_hi` are **retired** (READINESS B7 / S-items; admission dropped
them). OPTIONAL and ignored if present; the generator SHOULD omit them.

### `[fname]` (REQUIRED) → `rc::fnameprofile`

| key | type | fills |
|---|---|---|
| `ctor_prefix` | bytes | `kPrefix` (also code pin #1 bytes) |
| `ctor_prefix_size` | uint | must equal 32 and the byte count |

(`case_preserving`, `output_bytes`, `output_alignment` live in `[build]`.)

### `[code_pins]` (REQUIRED) → `rc::codeprofile::kPins[]` + prefixes

Structure (which pins exist, their ids) is generic and compiled; only the
label + prefix VALUES load here. **B5: the ToString pin (id 4) is removed** —
do NOT emit it (see `[tostring]`, probe constants only).

| key | type | fills |
|---|---|---|
| `sco_prefix` / `sco_prefix_size` | bytes/uint | `kScoPrefix` (pin id 2), size 25 |
| `listener_add_prefix` / `_size` | bytes/uint | listener pin id 5, size 16 |
| `listener_remove_prefix` / `_size` | bytes/uint | listener pin id 6, size 16 |
| `thread_name_prefix` / `_size` | bytes/uint | thread pin id 3, size 33 |

(The fname-ctor pin id 1 uses `[fname] ctor_prefix` + `[seeds] fname_ctor_label`.
Pak pins ids 20–25 come from `[pak]`. Labels for id 5/6/3 come from
`[listener]` / `[thread]`.)

### `[listener]` (REQUIRED) → `rc::englistener`

| key | type | fills |
|---|---|---|
| `add_label` | uint(hex) | `kAddDeleteListenerLabel` |
| `remove_label` | uint(hex) | `kRemoveDeleteListenerLabel` |
| `add_prefix` / `add_prefix_size` | bytes/uint | `kAddPrefix` (16) |
| `remove_prefix` / `remove_prefix_size` | bytes/uint | `kRemovePrefix` (16) |

(`kGuObjectArrayLabel` == `[seeds] guobject_array_label`.)

### `[tostring]` (REQUIRED) → `rc::engtostring` (pin removed, B5; constants kept)

| key | type | fills |
|---|---|---|
| `probe_id` | uint(hex) | `kProbeId` (`0x3`) — consumed by the in-port decoder (NameTypes.cpp) |
| `probe_expect` | string | `kProbeExpect` (`ByteProperty`) |
| `probe_expect_units` | uint | `kProbeExpectUnits` (12) |
| `out_units` | uint | `kOutUnits` (1035) |
| `label` | uint(hex) | provenance only (pin removed); OPTIONAL |
| `prefix` / `prefix_size` | bytes/uint | provenance only; OPTIONAL |

### `[thread]` (REQUIRED) → `NativeNameThread` (ThreadNameProfile.hpp)

| key | type | fills |
|---|---|---|
| `accessor` | uint(hex) | `kAccessor` (== thread pin id 3 label) |
| `game_name` | uint(hex) | `kGameNameStorage` |
| `render_name` | uint(hex) | `kRenderNameStorage` |
| `empty_name` | uint(hex) | `kEmptyNameStorage` |

(prefix + size in `[code_pins] thread_name_prefix`.) S6 may later remove the
need for this section; until then it is REQUIRED only when thread-name binding
is wanted — see `[features]`.

### `[pak]` (REQUIRED when pak mounting is a feature) → `ModBridge::PakMount::EnginePins`

Scalars: `pak_vtable`, `platform_mgr_get`, `platform_mgr_getpf`,
`mountpak_delegate`, `handle_mount_pak`, `delegate_vtable`, `execute`,
`lower_impl`, `snapshot_delegate`, `snapshot_vtable`, `snapshot_execute`
(all uint hex, image-relative). Fixed structural offsets the loader also
range-checks: `slot_get_lower_level=0x68`, `slot_delegate_execute=0x60`,
`delegate_inst_off=0`, `delegate_size_off=8`, `pak_count_offset=0x18`.
Six proofs `proof0..proof5`, each:
`proofN_linked` (uint hex), `proofN_size` (uint ≤64), `proofN_bytes` (bytes).
Proof order is fixed: `platform_mgr_get, platform_mgr_getpf, execute,
handle_mount_pak, snapshot_execute, lower_impl` (matches
`EnginePins::valid()` and `RuntimePakMount.cpp`).

### `[layout.members]` and `[layout.vtables]` (REQUIRED) → the staged pair

Two `<<< … >>>` text blocks carrying the exact `MemberVariableLayout.ini` /
`VTableLayout.ini` text (format identical to today's
`CompiledNativeLayoutDeclarations` member/vtable string literals, including the
`[NativeLayout]` identity header). These are the **single source of truth for
every field offset**. The loader derives from them:
- `CompiledNativeLayoutDeclarations.members` / `.vtables` (the text itself),
- `RuntimeBuildProfile::kRequiredMembers` / `kRequiredVtables` offsets (the
  REQUIRED subset + validator widths stay compiled-generic; the OFFSET for each
  is looked up here; `source` becomes `profile:<Class::Member>`),
- the expected offsets `JoinedLayoutPreflight` compares staged-vs-expected
  (B6: the written-in UE 5.6 numbers are dropped; the key set + dispositions
  stay compiled-generic).

The `[NativeLayout]` header inside each block MUST match `[build]`
(Format/PointerSize/EngineMajor/EngineMinor/NonShipping/Stats/EngineRevision/
BuildId); the loader refuses on mismatch.

### `[features]` (OPTIONAL) → which optional features may arm (B5 feeds this)

| key | type | meaning |
|---|---|---|
| `pak_mount` | bool | `[pak]` present and may arm; absent/0 ⇒ pak feature off, seeds still arm |
| `delete_listener` | bool | listener ops may arm (variant C) |
| `thread_name` | bool | thread-name binding may arm |

Absent `[features]` ⇒ a feature arms iff its section is fully present. A
feature marked present-but-failing-live-proof turns ONLY itself off (B5); a
SEED value failing is fatal (fail closed), unchanged.

---

## 5. What is NOT in the profile (stays generic / elsewhere)

- The ELF PT_LOAD segment table and the live base/extent anchoring — **Team S2**
  (`ImageLoadProfile.hpp`, image windows, `SeedValidation.hpp:129` base/extent).
  The profile carries `image_extent`/`image_base` as the authenticated
  expectation; S2 produces the observed values the code cache compares.
- Title detection and mod-root selection — **Team S2** (B1).
- `kMaxPins`, array sizes, `kChunk`, the FName port ABI type, which members are
  "required", validator widths, pool-chain logic, V1/V2 logic — generic,
  compiled, not per-game.
- Confidence grades — not shipped; a `present=0` REQUIRED value fails closed.

---

## 6. Example (Townfall, abbreviated — see `gen/townfall-profile.ini`)

```
[profile]
schema=1
name=townfall-ps5-ue56-shipping-stats-off
digest=d648e85ef79caa207025a8123c093a67f47ef12ff5ef9eb0801ba9c3b52ab135
body_sha256=<canonical body hash>
image_sha256=3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc
image_size=194241424
image_extent=0xbc602c8
image_base=0x400000
exe_path=/data/homebrew/Silent Hill Townfall PPSA33286/decrypted/eboot.bin
[build]
format=1
pointer_size=8
engine_major=5
engine_minor=6
target=prospero-x86_64-sysv
engine_revision=5.6.1-0+++UE5
build_id=04bb4330a7d71dcefe1cc2c4d19769d5
build_configuration=1
build_source=PT_SCE PATH:Townfall-PS5-Shipping.self
stats=1
stats_source=independent 244-marker classification:54 present;off margin12;C+Python agreement
non_shipping=0
stats_layout=0
fname_case_preserving=0
fname_output_bytes=8
fname_output_alignment=8
[pool]
current_block_offset=0x8
...
[seeds]
generated=1
required_set_complete=1
guobject_array_label=0x93ba110
...
[layout.members]
<<<
[NativeLayout]
Format=1
...
>>>
```
