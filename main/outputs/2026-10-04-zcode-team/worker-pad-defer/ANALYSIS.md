# ANALYSIS — pad-defer phase-6 trace, ownership probe, semantics verdict

Worker: worker-pad-defer · 2026-10-05 · LOCAL ONLY; no console, engine, SDK or
provider execution in this packet. All paths relative to the worktree root
`/WORKSPACE/.codex/worktrees/translation-core/what`.

## 1. Phase-6 trace (the live blocker, pinned)

Live evidence: launch-10 run 3 (pid 355), reproduced run 4 (pid 370) —
`runtime_start_failed code=3`, message `PS5Pad input unavailable: phase=6,
error=-2137915388`. TIMING.md corrects the mission's hex reading:
-2137915388 == 0x80920004 (verified: `-2137915388 & 0xFFFFFFFF == 0x80920004`),
which is `PS5_PAD_ERROR_ALREADY_OPENED` per
`.superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/primary-sources/SDL/src/joystick/ps5/SDL_ps5joystick.h:21`
(`#define PS5_PAD_ERROR_ALREADY_OPENED 0x80920004u`). The mission's original
"0x80A40004" is arithmetically a different value (-2136735740) and wrong.

| hop | where | what |
|---|---|---|
| call | `ue4ss_backend.cpp:26` (composition fold copy `worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/`; bootstrap unit `ue4ss_backend`) | `ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source)` in the backend ctor, BEFORE `program_->init()` |
| guard | `NativePadInput.cpp:13-16` | INI absent → return; input already supplied → conflict-throw |
| parse | `PS5PadConfig.cpp:52+` | full bounded parse (parse errors still throw) |
| factory | `PS5PadNative.cpp:16-22` | builds `PS5PadSource` wiring the real SDK (`sceUserServiceInitialize`, `scePadInit`, `sceUserServiceGetLoginUserIdList`, `scePadOpen`, `scePadReadState`, `scePadClose`) and **eagerly** calls `source->activate()` |
| phase chain | `PS5PadSource.cpp:25-48` | UserInitialize(:29-31, already-initialized 0x80960003 accepted) → PadInitialize(:35-36) → UserList(:37-39) → NoUser(:42) → **Open(:43-44)** → Active(:45) |
| **phase 6** | **`PS5PadSource.cpp:43-44`** | `int handle=m_backend.open(selected,PS5_PAD_PORT_TYPE_STANDARD,0,nullptr); if (handle < 0) return fail(PadPhase::Open,handle);` — `PadPhase::Open` is enumerator index 6 (`PS5PadSource.hpp:10-11`) |
| error propagation | `PS5PadSource.cpp:23` (`fail`), `:24` (`is_available()` == `handle >= 0`) | status carries (phase=Open, error=0x80920004) |
| **throw** | **`NativePadInput.cpp:19-24`** | `if (!source->is_available()) { ... throw std::runtime_error("PS5Pad input unavailable: phase=6, error=-2137915388"); }` — kills the whole runtime start |
| upstream consumer | `RuntimeAckSink` (`runtime_ack_supplier.cpp:96-110` lineage) | records `runtime_start_failed code=3`; ploader fail-closed; game boots anyway |

Run-3 timing (worker-real-timing/TIMING.md §4, single-shot): the open fires
≈+790-800 ms into `module_start` and the refusal is written once; no retry
exists anywhere in the lineage.

**Scope note (user directive):** the ~0.8 s figure is the EARLY-REFUSAL path
only. Full successful-startup timing is unmeasured; nothing here extrapolates
it.

## 2. Ownership probe — who holds the handle (duplicate-init hypothesis first)

Hypothesis (user): OUR port may open the pad earlier (235-ctor block, an
earlier unit, or backend + another input init), so ALREADY_OPENED may be our
own first handle.

**Verdict: disproven for our port.** Evidence:

