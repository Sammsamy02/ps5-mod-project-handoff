# Mac cleanup and PS5 storage ranking

## Completed Mac-only cleanup

- Removed 173 byte-identical old build intermediate/link-map copies: 20.62GiB of reported allocated file blocks.
- Rehashed all 58 retained originals after removal; full SHA256 mapping preserved in DUPLICATES-DELETED.jsonl. Removed only `.o` and `runtime.map` from old l10–l12 `-aside-` snapshots. No source, PRX, receipts, unique artifacts or current builds removed. Exact paths can be recreated by copying the mapped surviving files.
- Removed 17898 old browser-cache files: 0.94GiB reported allocated blocks. Cache indices/locks, recent files, cookies/authentication/session stores retained.
- All saved conversations and SQLite databases retained. Toolchains, user attachments, current l23/l24/l25 work, and active ZCode/Claude scratch files retained.
- Mac available storage was 2.8GiB at start; measured 24.12GiB at 2026-10-08T20:19:35.261864+00:00. APFS/concurrent activity means net free-space change need not equal summed file sizes.
- No PS5 contact or modifications in this cleanup.

## Why it grew

The Codex worktrees contained about145GiB of project material. Repeated builds retained many old snapshots, each with ~113MiB compiled runtime objects and ~140MiB linker maps, often identical. Saved conversations separately occupied52.47GiB, with50.21GiB in July2026. In the largest sampled July21 transcript (2,440,867,721 bytes /2.27GiB),14,933 records included75 compaction records occupying93% of the file. Embedded base64 images comprised98% of file bytes, with1,936 image occurrences. This demonstrates repeated image payloads dominate that file; it does not establish the exact proportion in every transcript. All six inspected multi-GB July21 files carry fork metadata. History was not deleted.

## Largest PS5 content, from the saved read-only scan

| Separate path under `/data/homebrew` | GiB logical |
|---|---:|
| Silent Hill Townfall PPSA33286 |128.88|
| PPSA07411.exfat |125.23|
| CONTROL Resonant folder / PPSA34547 |90.96|
| PPSA13197-app0 |89.23|
| ASTRO BOT / PPSA21567 |68.88|
| PPSA03747.exfat |50.47|

These six account for553.66GiB of558.64GiB in that directory. Nested game assets are included, not added again. The two image titles were not verified. Other separate categories: saved media14.76GiB, download files7.23GiB, installed application package area23.86GiB. Logical sizes from FTP are not allocated blocks or guaranteed reclaimable space. No PS5 files were deleted; no causal claim about recent growth is established by this one snapshot.
