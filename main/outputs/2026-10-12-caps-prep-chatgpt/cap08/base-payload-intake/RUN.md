# Replay

From repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap08/base-payload-intake -p test_intake.py -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap08/base-payload-intake/intake.py --job outputs/2026-10-12-caps-prep-chatgpt/cap08/base-payload-intake/captured-store-replay.job.json --out outputs/2026-10-12-caps-prep-chatgpt/cap08/base-payload-intake/captured-store-replay.json
```

captured-store-replay.job.json is immediately runnable with the retained source files. It reads the already-bound small chunk0 ContainerHeader .bin only, not an asset UCAS.

export-intake.example.json pins the actual audit and readers and contains exact selectors for one missing external base export and its chunk10 owner store. Its source paths/hashes are explicit placeholders; it is intentionally not runnable until those complete decoded chunks exist locally. Replace both path/SHA fields with supplied whole decoded .bin files; do not substitute raw UCAS, raw stored blocks, partial headers or slices. Then invoke intake.py with --job pointing to that saved job and --out to a new JSON in this directory. Repeat with exact audited selectors for other targets; split jobs when the aggregate8MiB limit requires it, including the verified owner store in each export job.

Each target selector contains exactly owner_toc_sha256, owner_container_id_u64, toc_row and chunk_id. Each source contains path and sha256. Missing geometry/IoHash is never supplied by the job: it must come from the hash-pinned audit receipt. The example does not authorize acquiring inputs. Existing quarantined partial dependency headers are still inadmissible.

No compatibility promotion follows from an identity match. Next integration action remains supplying provenance-complete base export chunks and the chunk10 ContainerHeader that match the audit's required hashes, then inspecting the emitted actual identities and unresolved references.
