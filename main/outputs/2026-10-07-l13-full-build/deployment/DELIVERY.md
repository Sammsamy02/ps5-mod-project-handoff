# Single-PRX delivery and rollback — HOLD

`prx_delivery.py` is prepared locally. No real transfer has been run. The current authorization covers local builds only; the user must separately approve upload or rollback before a real phase is executed. `--execute` is required; without it every phase remains HOLD and makes no curl call. There is no approval-file workflow.

The sole remote target is hardcoded as `/data/plugins/modbridge_supplier_composition.prx` on `ps5-host-not-configured.invalid:2121`. Configurations, game executables, payload loading, launches and remote deletions are outside this helper. The broken `/tmp/l12-upload/ROLLBACK.sh` is neither sourced nor reused.

The build owner must create the final manifest after the two candidate builds and their gates. Required JSON fields:

```json
{
  "schema": 1,
  "remote_path": "/data/plugins/modbridge_supplier_composition.prx",
  "candidate": {
    "path": "/absolute/path/to/the/final/signed/modbridge_supplier_composition.prx",
    "sha256": "64 lowercase hex characters from the final artifact",
    "bytes": 0
  },
  "expected_remote_sha256": "64 lowercase hex characters for the approved current remote preimage"
}
```

The example is intentionally not an executable manifest. `bytes` must be the actual file size. The current raw l12 hash previously verified in the retained read-only harvest is `024a4143bb84333e4552814fd68e67f6f72f7b04630dcd2b4a512b44d8bc34b4`; the helper compares a fresh decrypt-off preimage against the manifest before uploading, so unexpected remote drift stops the phase.

Default preview:

```sh
python3 prx_delivery.py
python3 prx_delivery.py upload --manifest /absolute/path/to/candidate-manifest.json
```

After separate user approval for that phase:

```sh
python3 prx_delivery.py upload --manifest /absolute/path/to/candidate-manifest.json --execute
```

The helper prints the newly generated `backup.json` and final phase receipt paths. Each execution uses a timestamp and random UUID directory beneath `deployment/jobs`, never reusing prior backup files. It freezes the validated local replacement snapshot, captures a fresh remote preimage, and writes an immutable `backup.json` before its single PUT. Every executable GET sends `--quote "SITE SELF"` on that same curl invocation, records the FTP response headers, and requires `226 SELF transfer mode disabled`. It verifies SHA-256, byte count and signed-container magic before accepting a replacement/readback. The remote preimage and postimage are both recorded. Curl config loading is disabled, the URL is fixed, and there are no mutation retries.

Rollback preview, using the exact checkpoint printed by the upload:

```sh
python3 prx_delivery.py rollback --backup-receipt /absolute/path/to/upload-.../backup.json
```

After separate user approval for rollback:

```sh
python3 prx_delivery.py rollback --backup-receipt /absolute/path/to/upload-.../backup.json --execute
```

Rollback verifies the saved backup's SHA-256, size and signed magic before any curl call. It also saves a fresh decrypt-off snapshot of the current remote PRX before restoration, so rollback has its own separate phase evidence. It then restores the validated old bytes with one PUT and verifies a fresh decrypt-off readback. No failed upload triggers rollback automatically. An interrupted/failed PUT can leave remote state uncertain: preserve its phase files and inspect them before requesting the next phase.

Executed phase receipts are created once with exclusive file creation and then marked read-only. Preflight refusals occur before any remote operation. `--curl` exists for local stub testing; ordinary use leaves the `/usr/bin/curl` default.

Local verification:

```sh
python3 -m unittest discover -s tests -v
```

Nine tests pass using an executable local curl stub and a local file standing in for the remote PRX. They verify default HOLD, upload/rollback argv, same-connection decrypt-off, exact path restriction, no retries, unique phase evidence, corrupt-backup rejection, unexpected preimage rejection, missing decrypt-off acknowledgment refusal, and post-upload hash failure without retry or automatic rollback. This is local command/behavior validation, not a console transfer test.
