# Replay

From repo root:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap08/base-identity-audit -p test_audit.py -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap08/base-identity-audit/audit.py --out outputs/2026-10-12-caps-prep-chatgpt/cap08/base-identity-audit/base-input-audit.json
```

The adapter uses saved source-pinned cap07/cap08 evidence and only writes the specified JSON under this folder. Large-TOC authorization is specific: <=8MiB aggregate random metadata reads, streaming SHA256 in <=1MiB buffers, no whole-file materialization, no UCAS reads. Every requested row is lookup-checked and records exact ID/length/expected IoHash/owner/block descriptors. Small metadata files are <=8MiB and their hashes are rechecked before output.

Next integration action: obtain provenance-complete already-local inputs matching the receipt's required base ExportBundleData and chunk10 ContainerHeader IDs/hashes, then decode actual export names/classes/public hashes/imports and compare with the verified mod identities. Raw partial header candidates cannot substitute for this. No acquisition is attempted or authorized by the tool. No compatibility, visual fallback, runtime mount or native shader result is implied.
