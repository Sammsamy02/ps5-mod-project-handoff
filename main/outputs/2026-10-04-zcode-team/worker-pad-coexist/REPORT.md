# REPORT — worker-pad-coexist (2026-10-05)

**VERDICT: outcome B (usable controller input alongside the game) is IMPLEMENTED at
source level and HOST-PROVEN end-to-end — deferred-attached source, event-driven
bounded retry inside poll() on the UpdateThread, binds registering while deferred,
OPTIONS edge dispatching to the menu callback — but it remains CONDITIONALLY usable
on console: the SDK analysis found NO header-decidable second-handle route, so the
retry succeeds only when the game's exclusive pad handle frees (a real but unproven
window); the launch-window probe variant that would upgrade it to a guaranteed route
is specced, not run.**

Local only; no console, no engine, no provider execution; nothing folded. All work
under `outputs/2026-10-04-zcode-team/worker-pad-coexist/`.

## 1. What was built (on the wpd1 lineage, proposal-only)

Four staged sources + the required-as-is wpd1 program staging (DIFF.md has exact
diffs, hashes and rationale):

1. `staging/native-pad-input-coexist/NativePadInput.cpp` (unit `native-pad-input`) —
   THE wpd1 DELTA: the deferred source is ATTACHED, not dropped (wpd1's drop made
   recovery unreachable). Deferral diagnostics unchanged; all refusal semantics for
   real config errors unchanged.
