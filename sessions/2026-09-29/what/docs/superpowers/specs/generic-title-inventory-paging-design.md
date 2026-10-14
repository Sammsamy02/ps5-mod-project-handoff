# Immutable title-inventory paging design

2026-09-30 design; 2026-10-01 implementation contract. Native inventory permissions and runtime remain unestablished; desktop paging does not establish a native provider.

Recommendation: add an explicitly versioned page transport backed by **one fully validated immutable capture**, while retaining the existing v1 small/unavailable endpoint. The UI stages every page and commits one complete list only after validation and a current-generation check. Original job references stay exactly `{snapshot_id,title_key}` with 32 lowercase hex characters each; browser metadata never supplies target authority.

## Current seams that must remain distinct

The current spec is [generic-ps5-title-inventory-design.md](/WORKSPACE/.codex/worktrees/translation-core/what/docs/superpowers/specs/generic-ps5-title-inventory-design.md), not `trusted-title-inventory-contract.md`. Read it together with the completed [primary-source investigation](/WORKSPACE/.codex/worktrees/translation-core/what/docs/superpowers/specs/ps5-title-metadata-primary-source-investigation.md).

[modbridge_inventory.py:152–196](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/companion/modbridge_inventory.py:152) caches one immutable serialized `_current`; a refresh invalidates it before capturing. `resolve()` currently calls the legacy `snapshot()` view. Materialization limits are 128 records, 32 issues and 8192 serialized bytes. Paging needs a separately bounded complete cache; **resolve must not consult a legacy size-error view**.

[Host routes](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/companion/modbridge_host.py:549) provide cached GET and protected POST refresh `{}`; GET queries are rejected globally. [respond():443](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/companion/modbridge_host.py:443) currently reserializes with default spaces, unlike the inventory's compact byte check. Page serialization must measure and send the **same final bytes once**. The native [body array](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/native-host/http.h:9) has 8192 bytes; reserve one terminator and cap JSON at **8191 wire bytes**.

[app.js:101–118](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/ps5-mod-bridge/web/app.js:101) currently commits one response using a title request epoch. Paging must replace that single-response commit, retaining cached startup revalidation, explicit refresh, independent Nexus selection and stale-job recovery.

## Exact transport and caps

Advertise `capabilities.title_inventory_paging=true` only when these handlers exist. This means transport availability, not an observed native provider. Otherwise use existing v1 behavior. Native preview remains paging-disabled until implemented.

| Request | Meaning |
| --- | --- |
| `GET /api/detected-titles/pages` | Page zero of the current cached generation; initial lazy capture once if none exists, matching current startup semantics. No refresh. |
| `GET /api/detected-titles/pages/<cursor>` | Continue only that cached generation. Cursor is an opaque 32-hex server token, not a URL, offset, path or provider argument. |
| `GET /api/detected-titles/pages/current/<snapshot_id>` | Small read-only confirmation that this generation is still current; never captures. |
| `POST /api/detected-titles/pages/refresh` with `{}` | Existing session/exact-Origin/CSRF protections; invalidate old generation/cursors first, capture once, return page zero or a terminal failure. |

Reject query strings, duplicate/unknown parameters, encoded path alternatives and noncanonical tokens. Request target ≤96 bytes; refresh JSON body ≤16 bytes and an exact empty object after strict parsing. Existing session/authority checks and `Cache-Control:no-store` apply. Keep the old v1 GET/refresh routes: a full capture that cannot fit their complete view remains a non-selectable v1 size error, never a truncated subset. Its error view does not replace or erase the separately validated paging cache.

The current-generation confirmation returns exactly `{schema_version:2,kind:"title_inventory_current",snapshot_id:Hex32,current:true}` with HTTP 200, or the constant stale error below. It carries no rows/provenance and cannot mint a generation. New error bodies are exactly `{code:ConstantCode,error:ConstantMessage}` and obey the same wire cap; no supplied token is echoed.

Use this **page type**, not a selectable snapshot type:

```text
TitlePageV2 = {
  schema_version: 2,
  kind: "title_inventory_page",
  snapshot: {
    status: "observed" | "partial" | "unavailable" | "error",
    complete: boolean,
    snapshot_id: Hex32 | null,
    source_scope: SourceTag,
    reason: ConstantReason | null,
    issues: Issue[]
  },
  page: null | {
    index: integer,
    page_count: integer,
    item_offset: integer,
    total_count: integer,
    next_cursor: Hex32 | null
  },
  items: TitleItem[]
}
```

`snapshot.complete` describes the validated provider capture; **it never makes an individual page selectable**. Version 2 prevents the current v1 normalizer from accidentally accepting a prefix. Item metadata fields are unchanged; in v2 only, encode fingerprint-provenance `byte_length` as a canonical positive signed-63-bit decimal **string**, preserving browser precision. The trusted internal provider/cache retains its integer; no browser provenance is submitted back.

Successful pages have observed/complete capture, non-null generation and `reason:null`. Metadata header, page count and total count remain identical; `issues` is bounded and appears only on page zero, with `[]` thereafter. Nonobserved captures return one terminal response with null ID/page, empty items and bounded constant reason/issues, with no selectable prefix. A complete empty capture is one observed page: index/offset/total zero, page_count one, items empty, next_cursor null.

