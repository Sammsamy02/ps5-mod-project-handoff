# Unreal container survey and first mod experiment

Status: accessible Unreal containers classified; real primary/secondary indexes verified; a 6,648-byte original-asset PAK built and validated locally by the full shared backend. Large unselected metadata is supported, and the composed backend now streams uncompressed selections through its existing 4 MiB ceiling. Both updated PS5 callers compile and link. No extra container has been installed or mounted in-game.

Library/UI development and new product infrastructure are paused. This work uses generic format readers and existing upstream tools; title paths and observed format settings stay in data/profiles.

## Accessible game content

| Observed game dump | Container type/version | Compression evidence | Encryption evidence | Signing evidence | Test readiness |
| --- | --- | --- | --- | --- | --- |
| Stellar Blade 01.014.001 | Four legacy PAK v11 files and five IoStore TOCs v3 | PAK: all 4,544 compact records declare None. TOCs: implicit None; small tables fully checked, large tables sampled at first/last records. | All four PAK encrypted-index flags clear; all five TOC encrypted flags clear. | All TOC signed flags clear. PAK footer has no signature field; signature enforcement unknown. | Best currently accessible Unreal candidate; three original files recovered and locally repacked. A playable game and engine mount observation are still needed. |
| ASTRO BOT 01.018.000 | Not classified as Unreal from inspected directories. Top-level AMPR volumes are a separate packaging layer. | AMPRPAK4 index / AMPRDAT3 volume headers observed; underlying engine codecs remain unknown. | Unknown; no Unreal headers identified. | Unknown. | No Unreal test recommendation from this evidence. |
| Other installed/cache candidates | Game content unavailable through inspected mounted/external routes. | Unknown | Unknown | Unknown | Not surveyed as readable Unreal titles. |

Titles and update numbers above are dump labels, with Stellar Blade matching the version supplied by the user. They are not an independently decoded installed/running inventory. `/user/app` exposed 45 directory entries (including homebrew/system candidates), while the mounted-content and external-storage routes returned FTP 550. This is a partial accessible-content survey, not a survey of every installed Unreal title.

## Confirmed inner files

| File | Type | Version | Container flags / index encryption |
| --- | --- | --- | --- |
| global.utoc | IoStore TOC | 3 | 0x00 |
| pakchunk0-ps5.utoc | IoStore TOC | 3 | 0x08: Indexed only |
| pakchunk0optional-ps5.utoc | IoStore TOC | 3 | 0x08: Indexed only |
| pakchunk1-ps5.utoc | IoStore TOC | 3 | 0x08: Indexed only |
| pakchunk1optional-ps5.utoc | IoStore TOC | 3 | 0x08: Indexed only |
| pakchunk0-ps5.pak | Legacy PAK | 11 | Encrypted index: false |
| pakchunk0optional-ps5.pak | Legacy PAK | 11 | Encrypted index: false |
| pakchunk1-ps5.pak | Legacy PAK | 11 | Encrypted index: false |
| pakchunk1optional-ps5.pak | Legacy PAK | 11 | Encrypted index: false |

Five 144-byte TOC headers and four 262-byte PAK tails were captured over ordinary read-only FTP (1,768 bytes total). Their hashes, magic and relevant fields were independently checked locally. This does not establish whole-container integrity, all payload codecs, platform asset compatibility or the game's acceptance of an extra container.

## Actual compatibility result

The shared C PAK v11 reader parsed all four captured primary indexes and matched their declared SHA1s. The main archive declares 4,544 files and mount point `../../../`; the three other PAKs declare zero files. Its 131,453-byte path-hash index and 114,870-byte directory index both match their declared SHA1s. The directory has 270 folders and 4,544 distinct compact-record references. All compact records declare None and clear entry encryption flags. This tests real PS5 container bytes with the project's shared native backend, without game-specific code. Three selected compact records were also decoded by the C entry decoder; their original physical headers and payload SHA1s match. This does not validate every physical entry or mount the game.

The composed backend initially rejected this archive because it applied its 4 MiB selected-payload cap to every metadata record. There are 33 larger files, including a 111,416,386-byte entry in the second compact record. The shared coordinator now validates those unselected metadata sizes without allocating their payloads, while preserving the selected-payload cap. It validates all 4,544 real records and extracts the selected 37-byte original exactly. Selecting the large entry rejects it before any physical header/payload request. The portable regression failed in three places before the fix and now passes 10,232 checks; all eight actual-corpus expectations pass. The updated 20-unit caller also compiles and links for PS5; it has not been executed on the console.

The content split limits what the first experiment can demonstrate. All 4,544 classic-PAK filenames were classified: 3,438 `.res`, 458 `.txt`, 282 `.png`, 108 `.uplugin`, 47 `.ini` and other loose resources/sidecars; none has a `.uasset`, `.umap`, `.uexp`, `.ubulk` or `.uptnl` suffix. The four captured IoStore directory inventories contain 221,362 filename references: 195,759 `.uasset`, 23,908 `.ubulk`, 1,688 `.umap` and seven `.uptnl`. These are per-container references, not deduplicated assets or a measure of Nexus coverage. Filenames alone do not establish decoded content or runtime consumers. Our three-file PAK tests the loose-file path; ordinary cooked-package replacements still need separate container and platform-asset acceptance evidence.

