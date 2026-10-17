# Stellar Blade content-directory evidence

The user's reopened FTP service exposed a concrete game-content candidate at `/data/Stellar Blade 01.014.001 PPSA13197`. This resolves the earlier limited internal-base discovery's missing title candidate, without promoting it to an installed or running game fingerprint.

Independent read-only discovery at 2026-09-30T14:32:53Z used 16 bounded path reads (15 LIST, one JSON RETR), totaling 9,470 bytes of listing/metadata, with 3-second per-operation timeouts and no bound exceeded. It sent no SITE/SELF/KILL/MTRW commands and changed no service, configuration or console file. Root separately re-read the exact metadata and matched the size/hash.

Metadata path: `/data/Stellar Blade 01.014.001 PPSA13197/sce_sys/param.json`; 3,023 bytes; SHA-256 `260f1d9d7e9e00a7f7573c7c154219e842b7528b0c2b64c818e5cb1f718f0a4a`.

| Field | Exact observed value |
| --- | --- |
| titleId | PPSA13197 |
| contentId | EP9000-PPSA13197_00-STELLARBLADE0000 |
| contentVersion | 01.014.001 |
| masterVersion | 01.00 |
| conceptId | 10006891 |
| en-GB title | Stellar Blade |

Directory listings include `eboot.bin`, `sce_sys`, `sb`, `engine` and AMPR-named bulk/index files. `/sb/content` contains `paks`, `images` and `movies`; `/sb/platforms/ps5` exists. These names establish content-directory evidence beyond an appmeta cache. No executable or large asset body was downloaded for this discovery.

Observed `/sb/content/paks` entries:

| File | Bytes reported by FTP LIST |
| --- | ---: |
| global.utoc | 597 |
| pakchunk0-ps5.pak | 1,273,878,395 |
| pakchunk0-ps5.utoc | 22,876,847 |
| pakchunk0optional-ps5.pak | 339 |
| pakchunk0optional-ps5.utoc | 6,565 |
| pakchunk1-ps5.pak | 339 |
| pakchunk1-ps5.utoc | 14,632,736 |
| pakchunk1optional-ps5.pak | 339 |
| pakchunk1optional-ps5.utoc | 99,739 |

Limited `/mnt/sandbox` listing contained system/NPXS directories, `pfsmnt` was empty, `/mnt/ext0`, `usb0` and `usb1` were empty, and `/mnt/pfs` showed trophy content. Discovery stopped after finding the requested title candidate; other USB/sandbox roots were not exhausted.

Next native route: read-only title detection should discover the same metadata locally, preserve raw version/title/content identity and metadata hash, and identify the asset-directory candidate. Listing sizes/names do not prove archive format, encryption, active mount, executable version, game ownership or acceptance. PAK/IoStore/AMPR requirements need their own bounded format audit. PS5 writers, active build fingerprints, runtime hooks and installation remain unresolved.