Policy constants: **128 records, 32 issues, 512 KiB complete canonical capture, 8 items/page, 8191 bytes/page, at most 128 pages**. Keep existing field-byte/provenance checks. Pack by actual final serialized bytes, not estimated row count; no row splitting or silent truncation. A row/envelope that cannot fit fails the entire capture as `inventory_resource_limit` before publishing a generation. Precompute boundaries/cursors once. Each nonempty successful page advances item offset; only the last has null next cursor. Page partition/cursor identity remains unchanged on repeated reads.

Per UI load reserve/count every attempted request, including failures, against **129 requests**, **1,056,639 aggregate response bytes** (`129 * 8191`), and a **15-second aggregate transfer deadline**; individual fetch timeout ≤5 seconds. No automatic page retry/restart loop. The complete staged logical JSON remains ≤512 KiB, all per-page arrays ≤8 records and all capture issues ≤32. These are protocol/data bounds, not a claim about total JavaScript/Python heap or a hard interrupt of a provider callback.

Canonical page bytes are compact sorted-key ASCII JSON (`ensure_ascii=True`, no NaN). Client reads a bounded body with cancellation before parsing; rejects noncanonical/invalid UTF-8, then validates the exact schema and bounded tree (depth ≤8, values+keys ≤2048/page). A schema-validated canonical reserialization equal to the original bounded body rejects duplicate object keys as well as hidden/alternate encodings without writing a new general JSON parser. Reencode non-ASCII UTF-16 code units as lowercase `\u` escapes; numeric fields are safe small integers, while exact wide byte length is a string. Test serializer agreement between Python/native/client before enabling the capability.

## Cache, refresh and commit rules

Separate private `ValidatedCapture` from legacy JSON projection. Store only one current immutable generation, owning validated metadata/provenance plus ≤128 page descriptors/cursors; borrowed provider objects cannot mutate it. Validate the whole capture and every page size before issuing IDs. Page requests read this cache only, never files/accounts/providers. `resolve(ref)` uses its full private record map and original provenance checks; no v1 serializer or page prefix participates.

Charge the complete-capture byte bound while validating/copying records and issues, before accepting the next field or allocating its retained copy; do not first clone an arbitrarily large provider object. The trusted provider's own observations/callback must have separately bounded work/storage. This protocol does not retroactively cap allocations already made inside that provider.

Refresh revokes old IDs/cursors atomically, even if capture fails. Permit one capture in flight; concurrent refresh returns constant `inventory_refresh_busy`, and reads while capturing return `inventory_capture_in_progress`, not old rows or another capture. Capture occurs within its separately established provider budget; only its still-current capture ticket may publish. Retired cache buffers cannot be borrowed by an in-flight response; return an owned ≤8191-byte response. GETs do not extend freshness or silently refresh. Restart drops the cache; durable job receipts cannot restore a selectable generation.

Unknown/retired cursor or generation: `409 inventory_page_stale`; concurrent refresh: `409 inventory_refresh_busy`; cached read during capture: `503 inventory_capture_in_progress`; malformed request: `400 invalid_input`; existing expired session: `401 session_required`. Provider terminal reasons normalize to a fixed allowlist (`platform_provider_unavailable`, `provider_error`, `invalid_provider_capture`, `inventory_resource_limit`, `source_changed`, `provider_observation_incomplete`), with no raw paths/errors. Version 2 issue codes use that same closed six-constant allowlist. Other syntax-valid private/v1 issue codes and nonobserved terminal reasons project to `provider_observation_incomplete`; the legacy view preserves their bounded safe text. Malformed codes/reasons still invalidate capture. An observed capture with a nonnull reason remains invalid. New error messages are constants. Cursors are pagination handles bound to generation/index, not authentication; existing authenticated local-session checks remain required.

UI load creates a fresh request epoch/AbortController and disables title selection and target-bearing submission while staging. Keep any prior list read-only; restore no saved target until commit. Require one version/profile/generation, sequential index, exact advancing offsets, consistent counts/scope, unique title keys across all pages, no repeated cursors, and exactly total_count validated items at terminal null cursor. Reject nonterminal empty pages, early/late termination, duplicate/conflicting rows, unknown fields, over-limit body/tree/issues or mixed generations; discard all staging and clear invalid target/pending intent.

After all pages, confirm current generation, then atomically replace the UI list and revalidate the saved opaque ref against it. For a failed/stale page or final check, publish no prefix, retain no selectable old target, and show explicit Refresh targets; do not recapture automatically. Existing stale-job recovery may perform its one protected refresh, then this same bounded collection. A refresh in another session can race even after confirmation: host job/ref validation before enqueue/work remains authoritative and returns existing `stale_target_ref`; no browser/server atomicity claim is made.

## Minimal implementation handoff and fixtures

Next authorized change would touch `modbridge_inventory.py` (separate cache/accessors/legacy projection), `modbridge_host.py` (four fixed routes, capability, exact bytes/constants), and `app.js` (bounded v2 collector/atomic commit). Preserve current records, provenance, job intents and source-scope labels. Native paging/session/refresh is a later matched implementation, not implied by the desktop wire design.

Focused controls: 25+/128 generic records; large Unicode labels/fingerprints/issues; exact/one-over capture and page limits; complete empty/unavailable/partial captures; immutable repeated page reads; refresh during staging and failed refresh; old/cross-generation cursor; restart/saved ref; duplicate title/object keys, repeated cursor, offset/count mismatch, malformed final page, early null cursor, oversize body and cancellation/deadline. Prove no target/key publication before final confirmation and no provider call on continuations; valid job refs/provenance resolve from the full trusted cache despite a legacy v1 size error. No real title inventory, FW13.60 permission or runtime proof follows from these fixtures.