One concrete loose-file limitation was also identified: the 29 `.locres` entries declare 54,859,521 uncompressed bytes in total, with a largest entry of 2,742,265 bytes. Across the main PAK, 48 entries exceed 1 MiB while fitting the 4 MiB output ceiling. The composed backend now handles this size window through its existing bounded read/hash/copy loop, preserving all output, source-budget and Zlib block limits. A separate synthetic regression first failed at reconciliation, then exposed a second payload cap; the promoted two-check change passes all 21 cases, including exact 4 MiB bytes and full rollback after a late failure. The original 10,232 checks also pass, and both callers compile/link for PS5. No large original payload has been retrieved or validated, so this is format/backend evidence rather than localization or font mod acceptance.

## First experiment

An unchanged upstream repak build now runs on the Mac. It packed three byte-preserved original files: a 37-byte INI, a 2,819-byte PNG and a 2,999-byte PNG. The extra PAK is 6,648 bytes, version 11, mount `../../../`, uncompressed, using the observed path-hash seed. All three files roundtrip exactly with upstream extraction and the existing generic C raw extraction adapter. The original containers were not modified.

The local artifact is [original-asset-format-spike.pak](../.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/artifact/original-asset-format-spike.pak). It is a format experiment from this source dump, not an install recommendation for another game. The full composed C backend now validates its metadata and all three original selections exactly. A one-byte corrupt payload is returned successfully by upstream extraction and the older raw C extraction adapter, but the composed backend returns `CHECKSUM_MISMATCH` and clears all output. Stored SHA1 checks establish consistency, not authenticity. Neither raw extraction API supplies installation authority.

A successful local pack/extract round trip proves tool/container handling only. The in-game success criterion is a recorded engine mount plus original-asset origin, or an observable substitution using original assets. A no-effect duplicate file, a directory listing, or a filesystem open counter alone does not prove engine mounting.

A focused offline lookup check also matches all 4,544 captured filename-hash/compact-reference pairs and all three pairs in the generated PAK using unchanged pinned repak hashing code. An independent Python calculation agrees. Wrong seed, case-sensitive hashing, mount-prefixed paths and a rehashed one-pair mutation produce the expected mismatches; invalid compact references are rejected. Both inventories contain only ASCII paths, so this does not validate arbitrary Unicode lookup rules. The actual path index ends with a pruned directory map containing 3,684 rows; repak writes an empty pruned map alongside a complete full directory index. Whether the retail engine accepts that enumeration layout remains part of the in-game test. No lookup mismatch was found that warrants another parser change before that test; the native backend still reports path-hash semantics as unverified.

The user has now put the PS5 in rest mode. No further console connections are planned until the user says it is online. No game is currently ready for the in-game experiment. A launchable test title and an observable mount/asset result remain required; another game can be the first test if its content becomes accessible. No unsupported PC asset, Windows DLL or runtime mod is automatically installable from these findings.

## Evidence and source schema

Private local capture and classification records are retained under `.superpowers/native-dependency-spike/unreal-library-container-survey-1/`; no raw game assets are published in this report. `root-validation/header-crosscheck.json` records independent header interpretation. `root-validation/compile-receipt.json` records rebuilding the existing copied-range caller against the generic PAK reader, used for the actual primary indexes. `root-validation/actual-primary-results.json` records all four successful C-reader results. `reader-gap/regression-red/` and `regression-green/` retain the metadata regression; `reader-gap/promoted/receipt.json` records eight actual-corpus expectations at that promotion checkpoint. `reader-gap/promoted-native-link/receipt.json` records its PS5 compile/link only. `reader-gap/asset-population/receipt.json` reconciles the complete captured filename populations and classic-PAK declared sizes. `reader-gap/large-none/promoted-host/receipt.json` records the current two-caller host checks, and `promoted-native/receipt.json` records current PS5 compile/link only.

Header geometry follows pinned CUE4Parse and the existing v11 reader. Existing-tool research follows pinned [repak](https://github.com/trumank/repak), [retoc](https://github.com/trumank/retoc) and [AMPR asset-pack format](https://github.com/drakmor/ampr_emu/blob/cfa85df379f6eeeb165d7badf9b648e266fe77b7/docs/ASSET_PACKS.md). The captured AMPRIDX3 logical inventory declares five UCAS paths. Its 418 records match the AMPRPAK4 file count; all four volume headers and the runtime profile match the index build ID. The full file-to-chunk map, original UCAS bytes and runtime activation remain unverified. `.pak` extensions alone do not identify an Unreal archive.

## Offline work for tomorrow

The original-asset PAK, corrupted-payload control and real large archive's captured metadata passed their focused checks at the metadata-fix checkpoint. Current host checks cover None selections through 4 MiB; Zlib blocks retain their 1 MiB limit. Larger payload staging, other codecs, cooked-asset conversion and runtime tools remain unfinished. The next console experiment needs an observable in-game container mount and asset origin. Until then, work targets existing mod backends and source-grounded runtime port experiments; the library browser and UI remain paused.

The latest lookup experiment is recorded in `reader-gap/path-hash-research/receipt.json`, with independent checks in `root-validation/path-hash-independent.json` and `path-hash-receipt-review.json`. It found no blocker in the captured filename hash pairs; it did not execute or change the native backend, contact the console, or establish installation, engine mounting or Nexus mod compatibility.

Broad engine/format backends and the Nexus storefront remain requirements. UE4SS, me3 and Windows DLL support are research/porting work, not established PS5 compatibility. There is no measured 99% support result. Overnight local follow-ups have been scheduled for this same thread.
