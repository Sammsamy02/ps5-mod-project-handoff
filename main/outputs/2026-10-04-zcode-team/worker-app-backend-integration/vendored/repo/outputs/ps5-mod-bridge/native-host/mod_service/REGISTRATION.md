# Native registration adapter

`mb_mod_service_register_download` is a trusted, serialized native call. It
accepts a durable download ID and an explicit selected PPSA/CUSA title ID.
The owner must establish that association; title spelling is not installed,
running, build, or compatibility evidence. The adapter does not expose HTTP
capabilities, contact Nexus, query `game.status`, prepare, install, or change
game files.

The adapter checks `service.describe` for `modforge-service/1` and advertised
`records.list` and `mod.register_download` methods, then reads existing records.
It performs one full durable-store lookup and archive hash after these reads.
The store supplies the archive path and Nexus source identity. A matching
download/title/source is a no-op, including prepared or installed records.
Conflicting identity, malformed rows, or duplicate download IDs refuse the
operation. The owner must serialize this entire sequence against other
backend and store mutations.

Optional metadata comes only from the native authenticated metadata owner:

```json
{
  "source": {"name": "Display name", "author": "Author", "version": "1.0"},
  "requirements": {
    "game": "domain", "mod_id": 1, "discovery_complete": true,
    "nexus_requirements": [], "dlc_requirements": [],
    "file_version_constraints_provided": false, "installation_supported": false
  }
}
```

All three source display fields are optional. Identity fields cannot be
overridden. Requirements must match the retained source and the normalized
schema, including unresolved typed relation rows. Missing metadata sends
`requirements:null`. It never means a successful empty discovery. An existing
record always reports `requirements_status:"unknown"`, because `records.list`
does not reveal its stored requirements. A new registration reports complete
only when its successful response confirms the supplied normalized object.

When the native caller supplies `NULL/0`, the adapter first looks for the
immutable metadata capture saved with the original archive. It validates that
capture using the same complete schema and source identity rules as explicit
input. A missing capture stays unknown. A corrupt or unreadable capture refuses
registration; it is never silently treated as missing. Explicit metadata from
the trusted native caller takes precedence over a saved capture. Existing
registrations remain no-ops and are not silently rewritten with newer metadata.

Metadata input is bounded to 128 KiB, a backend request to 144 KiB, and each
read/write response to 256 KiB. JSON is bounded to depth 64 and 200,000 parser
events; duplicate members and embedded raw NULs are rejected. Records are
bounded to 256 unique entries, and each requirements array to 1,024 rows.
These are local adapter bounds, not an expansion of provider capabilities.

The returned app-owned JSON receipt contains only download ID, title ID,
state, allowlisted source fields, registration status, and requirements
status. It never copies the backend envelope, archive path, requirement text,
or error details. The caller frees this receipt. Every returned backend-owned
buffer with a release callback is released exactly once, including failures.
After an invalid registration response the backend may already have stored
the record; retry performs the same read-before-write reconciliation.

Run the offline real-store/fake-backend controls with:

```sh
python3 outputs/ps5-mod-bridge/native-host/tests/test_mod_service_register.py
```
