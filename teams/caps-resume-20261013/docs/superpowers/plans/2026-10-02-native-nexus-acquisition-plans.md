# Native Nexus prerequisite acquisition plans

Continue the full active goal from a1c9543. Prerequisite discovery is verified;
the next concrete gap is selecting actual source files and walking dependencies
without inventing compatibility or installation authority. The user authorized
autonomous local overnight work and focused experiments. Preserve paused work,
avoid console/production-account contact and game/system writes, and do not
retry the previously stopped placement, memory-read or curl-upgrade actions.

## Contract

POST `/api/acquisition-plan` accepts exactly:
`{game, mod_ids, file_choices, file_policy}`. Game is a safe domain; mod_ids is
1..8 unique positive INT32 root IDs. file_choices is 0..8 unique
`{mod_id,file_id}` mappings, with positive INT32 mod IDs and signed63 file IDs.
file_policy is `explicit_only` or `unique_current_primary`. Unknown/duplicate
fields and wrong types are invalid. The existing 4096-byte JSON body frame and
cookie/CSRF/Origin gates apply. No trust, URL or credentials are caller inputs.

Use one additive auth C ABI with borrowed cookie/CSRF/Origin, intent byte span,
one timeout/cancel budget and independent owning output up to1 MiB. Reuse one
Busy/Operation and the private catalog/requirements helpers. The source mod must
be visible before reading files/requirements; apply existing fresh adult checks.
Do not recursively call the public catalog ABI or introduce another owner.

Preserve strictly typed nullable file hints: is_primary, category_id,
category_name. Missing/wrong-type hints stay unknown. Explicit choices require
exact membership in the unique-ID live file set. Automatic policy admits only
one current MAIN candidate with is_primary=true, category_id=1 and
category_name=MAIN; unknown/contradictory hints or ambiguous candidates remain
unresolved. Never choose by ID, version ordering, filename or free-form notes.

Walk a deterministic same-game closure of at most8 internal mod identities,
including roots; deduplicate fetches while preserving relation provenance.
Bound total relations and unresolved facts at128 each. Other-game requirements,
external/DLC requirements, INT32-unqueryable children, cycles, hidden nodes,
missing/ambiguous choices and closure limits remain typed unresolved reasons.
Unused explicit mappings also remain unresolved. Invalid provider responses,
transport failure, cancellation or deadline failure publish no plan.

Return `{plan:...}` with scope `nexus_acquisition_metadata`, typed root/node/
relation facts, file policy and deterministic prerequisite-first selected
sources. `acquisition_ready` is true only for a fully discovered internal
closure with all source files chosen and no unresolved facts. Otherwise publish
no selected_sources ready prefix. Always report compatibility/version
satisfaction unresolved, file_version_constraints_provided=false,
download_authorized=false, installation_supported=false and
acquisition_order_is_game_load_order=false. The normal resolver must separately
authorize downloads. Relation IDs/typed identities are preserved; notes/URLs
are not used as decisions or duplicated into this bounded plan.

## Work

- [x] Extend file metadata and add a pure bounded deterministic selector with
  focused current/archived/unknown/duplicate/explicit-choice controls.
- [x] Add auth-owned recursive planning, additive layout admission, source
  visibility, one deadline and final account/session/publication gates.
- [x] Add the exact native HTTP route and actual owned HTTPS graph fixture:
  diamond dedup, dependency order, other domains, cycles, malformed/hidden/limit
  cases, auth/framing and timeout boundaries.
- [x] Compile current host/SDK artifacts, run the focused current requirements
  regression, inspect current native closure and preserve paused inputs.
- [x] Save the concrete phase and full-goal handoff. Subsequent plan-to-job
  download coordination, persistent installation, UE4SS/DLL engine backends and
  the final PS5 store remain full-goal work.
