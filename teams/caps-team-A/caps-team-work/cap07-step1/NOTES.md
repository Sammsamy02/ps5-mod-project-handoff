# NOTES: capability #7 step 1 (Team A)

Labels: **[Mac-only]** = checked on the Mac in this session. **[earlier console evidence]**
= a record from an earlier session's console contact, cited and not re-verified here
(no new console verdict). I made no console contact.

## 1. The proven method and its evidence

- [earlier console evidence] `.superpowers/native-observations/leon-index-overlay-trial-2026-10-01/result.json`:
  `"phase": "in_game_leon_appearance_confirmed_by_user"`, `"base_ucas_modified": false`,
  derived index `3ff1946c…`, sidecar `27c7b50a…`, plus the caveat
  `"potential_material_texture_artifact": "pale checkerboard pattern on glasses"`.
  Journal `console-transaction.json`: upload sidecar temp + index temp, rename sidecar,
  rename `pakchunk0-ps5.utoc` → `.original-603c4879-….bak`, rename derived into place.
  Executor: `.superpowers/native-dependency-spike/iostore-sidecar-overlay-2/index_transaction.py`
  (sha `6510b846…`), one index, one sidecar.
- Builder that generalised it: `outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py`
  (sha `a72636fb…`, pinned by paktx). I ran nothing in place. `test_overlay_builder.py`
  writes into its own folder, which produced the two SUSPECT files, so I did not run it.

## 2. What the console still carries (why Leon looked odd and why it was bright)

- [earlier console evidence] 2026-10-08 storage scan
  `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/storage/files.json`,
  folder `…/townfall/content/paks`:
  - `pakchunk0-ps5.utoc` 29,870,724 B (= trial derived index size) plus two
    `pakchunk0-ps5.utoc.original-{8988cacd…,603c4879…}.bak` (29,707,562 B = original), and
    `pakchunk0-ps5_s1.ucas` 187,713,177 B. **The Leon trial was never rolled back.**
  - `zzz_Leon_Townfall_2K_P.{ucas,pak,utoc}`: the unchanged mod trio, staged as new files
    on 2026-10-01 (`townfall-leon-bundle-trial-2026-10-01/result.json`,
    `engine_mount_verified: false`).
  - `pakchunk0-ps5.pak` **297,483,249 B** plus `pakchunk0-ps5.pak.original-9a914e38-….bak`
    297,572,462 B.
- [Mac-only] 297,483,249 B is exactly the size of
  `.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/derived.pak`
  (sha `8bcf5eb1…`, re-hashed now). Its build receipt changes one entry,
  `Townfall/Platforms/PS5/Config/PS5Engine.ini`, to `unreal-config-adapter-1/merged.ini`
  (sha `94fc0da4…`, re-hashed). That file merges Nexus mod 110's Engine.ini:
  `r.VolumetricFog=0` (line 36), `r.EyeAdaptationQuality=0` (54), `r.Color.Mid=0.42` (55),
  `r.TonemapperGamma=1.3` (56), `r.ShadowQuality=1` (17), contact and distance-field
  shadows off. That matches the user's "bright as day, can see the monster clearly".
  No journal for this swap exists anywhere under the worktrees (searched for the nonce
  `9a914e38-5a1e`). Status: **strong file-level explanation, not log-proven.**
  [earlier console evidence, re-hashed by the reviewer] the integrator's read-only pull of
  2026-10-13 (`outputs/2026-10-08-l25-prep/evidence/console-pulls-20261013-paks/SHA256SUMS`):
  live `pakchunk0-ps5.pak` = `8bcf5eb1…`, exactly the config-trial `derived.pak`; its
  `.bak` = `ee145cbc…`, exactly the recorded original. One entry differs
  (`FINDING-bright-outdoors.md`). The `repair` step re-hashes the `.bak` against
  `ee145cbc…` again before putting it back.
- Checkerboard [Mac-only, from `outputs/2026-10-08-l25-prep/shader-fallback/fallback.md` §1a,
  re-read here]: 5 Leon packages carry inline PC shader payloads (`M_Invisible` ×2,
  `M_Leon_Body`, `M_Leon_Skin`, `M_Leon_Hair`), with 0 PS5 platform tokens, and every mod
  store entry has 0 shader-map hashes. The gray/white checker is the engine's default
  material (`WorldGridMaterial`, present in the base TOCs). The user now reports it on
  all of Leon, not only the glasses. That fits, since Body/Skin/Hair are PC-only too.
  **#8/#12 work, not #7.** The step-1 launch PASS does not depend on it.
