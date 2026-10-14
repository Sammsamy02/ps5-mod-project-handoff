# Numeric UE4 research-era correction

Preferred current recipe: SCANNER-INPUT-STAGE.md combines this correction with validated ELF input geometry. Choose one recipe against the original; do not apply both.

Mac-only, staged utility change. Original engine_match.py compares version strings with float(). Its actual replay sends 4.7,4.8 and4.9 to the >=4.23 research label, and selects that era when the minor version is missing. `version-era-original-replay.json` records this behavior and source SHA. No real UE4 executable or anchor was supplied.

`stage_version_era.py` requires original SHA256 `10497b76d4402ca220d19ca64217c95665d584856ce75a1def6a30aee2e09920` and one exact function boundary. It derives one replacement using integer major/minor ordering, retains the existing 4.23 research boundary and refuses to select an era for unknown/malformed UE4 versions. It removes unsupported object-array-era statements from that function. The historical `ue4-chunked` label remains for caller compatibility; its text explicitly requires separate object-array and anchor evidence. No candidate address, field, binary/layout match or runtime profile is admitted.

Dry run from this prep directory:

```sh
python3 -B cap02/stage_version_era.py
```

Optional staging creates a NEW derived directory only inside cap02, with engine_match.py and its receipt:

```sh
python3 -B cap02/stage_version_era.py --write-dir cap02/derived-version-era
```

That optional command was exercised only in temporary test folders, then cleaned. No durable derived scanner was created and no original/live file was changed. Source drift, second application, an existing destination or a destination outside cap02 refuse. Claude must review consumer label behavior and integrate later; no automatic profile pipeline was changed. This creates no build/launch dependency.

Eight version-era tests execute the actual derived module and staging CLI. They cover one-digit minors, the existing boundary, unknown/malformed versions, unchanged other families, original preservation, source drift/idempotence and confined new-copy behavior. The red log reproduces the wrong behavior before correction; its green log shows eight passes. The current combined cap02 suite has19 tests, including seven input-geometry tests. Existing synthetic engine classification tests remain separate from real UE4 anchor support.
