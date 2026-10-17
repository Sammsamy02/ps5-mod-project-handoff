# Native prerequisite plans to automatic download queue

Continue the full active goal from 19e0075. The previous turn is verified progress:
bounded plans pass 55 actual owned HTTPS controls, and current host/SDK builds
pass. The next gap is atomic acquisition admission and unattended queue progress.
The user remains unavailable. Keep all work local, preserve paused inputs, and
do not contact the console/production account or retry stopped actions.

## Contract

Add `mb_nexus_jobs_submit_acquisition_v1` using the unchanged existing jobs submit
params/result layouts. Its 4096-byte intent has exactly the four acquisition-plan
fields plus `idempotency_key` (existing 8..80 ASCII key rules). Strictly validate
duplicate keys, nesting, types, bounded IDs and canonicalize roots/choices before
replay. Expose exact POST `/api/acquisitions`; GET is405 and disabled jobs503.
Keep existing cookie/CSRF/Origin framing and body limit.

Within one jobs Busy guard, validate existing session/account/storage and look
for a same-cookie group replay. A canonical identical intent/key returns the
original ordered child IDs without provider discovery or entropy, even at full
capacity. Changed intent conflicts. Groups have their own namespace. Never
accept a serialized client plan, source URL, credentials or entitlement.

For a new group, call the actual auth-owned acquisition planner on the separate
auth context. Consume only final OK/complete ready output: complete same-game
closure, no unresolved facts, 1..8 unique selected mod/file identities and false
download/installation authority. No public jobs recursion or new auth owner.
Non-ready plans return additive ACQUISITION_UNRESOLVED=13 with no queued prefix.
Provider/transport failure publishes no acquisition.
Use the configured resolver budget for the whole new admission: checked
monotonic time starts before discovery, only the remaining budget reaches the
auth planner, and final precommit time is checked after private scratch cleanup.
The unchanged submit layout provides no caller cancellation API or hard syscall
interrupt promise. Reuse existing clock/budget code where practical.

Require remaining global source capacity for every selected source. Stage all
download_analyze records and one compact fixed-bound group receipt privately;
use heap staging for the large records, not an eight-record stack array. Admit
checked entropy for every fresh ID, rejecting existing and staged collisions.
Internal child keys remain empty, outside the public single-job key namespace.
Serialize the compact receipt and perform final session/account/storage checks
before a single non-failing POD commit. Refusal wipes output/staged bindings and
leaves records/count/receipt/scratch unchanged. Receipts are process-lifetime,
bounded by the existing eight global source slots, with no persistent journal.

Return `{acquisition:{state:"accepted",game,idempotency_key,source_job_ids,
selected_sources,compatibility:"unresolved",version_satisfaction:"unresolved",
download_authorized:false,runtime_supported:false,installation_supported:false,
acquisition_order_is_game_load_order:false}}`. selected_sources contains exact
game/mod_id/file_id/job_id mappings in prerequisite-first acquisition order.
Accepted describes admission; current child states come from existing jobs
routes. Workers independently recheck current live authorization/visibility and
use the unchanged credential-free sink and original archive handlers.

Make the serial host advance one queued job after an idle listener wait as well
as after an HTTP response. Preserve the one curl/PSA owner and existing lifetime
checks. Between jobs, listener readiness takes priority. No background thread,
concurrent transfer/polling promise or implicit generation selection. This makes
an admitted group progress without repeated HTTP requests. Existing explicit
IoStore generation remains all-source, profile-bound and non-installing.

## Work

- [x] Add atomic jobs admission/replay and focused capacity, staged entropy,
  output/final admission, ownership/untrusted input and re-entry controls.
- [x] Add exact HTTP route and bounded serial idle advancement, with historical
  actual application RED before GREEN.
- [x] Exercise actual owned HTTPS two-source premium acquisition of unchanged
  Leon and Movement archives; prove idle progress, replay and independent
  resolver authorization. Use exact receipt IDs for one existing generation
  and retained full readback/oracle. Provider dependency metadata is synthetic,
  not a claim that the real mods depend on one another.
- [x] Exercise a fresh free-account/no-grant group: both children require Nexus
  authorization, no link/archive calls or partial generation. Reuse existing
  harnesses; do not repeat graph/codec matrices absent a concrete regression.
- [x] Compile current full host/SDK closure, save results and preserve paused
  baseline. Full native free-account grant/resume, persistent installation,
  target discovery, UE4SS/DLL game execution and final PS5 UI remain unfinished.