- The shipping executable has no UE log strings for material/shader misses (GM §1c,
  eboot string search), so no engine log line can confirm load or shader state. Hence
  the optional `[pk]` probe.

## 3. Multi-container ownership (real Leon data) [Mac-only]

`tests/output/real-leon-replay.json` (read-only inputs: 2026-10-01 captures plus the
unchanged mod):
- 127 Leon packages: **66 replace packages of `pakchunk0-ps5`, 2 replace packages of
  `pakchunk10-ps5`** (`2c77facba1d32785` = `MI_Simon_BodywarmerBeanieTag_Deluxe`,
  `976ba4c659563aa7` = `SK_Simon_HeadCap_Deluxe_optimized`), **59 are new** and go to the
  primary container (the one owning most replaced packages, `pakchunk0-ps5`).
  ChatGPT's `cap07/evidence/leon-container-selection.json` reports the same split
  (68 matched: 66/2; 59 unmatched): two independent derivations agree.
- The console-proven trial put all 127 into `pakchunk0`, so the 2 Deluxe packages became
  duplicates of packages `pakchunk10` already owns. Which copy the engine used is not
  pinned locally: the local Epic source subset has no IoDispatcher/FilePackageStore files.
  paktx therefore never creates a duplicate. Each package goes to its existing owner. It
  refuses if a package has two owners, or if a mod chunk id already exists in another
  container.
- The `pakchunk10` part is a 2-package projection, **123,312 B UCAS**. This matches
  ChatGPT's figure independently. One of the two imports `0x5dd733019f47199a`, a new mod
  package placed in `pakchunk0`. That cross-container import is the one new runtime fact
  this launch exercises. At worst the Deluxe beanie tag shows wrong, not the whole outfit.
- **Equivalence with the console-proven bytes** (pakchunk0 target built alone, compared
  with trial `3ff1946c…`/`27c7b50a…`):
  - 59,411 base rows identical (offset, length, IoHash meta)
  - 159 mod chunk payloads byte-identical
  - 38,498 merged store entries identical (imports and shader hashes)
  - 56,991 directory rows identical
  - the only difference: the 2 Deluxe packages' 2 chunks, 2 paths and 2 store entries
    moved to their owner `pakchunk10`.

  My pakchunk0 output: index `24884a02…`, sidecar `6546d73f…` (187,596,960 B). The trial
  repacked the mod through retoc; the builder copies the mod's block stream as-is, so the
  file bytes differ while every chunk's content is equal.
- Still missing: the `pakchunk10-ps5` ContainerHeader, which lives inside
  `pakchunk10-ps5.ucas` (113,396,720 B). The full plan refuses with
  `base_container_header_unavailable` until the integrator fetches it (needs list).

## 4. What paktx does (design)

- `record`: the game's own files become data. sha256 for every `.utoc`/`.pak`/other small
  file; size only for `.ucas`. Per container: package ownership from export chunks
  (type 1), and the ContainerHeader accepted only if it hashes to the TOC's IoHash and
  its package set equals the export set. The record hashes itself and refuses on tamper.
  Partial record staged: `PATCH/records/…PARTIAL.record.json`.
- `install` builds a deterministic plan (same inputs give the same sha). It projects
  split mods per owner and calls `build_overlay` per destination, adding checks of its
  own: equal block size, lookup resolves, every mod chunk present. It writes artifacts
  under `--work`. `--dry-run` prints every change. A real run needs
  `--confirm <plan sha>` and `--game-closed`.
- Executor:
  - phase A: uploads to temp names, each re-read and sha-verified (nothing live changes)
  - phase B: per container, sidecar into place, original renamed to `.original-<id>.bak`,
    derived into place
  - phase C: full re-read of every live index and backup.

  It journals each mutation's intent before acting. A lost reply is recorded as
  `unknown_may_have_completed` (exit 3).
- `restore` reads the current identities of the files (original / derived / absent /
  unknown), so it works after any interruption. Original bytes are only ever renamed,
  never rewritten, so the result is byte-exact, and it is re-read with sha256. Unknown
  files are preserved and the tool refuses. Partial temps are preserved. Deletion happens
  only with `--cleanup --allow-delete`, only for files this plan owns, and only after a
  re-hash.
