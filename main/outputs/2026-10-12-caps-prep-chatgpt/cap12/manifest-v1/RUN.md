# Run

From repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap12/manifest-v1 -p test_manifest.py -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap12/manifest-v1/manifest_v1.py --payload outputs/2026-10-12-caps-prep-chatgpt/cap12/manifest-v1/synthetic-fixture/fixture.bin --receipt outputs/2026-10-12-caps-prep-chatgpt/cap12/manifest-v1/synthetic-fixture/receipt.json --chunk-id 010000000000000000000008 --out outputs/2026-10-12-caps-prep-chatgpt/cap12/manifest-v1/NEW-SYNTHETIC-REPLAY.json
```

The second command must choose a new filename; existing output refuses. It reproduces synthetic metadata only. For a future real intake, replace --payload, --receipt and --chunk-id with the chosen verified .bin, existing intake receipt.json and exact type8 chunk ID. Leave originals intact; choose a new output file inside manifest-v1/. No UCAS argument or acquisition operation exists.

The pinned format sources are ../manifest-format-audit/public-retoc-source/{shader_library.rs,ser.rs,lib.rs,LICENSE}; SHA256/revision provenance is in ../manifest-format-audit/public-source-receipt.json. MANIFEST.sha256 in this folder records the completed utility/test/fixture hashes. No broad repository suite or compiler was run.

Latest verified replay is synthetic-replay-bounded.json (15-test loading-fix suite). verification.json records utility/test/replay hashes; existing output files remain untouched by the CLI.
