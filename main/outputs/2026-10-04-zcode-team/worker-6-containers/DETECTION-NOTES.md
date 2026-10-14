# Detection & handling notes — original containers (worker-6-containers, 2026-10-04)

All items below are grounded in bytes actually observed in this pass or in cited checkpoint receipts.
Where engine semantics are unknown, that is stated as a gap — no guessed byte rewrites.

## 1. Container generations actually observed

| Generation | Observed where | Facts |
|---|---|---|
| PAK v11 (UE5-era loose-file) | All 6 saved paks (`evidence/pak-probe.json`) | Footer at EOF (~204 B window): magic 0x5A6F12E1, version 11, index offset/size, index SHA-1, frozen-index byte = 0, 5x32B compression-method table ALL EMPTY → payloads stored uncompressed; mount point `../../../`; index plaintext-readable |
| IoStore/Zen TOC `-==-` (0x2d3d3d2d) | All saved Townfall utocs + Leon utoc + 1_999_P.utoc | First 16 bytes are the `-==-` tag repeated 4x — this is a Zen-style TOC, NOT desktop 0xCAECA5A9. Matches receipt name `townfall-original-zen-headers-1`. retoc v0.1.5 parses/verifies this variant |
| IoStore-in-PAK wrapper | Leon `zzz_Leon_Townfall_2K_P.pak` (347 B) and `1_999_P.pak` (347 B) | Tiny v11 pak whose payload is the utoc/ucas sidecar pair; real content lives in the sidecars. Detection rule: `.pak` + sibling `.utoc`/`.ucas` with same stem ⇒ do not treat the pak as the content container |
| Desktop 0xCAECA5A9 utoc | NOT observed anywhere in saved consoles | Any intake code assuming the desktop magic will mis-detect PS5 Zen saves. This is a per-platform fact, not a guess — both magics must be probed |
| ZIP store/deflate | 8/8 Nexus originals | methods {0,8} only; no encrypted members; data-descriptor flag only on ue4ss-v1.zip and leon-2k-v1.1.zip; no zip64 in the observed set (preflight corpus covers zip64 rejection anyway) |
| RAR | one-off corpus `townfall-62-rar-intake-1` | Extraction happened historically; no general RAR reader receipt ⇒ treat RAR as gap, not capability |

Generation limits to enforce in intake:
- PAK: accept v11 (and the writer's output); v8/v9 path-hash-index layouts exist in the wild but
  have zero local evidence — mark untested rather than "supported".
- IoStore: accept Zen `-==-` and desktop `0xCAECA5A9` magics; reject others with a specific reason.
- Version fields after the Zen magic were NOT decoded this pass (hostile-to-guesses rule); the
  checkpoint's `toc_version_preserved` in the retoc receipt is the only version-fidelity fact.

## 2. Compression variants

- PAK: footer method table empty in every observed original ⇒ loose-file payloads uncompressed.
  Intake must still parse the per-entry compression block index when the table is non-empty; no
  receipt exists for compressed-block PAK on target (gap).
- PAK reader large-payload support: "None" compression extension to 4 MiB selected payloads
  (1 MiB zlib block cap) is host-proved with 10232+4138 checks; >4 MiB real payloads unread (gap).
- IoStore chunks: compression method names live in the TOC; Oodle-decoded chunks are a hard gap
  (no decoder in workspace; pinned stack has zlib/lzma only). Any chunk whose method is not
  None/zlib must fail with an explicit reason, never a silent passthrough.
- ZIP: deflate/store proved; legacy methods (implode, bzip2, ppmd, aes) unproved (gap).

## 3. Encryption / signing detection

- ZIP: flag bit 0x1 checked per member (all 8 originals: absent). Encrypted ZIPs ⇒ explicit
  unsupported reason. No AES-ZIP fixture exists (gap).
- PAK: index-encryption is detected operationally: plaintext index parse failure (bogus entry
  count / unreadable mount point) ⇒ report `possibly-encrypted-index`, do not retry blindly.
  No encrypted-PAK fixture exists (gap).
- Zen/IoStore: no signing/encryption fields decoded this pass; retoc `verify` success is the only
  integrity oracle used. Treat TOC signing semantics as unknown (gap).
- SELF/PRX: `encrypted`/`compressed` segment property bits (0x2/0x8) are already handled by the
  modforge PRX checker (assessment codes `encrypted` problem, `plain-elf`/`not-fake` warnings);
  that path is console-verified for the 4-byte SDK retarget only.

## 4. Windows-cooked vs PS5-cooked assets (open questions — engine semantics unknown)

- `originalPackageHeaders` receipt: `same_readable_cell_present_layout: true` but
  `platform_compatible: false` — the original Zen package headers are readable yet the contained
  cooked packages are not platform-compatible as-is.
- `imported_package_reader`: 127 corpus packages, 367 raw imported names, 378 bindings, native
  compile/link passed, `platform_compatible: false`, `native_probe_executed: false`.
- Observed population (main-game TOC refs): 195759 `.uasset`, 23908 `.ubulk`, 1688 `.umap`,
  7 `.uptnl` — i.e. cooked Unreal packages dominate; the passing PAK fixtures are INI/PNG loose
  files only ("scoped to loose-file container acceptance").
- Open questions (no guessing): whether Windows-cooked `.uasset`/`.ubulk` can ever be byte-laid
  into a PS5 Zen container without re-cooking; whether the engine validates per-platform
  package summary flags; whether texture LOD/virtual-texture tile layouts differ. Until a real
  conversion path exists, any Windows asset container must classify `unsupported-needs-conversion`,
  not `native`.

## 5. Operational detection rules (generic, no per-mod special cases)

1. Sniff trailer first: last 221 B for PAK magic (0x5A6F12E1/0x5A6F12E0) → parse footer.
2. Else sniff head 16 B: `-==-` tag or 0xCAECA5A9 → IoStore/Zen TOC; require sibling .ucas (and
   wrapper .pak optional) for anything beyond header detection.
3. Else ZIP central-directory scan (preflight corpus already covers hostile layouts).
4. Record per-container facts (magic, version, method table, entry count, sibling presence) into
   the build profile — not into mod-specific code.
5. Failure taxonomy must distinguish: unreadable-format / possibly-encrypted / missing-sibling /
   unsupported-compression / platform-incompatible-payload.
