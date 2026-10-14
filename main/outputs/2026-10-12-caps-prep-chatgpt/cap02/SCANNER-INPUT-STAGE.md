# Combined scanner input correction — Mac-only

The pinned original scanner can report VERSION_CONFIDENT/4.27 on synthetic inputs declaring ELF32, big-endian encoding or an invalid identification version, and on PT_LOAD ranges extending past EOF or larger than their memory ranges. A short program-header stride escapes as struct.error rather than the CLI's controlled input error. `elf-geometry-original-replay.json` reproduces these cases with source/input hashes. These are synthetic classification failures; they do not establish real UE4 support.

`stage_scanner_inputs.py` is the preferred staging entrypoint. It consumes the exact original SHA256 `10497b76d4402ca220d19ca64217c95665d584856ce75a1def6a30aee2e09920`, reuses the previously tested numeric-version correction, and replaces the image parser once. The resulting scanner supports ELF64, little-endian, version1 and x86-64; checks the known header size, program-table bounds/stride and consumed PT_LOAD ranges, sizes, virtual extent and declared alignment; and returns ValueError for malformed/unsupported geometry. Padded program-header entries retain their stride while only their known56-byte prefixes are read. SELF inputs still refuse.

The structural rules follow the primary [ELF header specification](https://gabi.xinuos.com/elf/02-eheader.html) and [program-header specification](https://gabi.xinuos.com/elf/07-pheader.html). This reader deliberately refuses unsupported header sizes and extended program-header counts. It does not validate every ELF structure, loadability, signatures, unknown platform extensions or runtime anchors. It does not fix every unrelated original CLI error path. No game-specific address or page size is introduced.

From this prep folder:

```sh
python3 -B cap02/stage_scanner_inputs.py
```

Optional later staging writes a NEW derived tree inside cap02:

```sh
python3 -B cap02/stage_scanner_inputs.py --write-dir cap02/derived-scanner-inputs
```

Choose this combined recipe OR the older version-only recipe. Do not apply both: each consumes the original hash and refuses a second application. The combined receipt reports1 input and2 exact function replacements. No original scanner, profile pipeline or runtime file is edited. Temporary tests exercised new-copy behavior; no durable derived tree was created. Claude reviews/integrates later; B7/hooks/15-fields do not depend on this work.

Seven new tests exercise the actual derived parser and CLI. The recorded red run shows absent validation and uncaught struct errors; the green full cap02 run has19 passing tests. `elf-geometry-staged-replay.json` replays the exact synthetic inputs: valid control retains VERSION_CONFIDENT/4.27, all six malformed variants refuse, and AST comparison changes only parse_image and anchor_pack_recommendation. The combined parser also accepts the pinned194,241,424-byte saved Townfall image, returns five PT_LOAD spans and matches its full SHA256 (saved-image-input-replay.json); that replay does not classify the image or admit any anchor. Original source bytes remain unchanged. The retained original marker-loader emits an unclosed-file ResourceWarning in the characterization test; it is recorded in the log and does not affect these results.
