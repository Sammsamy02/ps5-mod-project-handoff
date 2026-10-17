# Runtime stop candidate — local control-flow fixes

Status: **Mac-only candidate, not integrated or uploaded.**

Three actual selected sources are copied into this directory. Their originals are preserved and pinned in `SOURCE-DELTA.json`.

- The C module entry asks the in-link residence provider unconditionally, including a partial-start/no-runtime-owner case. Unknown or retained code refuses with a negative result. A runtime stop failure preserves `runtime_active` and returns before lifecycle finalization, input release, or diagnostic-channel closure.
- The acknowledgment helper destroys its owner only after a successful `stop()`. A refused stop retains the owner.
- The runtime bridge checks residence before its no-owner shortcut. A stop exception preserves the owner and startup resources rather than destroying them while reporting failure.

The residence provider must atomically close future hook publication under the same lock used for registration. Zero active callbacks is insufficient: once any thunk has been published, the candidate provider must report required code residence even after retirement. This export is defined inside the composition; it is not a new system-library import. Until its actual provider, target compilation, and full link are verified, these three files remain candidates.

Tests extract the actual source bodies and use explicit owner, resource, residence and OS seams. Fatal ASan/UBSan runs establish control flow only:

- Entry: original two refusal controls fail; initial candidate five controls pass; integrated residence candidate eight controls pass, including no-owner and unknown-residence refusal.
- Acknowledgment helper: original false-stop control fails; candidate success/refusal/exception controls pass.
- Bridge plus helper: original five refusal/exception/residence controls fail; candidate seven controls pass.

These tests do not execute the real initializer, prove engine quiescence, or establish a native unload contract. They prevent the identified destructive control flow. Separate Lua-mod uninstall remains unresolved: admitted script callbacks must finish before their raw userdata or Lua state is closed. The independent teardown report identifies that boundary; these changes do not relabel it as solved.

Receipts: `RESULTS.entry-only.json`, `INTEGRATION-CONTROLS.json`, `BRIDGE-CONTROLS.json`, `SOURCE-DELTA.json`. The interrupted independent review is not counted as acceptance. No console contact, uploads, payload loads, game launches, config changes or deletions occurred.
