# Replay and tests

From repo root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap08/script-objects -p test_resolve_classes.py -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap08/script-objects/resolve_classes.py --identity outputs/2026-10-12-caps-prep-chatgpt/cap08/leon-zen-identities.json --profile .superpowers/native-dependency-spike/zen-script-import-resolver-1/profile.json --capture-receipt .superpowers/native-observations/townfall-global-scriptobjects-1/capture-receipt.json --manifest .superpowers/native-observations/townfall-manifest-capture-2026-10-01/package.manifest --toc .superpowers/native-observations/townfall-manifest-capture-2026-10-01/global.utoc --ucas .superpowers/native-observations/townfall-global-scriptobjects-1/global.ucas --parser outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py --decoder .superpowers/native-dependency-spike/zen-script-import-resolver-1/zen_script_objects.py --decoder-readme .superpowers/native-dependency-spike/zen-script-import-resolver-1/README.md --out outputs/2026-10-12-caps-prep-chatgpt/cap08/script-objects/leon-script-class-resolution.json
```

Only small captured metadata is read (8MiB cap per input). Root approved the 3.4MB metadata replay alongside the separate large-image job. No console/network, payload acquisition, game executable analysis, compiler/install, live edits, Epic/Sony source copying or payload duplication.
