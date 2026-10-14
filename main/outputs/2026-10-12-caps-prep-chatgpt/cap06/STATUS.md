# Capability 6 — controller receipt audit

Mac-only replay of historical raw console receipts. Fresh audit does not upgrade broader controller mapping or expected menu action to console-proven.

Implemented audit_pad_receipts.py: reads the existing supplier-diag/v1 JSONL directly; requires five consecutive, same-segment/same-PID events with consecutive sequence numbers, nondecreasing times and exact values: L3+R3 edge 118 → allowed 1 → subscribed 1 → modifiers 0 → F7 enqueued 118. It does not create normalized events, infer behavior from marker string counts, or claim the provider enqueue is a consumer callback.

Preserved replay: **36 provider-reported edge/enqueue chains**, first at physical JSONL lines 35443–35447, pid126 seq1114–1118. Source emission locations: PS5PadSource.cpp lines159–162/170. Local source/build pairing remains unproved; capture concatenates sessions, and the adapter's inferred segment numbers are not independent run IDs.

Expected-action source: retained spawner Lua lines694–696 binds F7 to scheduling toggleMenu; lines670–672 call showMenu(not menuOpen). This is static expectation from a different dated snapshot, not an observed action associated with those provider sequences. Result remains **INCONCLUSIVE**. No menu-action acknowledgment or correlation ID exists in the inspected JSONL adapter.

Generated-map contract: GENERATED-BASELINE.ini remains a 2,422-byte staged candidate, SHA256 1f07a85a3b60393789a87112db7e683837e5e98db34f1a790d00679dcb5c59c3; line22 carries L3+R3=F7. MAPPING.md distinguishes rising-edge provider delivery, non-exclusive notifications, reserved controls, subset/superset chord refusal, overflow warnings and size-exact loadlist admission. A generated file is not evidence it was deployed in this captured session. Generator was read only, not run.

Fresh tests: 2/2, including removal of the three association/guard records from a real preserved edge/dispatch pair; the cut pair correctly yields zero attributed provider chains and INCONCLUSIVE. Replay JSON contains exact hashes and line references without raw logs.

Next proof: capture deployed map/build identity and per-edge callback/action acknowledgment in the same run, plus hold/release samples for no-repeat claims; separately test each additional generated mapping and game-control coexistence. No runtime code, patch, compiler/build, network, console action or shared document changes included.