- `repair` returns an install to the recorded build. It puts back sha-verified
  `.original-*.bak` copies and moves unrecorded containers found at ANY depth under
  `paks/` (review F1) to `content/paktx-quarantine-<id>/`, outside `Paks`, keeping the
  subfolder structure, and checks each moved file at its new place. UE looks for pak files
  recursively under `Paks`; that is general UE knowledge, not pinned in the local source
  subset. Nothing is deleted. `--only` restricts it to named files, so each console step
  gets its own OK (`../PRISTINE-PAKS.md`). It refuses a PARTIAL record and never moves a
  top-level file that shares a stem with a recorded container (a record missing one of a
  container's own files would otherwise quarantine a real game file).
- `check` (read-only): recursive listing + the full gate; `"verdict": "PRISTINE"`.
- The gate walks the whole `paks/` tree (depth limit 8, refuses links) and refuses any
  unrecorded `.utoc/.ucas/.pak/.sig` in any subfolder.
- Exit semantics (review F2): `refused` (exit 2) = no change was started. Once any change
  was started, every later error (failed verify, refusal, lost connection) is `stopped`
  (exit 3, `"console_changed": "maybe"`). `restore` renames an unknown live index aside
  (`.foreign-<id>.retired`, kept) when the verified original backup exists (review F3).
- `install --only-into STEM` (review N5): diagnostic fallback to one container; packages
  owned elsewhere are listed as not installed. On the real Leon files,
  `--only-into pakchunk0-ps5` plans exactly the pakchunk0 build of §3 (`24884a02…`).
- Classic `.pak`: a zero-entry companion is ignored. Loose files that override a base
  `.pak` trigger a full rebuild through the pinned repak, using the same commands as the
  2026-10-01 config trial. Each rebuild checks every entry: unchanged entries keep their
  sha, changed ones equal the mod bytes, a second reader (the builder's `parse_pak`)
  agrees, and the pack is repeated to prove it deterministic. Cooked assets inside a
  classic `.pak` are refused (conversion is not step 1).
- FTP backend: LAN/loopback only. It refuses every RETR/STOR/RNFR/RNTO/DELE on recorded
  `.ucas` names and checks that the destination is absent before any rename or upload.
  DELE accepts the ftpsrv `226 File deleted` reply. **Not console-proven**: tested only
  against `tests/fake_ftp.py`.

## 5. Bugs and hazards found along the way [Mac-only]

- `repak pack` is **nondeterministic** with several files when it runs multi-threaded:
  three packs of the same 3-file tree gave 3 different sha1s; with `RAYON_NUM_THREADS=1`
  four packs were identical. The config trial already pinned 1 thread. paktx sets it and
  double-packs to prove it.
- `overlay_builder.build_overlay` does not check that the mod's compression block size
  equals the base's. Its self-verification would probably catch a mismatch, but paktx
  now refuses explicitly (`mod_block_size_differs_from_base`).
- The builder's `_OffsetView` returns the wrong bytes (no error) for a read below the
  partition-1 boundary, because of a negative slice. paktx never uses it, and the replay
  checks that every compared block sits in partition 1 first.
- The builder's own tests write into `pak-overlay/` (the SUSPECT-file mechanism). paktx
  loads the builder from verified bytes and writes only under `--work`.

## 6. Prior ChatGPT cap07 work, re-checked

Reused only as cross-checks, all re-derived: container split 66/2/59 (agrees) and the
2-package projection size 123,312 B (agrees). I used none of its code, and none of its
receipts as evidence. The two SUSPECT files were not opened as evidence.

## 7. Open questions / limits

1. Engine acceptance of a derived index in a **second** container (`pakchunk10`) and of a
   cross-container import into new packages: the launch answers it.
2. `.ucas` integrity is gated by size only (128 GB cannot be hashed over FTP). paktx
   never writes them.
3. The paks folder state after 2026-10-08 is unknown. The repair dry run shows it, and
   refuses on anything it cannot prove.
4. The fixture ContainerHeader bytes come from the builder's serializer. The real-data
   replay uses real header bytes.
5. `tests/output/leon-probe-preview.lua` is a preview. Regenerate the probe from the real
   plan (its sha12 is embedded).
6. Step 2 (no-file-change mount) is separate. paktx's record/gate/repair can serve it too.
7. Compressed IoStore mods are not covered (review N1): the pinned builder's `parse_toc`
   admits only containers with `compression_method_name_count == 0`, and `project_mod`
   writes no compression-name section. Leon and the Townfall bases qualify. A compressed
   mod (common on Nexus) is refused with `unsupported_compression_methods`: fail closed,
   but a per-engine coverage gap to close in a later step.
8. Single-source hashes (review N6): the sha256 of `pakchunk1/10/11/12-ps5.pak` and
   `global.utoc` will come from one source only (today's file, trusted by size + date; no
   experiment touched them and no `.bak` exists). APPLY step 3 records that in the
   record's `provenance` (`single_source`).
9. The record stores the header path relative to the record's folder (review N8), so the
   record sha no longer depends on where the folders sit.