2. `staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.{hpp,cpp}` (unit
   `native-pad-PS5PadSource`) — the mission-shaped retry: `poll()`'s unavailable
   early-return now runs `maybe_retry()`: one `activate()` per interval, 200 polls
   initial (~1 s at the 5 ms UpdateThread cadence), doubling to a 3200-poll cap
   (~16 s), ≤4 bounded SDK calls per attempt, ZERO new threads/timers, same-thread
   serialized (PlatformInputSource.hpp:11-14 + Handler.hpp:60-64). R4 vocabulary on
   the shared supplier-diag channel: `pad_open_class` (=0, STANDARD scePadOpen,
   once), `pad_retry` (attempt #), `pad_retry_failed` (raw error), `pad_available`
   (handle). Ownership/lease discipline byte-preserved.
3. `staging/ue4ss-native-pad-provider-coexist/port/PS5PadNative.cpp` — byte-identical
   RELOCATION (content == base, gate-proven) so the provider's own two TUs see one
   header; `PadServiceLease.hpp` + vendored SDL header relocated byte-identically.
4. `staging/input-handler-coexist/Handler.cpp` (unit `input-handler`) — exactly five
   deltas (diff-verified): deferred attach in `init`; availability gate moved out of
   `process_event` (the retry's reachability); `register_keydown_event` x2 tolerant
   of attached-but-unavailable sources (the load-rung enabler — main.lua's six binds
   register while deferred, closing wpd1's FOLD §3 "input-less boots" mod death);
   `set_input_source` injected-branch checks identity, not availability.
   `Handler.hpp` byte-unchanged.

Why the retry could not live anywhere else: worker-menu-input-map REPORT §4 (the
definitive outcome-B analysis) — the blocker is availability AT POLL TIME; the retry
MUST be event-driven from inside poll() on the UpdateThread; Handler::init
re-injection cross-thread is forbidden. pad-defer ANALYSIS §3 recorded exactly this
design as "unreachable dead code" on wpd1 wiring and handed it to this packet.

## 2. The SDK open-class analysis (the decisive question)

Full argument in SDK-ANALYSIS.md; one-paragraph answer: the brief's `sce_pad.h` does
not exist in the pinned SDK; the 130-symbol libScePad.so stub proves open-class
ENTRY POINTS (`scePadOpen/OpenExt/OpenExt2/DeviceOpen/GetHandle`, plus the
virtual-device emulation family and `scePadGetHandleId`'s NON-existence) but carries
zero semantics; no in-tree signature has any non-exclusive mode (`param` is opaque
void*); ALREADY_OPENED documents exclusive ownership with UNDECIDABLE scope
(tuple/class/process); `scePadGetHandle` remains alias-or-borrow-undecidable
(pad-defer §4); ReadState's cached-snapshot semantics make second-handle mirroring
plausible but strictly conditional on a second handle existing. **Verdict: no open
class is decidable from in-tree evidence to yield a readable handle alongside the
game's — so the staged retry stays on the ONLY proven class (standard scePadOpen,
the exact call observed to fail), and no unverifiable second-handle bet was
implemented.** The user-list class is exonerated by the live evidence (run3/4 passed
phases 2-5; the refusal is squarely the open).

## 3. Host proof (all four mission cases; 54 checks, lineage ASan+UBSan, exit 0)

`tests/host_pad_coexist.cpp` drives the REAL staged NativePadInput + Handler +
PS5PadSource through the REAL `ConfigureRetainedPS5PadInput` with an owned backend
stub (the lineage host-1 recipe; only the sources under change swapped; recipe
deviations documented). Receipt: `host-prove/receipt.json`.

- (a) unavailable-then-available (THE RETRY PATH): ALREADY_OPENED at configure →
  deferral diagnostics → deferred ATTACH → `Handler::init` true → F7 registers at
  load → tick 200's poll retries, opens → `pad_open_class`/`pad_retry`/
  `pad_available` exact → availability observed → baseline swallowed → rising
  OPTIONS edge dispatches to the registered callback; edge-only semantics (no
  autorepeat, duplicate-timestamp and held-edge guards) verified; re-press fires.
- (b) second-handle class CONTROL: every open — construction and every retry —
  uses ONLY (user, PS5_PAD_PORT_TYPE_STANDARD, 0, nullptr); `pad_open_class`=0
  emitted exactly once per session. (The SDK route itself: NOT implementable from
  headers — §2; the probe answers it.)
- (c) permanent unavailability: 1000 ticks with the handle never freeing → exactly
  2 retry opens (backoff schedule: attempts at polls 200 and 600, next would be
  1400) — no busy loop; startup unaffected; source stays attached; borrowed user
  service never terminated; no close of a never-acquired handle. Outcome A
  preserved in the worst case.
- (d) lifecycle contract: with a cross-thread driver, every construction-phase call
  is on the configuring thread and EVERY retry/read call is on the driver thread —
  the sole processing thread; a second `Handler::init` performs zero backend calls
  (no re-injection path); shutdown closes our handle once, on the shutdown caller's
  thread, after the driver joins.

Honest fixture note (DIFF.md §4): the first sanitized run caught a REAL staging
hazard — the host test allocated the source through the BASE header while the
staged TU constructed it with appended members (ASan heap-buffer-overflow in the
ctor). Fixed by giving the allocating TU the staged header; the append-only member
layout (byte-identical offsets for every pre-existing member) keeps all other
mixed-header TUs consistent, and the byte-identical PS5PadNative relocation keeps
the provider's own TUs ODR-clean (gate-proved on the real target compiler).

## 4. Compile gates

Four gates, EXACT pinned composition argv from the launch-10 base receipt, only
source/-o/-MF redirected (asserted): `compile-native-pad-input`,
`compile-native-pad-PS5PadSource`, `compile-native-pad-PS5PadNative` (proves the
byte-identical relocation compiles on `prospero-clang++` against the staged
header), `compile-input-handler`. All exit 0. Receipts alongside each gate.

## 5. Probe decision: PROBE NEEDED (one bounded launch-window companion)

Headers decide everything EXCEPT the open-class argument matrix. The spec
(SDK-ANALYSIS.md §6): variants V0-V5 on the EXISTING `worker-pad-hardware-probe`
instrument (modbridge_pad_probe.prx lineage, dir verified present with its
host-tested runner, JSONL sink and prepared-module ordering) — the baseline tuple,
each other enumerated user, system user 0xFF, user -1, REMOTE_CONTROL port type 16,
index 1 — ZERO new import rows (every variant is only different arguments to the
already-proven `scePadOpen`), each opening variant immediately ReadState'd (the
MIRROR answer) and closed. A decision table maps outcomes to actions: all-negative ⇒
the staged retry is the only route and new-identity experiments lose value; any
open+mirroring tuple ⇒ a one-line, separately-reviewed open-class switch in the
already-retrying source. New-identity rows (`scePadGetHandle`/`OpenExt`/
`OpenExt2`/`DeviceOpen`) are deferred to a separate per-row loader experiment and
are NOT bundled into the §6.1 launch.

## 6. Honest bounds

- Nothing ran on console; "outcome B achieved" means the composition can no longer
  be the reason it fails — the handle may NEVER free, in which case case (c) is the
  permanent live state (safe, input-less, load-capable).
- R3 (EngineTick drain liveness, gap G-AP2) and R4's per-event tags (G-AP1) remain
  the menu's other known gaps; this packet's retry/availability tags fill only
  G-AP4 (pad retry accounting) and the availability half of G-AP3 (phase
  transitions surface as `pad_retry*`/`pad_available`).
- The wpd1 program staging is REQUIRED by this fold (FOLD.md §2) but is pad-defer's
  artifact; no program delta was added here.
- Thread identity, backoff schedule, borrowed-service and close discipline are
  HOST-proven against stubs; they bound, but do not replace, a real boot.

## 7. Files

- `CLAIM.json` — C1-C11 with grades and evidence paths.
- `SDK-ANALYSIS.md` — the open-class verdict + the probe spec (§6).
- `DIFF.md` + `diffs/*.wpc.diff` — exact staged deltas vs byte-pinned bases.
- `staging/` — the four staged sources (+ byte-identical relocations).
- `tests/host_pad_coexist.cpp`, `host-1-coexist/`, `host-prove/receipt.json` — the
  54-check proof.
- `compile-*/` — the four pinned-argv gates; `build_and_prove.py` — reproducible
  driver (read-only against all lineage files).
- `FOLD.md` — admission instructions for the chief (all four units need NEW
  admission; wpd1 program required as-is; post-link symbol checks).
