# Optional static service startup

The ordinary app build leaves ModForge disabled. To link a retained backend,
pass both `--mod-service-profile /absolute/service-profile.json` and
`--mod-service-config /absolute/service-config.json` to `native-host/build.py`.
These options do not require OAuth configuration. Native builds still need
the app's existing verified SDK/crypto or download dependency inputs.

The profile is an explicit archive descriptor:

```json
{
  "schema_version": 1,
  "target": "host",
  "service_archive": {
    "path": "/absolute/libmodforge_service_reference.a",
    "sha256": "REPLACE_WITH_64_LOWERCASE_HEX_SHA256_DIGEST"
  },
  "dependency_archives": []
}
```

Use `host` for `--profile desktop_fixture`, or `native` for `loopback` and
`development_lan`. Optional dependency archives have the same `path` and
`sha256` fields, in link order. The primary archive is force-loaded to retain
static method registrars; dependencies follow it before the app's existing
crypto archives. The builder accepts no extra shell, compiler, or linker
flags in this descriptor. Supply compatible self-contained static archives;
thin archives, mismatched hashes, duplicate paths, and unknown fields fail.
The builder does not rebuild backend sources or add another SDK/crypto stack.

The service configuration is a bounded JSON object:

```json
{
  "state_dir": "/data/modforge",
  "mod_root_base": "/data/modbridge",
  "data_dir": "/data/modforge/share",
  "limits": {
    "max_archive_entries": 10000,
    "max_archive_bytes": 4294967296,
    "max_entry_bytes": 2147483648,
    "max_jobs": 2
  }
}
```

`state_dir` and `mod_root_base` are required and must differ. `data_dir` and
`limits` are optional. Paths must be absolute, normalized UTF-8 strings of at
most 1024 bytes without control characters, repeated separators, `.` or `..`
segments, or a trailing separator. They are trusted build inputs, never
browser-provided values, and need not exist on the build host. The builder
does not create these device paths. Only the four documented positive integer
limit fields are accepted. Config files are capped at 16 KiB and duplicate
JSON keys are refused. See [the backend contract](vendor/SERVICE.md) for
runtime directory ownership and limit semantics.

For a desktop build, supply paths under an owned local fixture directory:

```sh
python3 outputs/ps5-mod-bridge/native-host/build.py \
  --profile desktop_fixture \
  --mod-service-profile /absolute/service-profile.json \
  --mod-service-config /absolute/service-config.json \
  --output-root /absolute/owned-build-output
```

The generated header sets `MB_HOST_MOD_SERVICE` to `0` or `1` and embeds
canonical compact configuration in `MB_HOST_MOD_SERVICE_CONFIG_JSON`. The
descriptor, original config bytes, canonical config, archive hashes, and
target enter the manifest and build identity. The builder verifies the same
files again immediately before linking and refuses intervening changes.

Enabled startup opens one service before listening and validates its
`modforge-service/1` description and advertised `records.list` method. A
failed open or incompatible description refuses startup. The host owns the
handle; HTTP and registration callers borrow the validated bridge. Shutdown
detaches the bridge and closes the service before the download store closes.
Read-only service HTTP access still requires the app's local browser session;
linking the backend alone does not create a session or an account. Registration
also requires its advertised method and the configured durable store.

Run the offline build checks with:

```sh
python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_mod_service_build.py
python3 -m unittest discover -s outputs/ps5-mod-bridge/native-host/tests -p test_build.py
```

The registrar test compares ordinary static linking against force-loading and
executes only owned local fixture code. A linked native artifact establishes
compile/link evidence; it does not establish console or game execution.