1. **Single call site.** `scePadOpen` is referenced exactly once in composed
   source: `PS5PadSource.cpp:43` through the `PadBackend` function-pointer
   table wired at `PS5PadNative.cpp:18`. `make_native_ps5_pad_source` is
   called exactly once per boot (`NativePadInput.cpp:17`); no other caller
   exists in the spike trees (grep; the SDL backend's own `scePadOpen` at
   `SDL_ps5joystick.c:355` is a primary-source reference, not linked —
   the provider includes only its declaration header for `PS5_PadData`).
2. **Single attempt per object; idempotent re-activation.** `activate()`
   returns immediately if already available (`PS5PadSource.cpp:27`).
   `Handler::init` cannot trigger a second open on an unavailable source:
   it returns before activation when `!input_source->is_available()`
   (`Handler.cpp:59`, port `first/src/Input/Handler.cpp:53-64`).
3. **Object-level proof in the lineage.** `worker-pad-chord-audit/PAD-DISPATCH-PATH.md`
   §0: composition-5/6 `combined-supplier.o` contains exactly the pad symbols
   `parse_ps5_pad_config`, `make_native_ps5_pad_source`,
   `ConfigureRetainedPS5PadInput`; the pad provider objects are
   `native-pad-PS5PadSource.o` / `native-pad-PS5PadNative.o` (provenance in
   the comp6 base receipt `selected_runtime_units`).
4. **In-run facts.** Run 3 got PAST phases 2-5 (user service, `scePadInit`,
   user list, a logged-in user selected) before failing at phase 6 — the pad
   service layer was already up and a user existed at +790 ms. Our open was
   refused with ALREADY_OPENED — under per-process exclusive open semantics
   that means another component of the SAME process held the pad. Since our
   port made exactly one attempt, the holder is not our duplicate init.
   Most plausible holder: the game's own pad stack (already open before
   injection completed — TIMING.md §4 inference). NOT provable from our
   evidence: which in-process subsystem owns it; we have no successful open
   anywhere in the evidence to compare timing against.
   Also ruled out by evidence shape: the OTHER injected module (path_test)
   is a readiness probe with no pad imports in its lineage artifacts.

Consequence: "reuse our first handle" does not apply (there was no first
handle — our open was refused). The retry window is "until the game's
exclusive handle is free" (disconnect/user switch/menus — possibly never
during normal play). No boot-delay retry is justified.

## 3. Availability semantics — cached, with one honest wrinkle

- **Cached:** `poll()` returns empty when `!is_available()`
  (`PS5PadSource.cpp:69-71`) and NEVER re-attempts activation. After the
  construction-time failure the source stays unavailable forever unless
  someone calls `activate()` again.
- **Retry-safe primitive:** `activate()` IS a clean retry (availability
  short-circuit at :27; failed open leaves `m_initialized` true so a retry
  redoes only pad_initialize + user_list + open — cheap and bounded).
  Host-proven: `recorded_source->activate()` connects once the stub handle
  "frees" (host-1-fix case a).
- **Composition wiring wrinkle:** on this wiring a deferred (unavailable)
  source can NEVER become active later, regardless of provider-side retries:
  - `UE4SSProgram::validate_native_services` (staged l10 program source
    `worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp:740-743`)
    throws `Native supplied input source is unavailable or failed activation`
    when `Handler::init` fails, and `Handler::init` returns false for an
    unavailable source (`Handler.cpp:59`).
  - Even with no source at all, the second gate (`:748-750`) throws
    `Native input service is unavailable; disable bindings and clear
    InputSource` on EVERY input-less boot, because `General.InputSource`
    defaults to non-empty `"Default"` (`SettingsManager.hpp:42`) and is not
    parsed from any ini on this target (worker-ue4ss-settings/
    SETTINGS-CONTENT.md:61-63) — i.e. the clause is a constant here, not a
    request.
  - `Handler::process_event` also skips unavailable sources
    (`Handler.cpp:66-71`), so a poll-side retry would never even run for a
    source the Handler never accepted.

