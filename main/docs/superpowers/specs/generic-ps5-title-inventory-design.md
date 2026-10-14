# Generic PS5 title/build inventory design candidate

Status: source-grounded design only. No provider, UI, or planner changes were made; no console or game data was read.

## What the retained sources establish

The Python host already exposes `GET /api/detected-titles`, but returns an empty list with `status: unavailable` ([modbridge_host.py:502-503](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/companion/modbridge_host.py:502)). The native HTTP service currently exposes `/`, `/version`, `/core`, and `/report`; `/report` hard-codes game metadata as unavailable/not queried ([http.c:173-206](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/http.c:173)). Installation remains explicitly unsupported ([http.c:198-200](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/http.c:198)).

The current UI's “Choose a game” cards are Nexus catalog records labeled by Nexus domain; `chooseGame` uses that domain for catalog requests and download jobs ([app.js:82-93](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/web/app.js:82), [app.js:134-159](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/web/app.js:134)). It does not fetch `/api/detected-titles`; its saved navigation contains only Nexus domain/mod/file ([app.js:29-36](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/web/app.js:29)). Standard-controller navigation and Cross/Circle/Options actions already exist ([app.js:253-275](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/web/app.js:253)).

The retained SDK source is pinned to `ps5-payload-dev/sdk` commit `d9c9519116944a7f1c22d262012c53b80b3520b7` ([native-host/README.md:34](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/README.md:34)). Its `samples/ps/main.c` uses `sysctl(CTL_KERN, KERN_PROC, KERN_PROC_PROC)` to list running processes and manually declares `sceKernelGetAppInfo(pid, app_info_t *)`; its sample record includes `title_id[10]` ([pinned sample](https://raw.githubusercontent.com/ps5-payload-dev/sdk/d9c9519116944a7f1c22d262012c53b80b3520b7/samples/ps/main.c), local [main.c:29](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/ps/main.c:29) and [main.c:49](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/ps/main.c:49)). The same pinned revision's `samples/test_privileges/main.c` manually declares the same API but uses `title_id[14]` ([pinned sample](https://raw.githubusercontent.com/ps5-payload-dev/sdk/d9c9519116944a7f1c22d262012c53b80b3520b7/samples/test_privileges/main.c), local [main.c:49](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/samples/test_privileges/main.c:49)). These examples demonstrate a process-title observation path, not a system-wide installed-title list, title display-name lookup, installed build/version schema, or executable/content fingerprint. The conflicting sample layouts also leave the app-info ABI to validate before treating a title ID as trusted provider output. No title metadata path or schema was established by the pinned source; do not guess an SFO/JSON location.

Planner target fingerprints are currently nonempty string key/value pairs with no confidence field ([planner.cpp:16-19](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/planner.cpp:16)). Empty fingerprints produce `unknown_target` ([planner.cpp:179](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/planner.cpp:179)), and capability evidence must match current target fingerprints exactly ([planner.cpp:80-90](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/src/planner.cpp:80)). Consequently, an observed title ID, display name, Nexus domain, or human-readable version must not by itself be projected into planner fingerprints.

## Minimal provider contract

Start with a bounded, read-only **running-process observation** provider. Do not label it “installed titles.” Until the `sceKernelGetAppInfo` layout is resolved, return `unavailable` with a machine-readable reason. Once validated, emit only bounded, NUL-checked title IDs from successful calls; display name, version label, and build fingerprint stay explicitly unavailable until a source for each is independently established. A display/version label is descriptive metadata, not a build fingerprint. Add a planner fingerprint only when the provider hashes exact, relevant target bytes and records that provenance; keep title identity and measurement confidence outside the planner map.

`GET /api/detected-titles` should return one immutable snapshot. Use the same top-level names as the host/UI contract, including `source_scope` and a bounded machine-code `reason`:

```json
{
  "schema_version": 1,
  "status": "observed",
  "complete": true,
  "snapshot_id": "opaque-current-snapshot",
  "source_scope": "running_processes",
  "items": [{
    "title_key": "opaque-key-within-snapshot",
    "title_id": {"value": "…", "status": "observed", "source": "sceKernelGetAppInfo"},
    "display_name": {"value": null, "status": "unavailable", "source": null},
    "build_version": {"value": null, "status": "unavailable", "source": null},
    "build_fingerprint": {"algorithm": null, "value": null, "confidence": "unknown", "source": null}
  }],
  "reason": null,
  "issues": []
}
```

`status=observed` requires a complete capture. A successful empty observation is `observed`, `complete=true`, with a non-null snapshot ID and no items. `partial` may show observations for reference but is not planner-selectable. `unavailable` is `complete=false`, `snapshot_id=null`, `items=[]`, and a reason code; `error` covers failed or malformed reads. The host creates a new opaque snapshot ID when it materializes a capture; a title/build-source change necessarily invalidates older IDs. Include bounded issue codes, not raw paths or arbitrary error text. Bound process snapshot bytes, record count, string sizes, and total response; reject size changes, malformed/unterminated IDs, or limit overflow without publishing a partial selectable target. Native JSON serialization must fail the whole response if it exceeds the current 8 KiB body buffer ([http.h:8-9,25](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/http.h:8)). The SDK sample uses a size query followed by a second `sysctl` read and `malloc`; the provider should replace that unbounded allocation with caller-owned capped scratch and treat a changed/short second read as a failed snapshot.

Expose the platform seam as a versioned C POD callback: `int (*enumerate_v1)(void *opaque, const request_v1 *, response_v1 *)`. The request supplies caller-owned process scratch, record array, issue array, and text arena plus capacities. The response contains `struct_size`/ABI version, status/completeness, source scope/reason, and offset/length pairs into caller-owned text storage. The host creates `snapshot_id` and per-snapshot `title_key` while materializing the response. No SDK structs leak across the callback boundary, no pointers are retained after return, and no provider result mutates install/deploy capabilities. A later host bridge may serialize this trusted response; the browser supplies only a reference, never platform facts.

## Selection and planner flow

Keep `targetRef` separate from the existing Nexus `game` state. Load the title snapshot independently of Nexus catalog/account loading. Add a controller-focusable “Console target” panel that displays title ID first, optional sourced label/version, and separate “Build unknown”/confidence text. Cross selects a record only from a complete snapshot. The client carries only `{snapshot_id, title_key}` to a planning request; the trusted host resolves that pair against its current immutable snapshot and constructs planner fingerprints itself. If the snapshot changed, return `409 stale_target_ref`, refresh, clear selection, and ask for a new Cross selection. On app reload, revalidate the saved reference before restoring it.

Nexus domain remains an independent catalog choice. Never auto-map by matching names or domain strings; a Nexus label is not PS5 title/build evidence. With title ID only or unavailable build measurement, planner fingerprints remain empty and the planner's existing `unknown_target` path blocks compatibility claims. This flow is metadata inspection, not install authority.

## Focused acceptance fixtures for a later implementation

- Complete synthetic process snapshot with multiple unrelated title IDs; verify generic sorting/selection and no fixed title-ID branch.
- Repeated title ID with changed exact build bytes; verify changed fingerprint/snapshot and stale-reference rejection.
- App-info failure, conflicting/invalid/unterminated title ID, process-table size change, zero entries, and process-byte/record/text cap overflow; none may yield a selectable partial target.
- Identity-only, display-name-only, or version-label-only records leave planner fingerprints empty and resolve as `unknown_target`.
- Nexus domains with same/different display names never alter the target reference or synthesize fingerprints.
- A measured exact-byte fingerprint is constructed by the trusted host only; browser-injected fingerprint fields are ignored/rejected.

The immediate prerequisite is to resolve the pinned app-info structure/API contract. The pinned SDK does not supply evidence for installed-title enumeration or build metadata, so that broader capability remains a separate source investigation rather than an assumption in this design.