**Verdict:** the original brief's "log-not-throw + accept the source" would
merely move the refusal to `validate_native_services`. The floor
("unavailable controller input must NEVER abort runtime startup") requires
(a) the deferral to NOT supply the source, and (b) the program-unit input
gate downgrade. Both are staged (see DIFF.md). A provider-side lazy-retry in
`poll()` is NOT staged: with this wiring it would be unreachable dead code;
the design (one `activate()` attempt per poll tick, bounded backoff counter,
no timer/thread, owned-close discipline unchanged) is recorded below for the
follow-up unit set that adds Handler-side pending-source support.

## 4. Coexisting input route — no verified path in the known surface

Pinned surface (`SDL_ps5joystick.h:89-98`, the declaration header the
provider includes): `scePadInit`, `scePadOpen(int user_id,int type,int
index,void* param)`, `scePadGetHandle(int,int,int)`, `scePadReadState`,
`scePadSetLightBar`, `scePadSetVibrationMode`, `scePadSetVibration`,
`scePadClose` (+ UserService trio). Findings:

- **No open modes.** `scePadOpen`'s param is an opaque `void*`; no
  non-exclusive/shared flag exists in this surface. The ps5-payload-sdk
  target headers carry no `sce_pad.h` (sce prototypes are per-unit extern
  declarations); no `scePadGetHandleId` or enumeration/peek API exists
  anywhere in the worktree.
- **`scePadGetHandle` is declared but semantically unverified.** It is the
  PS4-era name; on known targets it behaves as the open/acquire call (same
  exclusivity), but nothing in our evidence distinguishes alias-of-open from
  borrow-existing-handle ON THIS TARGET. A single bounded console probe
  (call it after the observed ALREADY_OPENED and log the result) would
  settle it. This fix does NOT use it — no unverifiable behavior claims.
- **Pinned SDL fails closed too:** `SDL_ps5joystick.c:355-358` treats any
  negative open as fatal for that joystick (ALREADY_OPENED is defined but
  not special-cased in this pinned revision).

So the staged fix starts input-less when the handle is held, and the only
later-recovery routes are (1) the unverified `scePadGetHandle` probe, or
(2) a future Handler/program-side "pending source" that retries
`activate()` event-driven until the game frees the handle (design recorded
in §3; not staged here).

## 5. Input-less startup, what actually happens downstream

- `LuaMod::start_mod` catches ALL `std::exception` from `main.lua`
  execution and logs `Exception during mod startup: ...` without rethrowing
  (`LuaMod.cpp:6440-6489` in the composition port). The mod's
  `RegisterKeyBind` calls will hit `Handler::register_keydown_event`'s
  injected-mode refusal (`Handler.cpp:109/123` — `m_source_injected_mode`
  defaults true on `__PROSPERO__`, `Handler.hpp:56-58`), so the F7 menu mod
  is dead but the runtime and mod inventory survive. Code-read only; not
  console-proven.
- `EnableDebugKeyBindings` (default false, unset in the shipped ini) keeps a
  fatal refusal in the staged program gate — an explicit binding request is
  not "unavailable controller input"; its later registration site
  (`UE4SSProgram.cpp:1510-1515`) would throw without a live source anyway.
- What the mod loses on input-less boots: OPTIONS→F7 menu toggle and the
  five nav binds — consistent with launch-10's load-capable status.

## 6. Owned vs borrowed cleanup (preserved and host-asserted)

`PS5PadSource` destructor → `deactivate()` (`PS5PadSource.cpp:49-64`):
- handle never acquired (`handle < 0`) → `scePadClose` never called;
- `sceUserServiceTerminate` only when THIS object's
  `sceUserServiceInitialize` returned 0 (`m_owns_user_service`); the
  already-initialized 0x80960003 path keeps the flag false, so the host's
  service is never torn down (borrowed). The deferral drop in the staged
  NativePadInput.cpp inherits exactly this discipline (comments in source;
  host-1-fix asserts both paths).
