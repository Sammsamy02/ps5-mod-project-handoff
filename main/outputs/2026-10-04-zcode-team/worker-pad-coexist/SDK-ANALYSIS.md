# SDK-ANALYSIS — the scePadOpen class question, decided as far as in-tree evidence allows

Worker: worker-pad-coexist · 2026-10-05 · LOCAL ONLY; read-and-derive; no console,
no engine, no provider execution. Mission question: can ANY scePadOpen class yield a
READABLE pad handle while the game holds its own (the outcome-B first gate)?

## 0. What the pinned SDK actually contains (correcting the brief)

The brief's `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/
target/include/sce_pad.h` DOES NOT EXIST (verified: no sce_pad header anywhere under
the sdk-v0.43 tree; the payload-SDK target headers carry no sce prototypes — this
confirms worker-pad-defer ANALYSIS.md §4). The pinned SDK's pad evidence is:

1. **The libScePad.so link stub** (`.../sdk-v0.43/ps5-payload-sdk/target/lib/libScePad.so`,
   ELF x86-64, not stripped): **130 exported `scePad*` symbols**. Every symbol is a
   strong, size-0 `.text` alias at the SAME address (0x3334; verified via `nm`), i.e.
   the ps5-payload-sdk stub convention: symbols exist to satisfy PRX linking; the real
   functions resolve by NID inside the console's `libScePad.sprx`. The stub therefore
   proves **which entry points exist on SDK v0.43** — and carries **zero semantics**
   (no bodies, no error tables, no docs).
2. **The pinned SDL PS5 backend** (the provider's declaration header):
   `.superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/primary-sources/
   SDL/src/joystick/ps5/SDL_ps5joystick.{h,c}` — 8 pad externs + error/button defines.
3. **The Rufidj ps5_sdk headers** (same primary-sources tree): `ps5_sdk_pad.h` is
   helper statics over raw dynacalls (button bit maps, packet decode); **no ownership
   or scheduling documentation**.
4. **Townfall's OWN saved import declarations**
   (`.superpowers/native-dependency-spike/ue4ss-public-import-declarations-1/
   declared-input.json`): exactly `scePadClose, scePadInit, scePadOpen, scePadReadState`
   (+ UserService trio). These are the identities a ploader plugin can import with
   Townfall-proven provenance.

The 130-symbol stub surface, open-class-relevant members:

| entry point | class | in-tree declaration? | in-tree semantics? |
|---|---|---|---|
| `scePadOpen(user,type,index,param)` | standard | YES (SDL_ps5joystick.h:105) | exclusive-open failure class documented (ALREADY_OPENED, :10); SDL opens (user, STANDARD, 0, NULL) and treats any negative rc as fatal (SDL_ps5joystick.c:355-358) |
| `scePadOpenExt` / `scePadOpenExt2` | extended | NO | NO — param/extension semantics unknown in-tree |
| `scePadDeviceOpen` | device-class | NO | NO |
| `scePadGetHandle(user,type,index)` | PS4-era acquire | YES (SDL_ps5joystick.h:106) | NO — alias-of-open vs borrow-existing-handle undecidable (pad-defer ANALYSIS §4) |
| `scePadIsValidHandle` | validation | NO | NO (needs a handle anyway) |
| `scePadGetInfo` / `scePadGetInfoByPortType` | enumeration | NO | NO |
| `scePadRead`, `scePadReadHistory`, `scePadReadStateExt`, `scePadGetDataInternal` | reads | NO (ReadState YES) | NO |
| `scePadVirtualDevice*` (+ Sony's own typo `scePadVertualDeviceAddDevice`) | virtual pad | NO | NO |

## 1. (a) Can a SECOND open with a different userId/port-type succeed?

**UNDECIDABLE from in-tree evidence — and here is exactly where it stops:**

- No in-tree signature carries any mode/flag: `scePadOpen`'s 4th argument is an opaque
  `void* param` (SDL opens with NULL; the provider passes nullptr). There is no
  non-exclusive/shared bit anywhere in the surface.
- `PS5_PAD_ERROR_ALREADY_OPENED` (0x80920004) EXISTS as a distinct error code — that is
  in-tree documentation that at least one open class is EXCLUSIVE. What the evidence
  cannot say is the SCOPE of the exclusivity: per (user,type,index) tuple, per port-type
  class, or process-wide. The live refusals (launch-10 run3/run4, pids 355/370) cover
  exactly ONE tuple (first logged-in user, STANDARD, 0) — no second tuple was ever
  tried in-game, and no successful open exists anywhere in the captured evidence to
  compare against.
- The stub proves `scePadOpenExt/OpenExt2/DeviceOpen/GetHandle` EXIST on this SDK, but
  they have no in-tree declaration, no doc, and are NOT in Townfall's own import
  declarations — attempting them from a plugin requires NEW import identity rows, each
  an unproven loader experiment on its own.

## 2. (b) Would scePadReadState mirror the shared controller from a second handle?

ReadState is a **cached snapshot**, not a queue consumer (menu-input-map REPORT §2c;
the provider's duplicate-timestamp guard is built on exactly that, PS5PadSource.cpp:81
lineage). Snapshot semantics make button mirroring from a second handle PLAUSIBLE
(each handle's ReadState reflects current hardware state), while `scePadRead` /
`scePadReadHistory` consume per-handle event queues and would be the wrong instrument.
But this is strictly CONDITIONAL on (a): no second handle has ever existed in our
evidence, so mirroring is unverified. The probe variant below answers it in one step
(ReadState from the variant handle while the game holds its own).

## 3. (c) What does enumeration return in-game?

- `scePadGetHandleId` **does not exist** — not in the 130-symbol stub surface, not in
  any in-tree header. Closest existing names are `scePadGetHandle`,
  `scePadIsValidHandle`, `scePadGetInfo`, `scePadGetInfoByPortType` — all
  non-importable without new identity rows and undocumented in-tree.
- The user-list class is NOT the blocker: `sceUserServiceGetLoginUserIdList` is
  Townfall-proven and the provider's selection (first logged-in user,
  PS5PadSource.cpp:37-42 lineage) demonstrably SUCCEEDED in-game — launch-10 run3/run4
  got PAST phases 2-5 (user service up, pad initialized, user list populated, a user
  selected) before failing at phase 6 (the open). The source's user-initialize also
  already accepts the already-initialized class 0x80960003.

## 4. (d) Any peek / non-exclusive read API?

No. The export surface contains no non-exclusive mode. The `scePadVirtualDevice*`
family is the EMULATION direction (a module INSERTS input into a virtual pad for other
titles to read — the inverse of our problem); `scePadGetDataInternal` /
`ReadStateExt` / `ReadHistory` all require an existing handle. No peek API produces a
readable handle without an open.

## 5. VERDICT (drives the staged fix)

**No open class is decidable from in-tree evidence to yield a readable handle alongside
the game's.** Therefore:
- The staged coexist fix (this packet) keeps the retry on the ONLY proven class —
  `scePadOpen(first_logged_in_user, PS5_PAD_PORT_TYPE_STANDARD, 0, nullptr)`, the exact
  call observed to fail — repeated EVENT-DRIVEN from inside `poll()` on the sole
  processing thread with bounded backoff. It needs no undecidable behavior claim, and
  it catches the realistic recovery windows (disconnect/user-switch/system-menu) the
  moment they occur.
- A second-handle route is NOT implemented as primary (it would be an unverifiable
  bet); the question is handed to the probe below.

## 6. PROBE DECISION: PROBE NEEDED (headers cannot decide the argument matrix)

The header-decidable part is exhausted (§0-§4). What remains undecidable is whether ANY
argument tuple of the PROVEN `scePadOpen` (or the unproven new-identity entry points)
succeeds while the game's handle is held. The probe instrument exists:
`outputs/2026-10-04-zcode-team/worker-pad-hardware-probe/` (modbridge_pad_probe.prx
lineage; host-tested runner in `src/pad_probe_core.c`, bounded window/grace/budget,
JSONL sink `/data/pad-probe-events.jsonl`, prepared-module ordering probe_start →
activate → register → poll loop → probe_end/teardown).

### 6.1 Variant "open-class matrix" — ZERO new import rows (preferred)

Reuses exactly the 14 pinned import identities (pad_abi.h): every variant is only a
DIFFERENT ARGUMENT TUPLE to the already-proven `scePadOpen`. Spec (one bounded sweep,
once, during a launch-window companion run — i.e. AFTER the game is up and holding the
pad, same session as the composed runtime):

| id | user | type | index | param | expect if per-tuple exclusivity | expect if per-class |
|---|---|---|---|---|---|---|
| V0 | selected user (as today) | 0 STANDARD | 0 | null | 0x80920004 (baseline, matches run3/4) | 0x80920004 |
| V1 | EACH other userId returned by sceUserServiceGetLoginUserIdList | 0 | 0 | null | ? (per-user scope ⇒ may open) | 0x80920004 |
| V2 | 0xFF (PS5_USER_ID_SYSTEM, SDL_ps5joystick.h:8) | 0 | 0 | null | ? | 0x80920004 |
| V3 | -1 | 0 | 0 | null | ? | 0x80920004 |
| V4 | selected user | 16 REMOTE_CONTROL | 0 | null | ? (per-type scope ⇒ may open) | ? |
| V5 | selected user | 0 | 1 | null | ? (per-index scope ⇒ may open) | ? |

Per variant, record exactly: `open_rc` (raw), on rc>=0 immediately one
`scePadReadState` rc + `buttons` + `timestamp` (the MIRROR answer: do the buttons
track the physically pressed controller while the game holds its handle?) + one
`scePadClose` rc; on rc<0 the raw negative error only. Then `scePadClose` NOTHING that
was not opened. All records land in the existing JSONL sink with a
`variant=V0..V5` field; the probe_end record gains `open_class_matrix={V0:rc,...}`
(one-line summary, MIRROR-able).

Decision table after the sweep:
- ALL variants negative ⇒ standard-class exclusivity is process-wide for every
  in-tree-decidable argument; outcome-B depends on the game freeing the handle (the
  staged retry is the only route; the probe conclusion also downgrades the value of
  new-identity experiments).
- ANY variant opens AND ReadState mirrors buttons ⇒ THAT tuple is the second-handle
  route: the staged source gains a one-line open-class switch (the retry already
  re-runs `open(selected,...)`; switching the tuple is a bounded, host-testable
  change), still event-driven, still same-thread.
- ANY variant opens but ReadState does NOT mirror (empty/stale) ⇒ handle without
  usable data; record and stop — outcome-B stays retry-only.

### 6.2 New-identity variants (deferred, separate experiment)

`scePadGetHandle`, `scePadOpenExt`, `scePadOpenExt2`, `scePadDeviceOpen` exist on the
SDK (stub §0) but are absent from Townfall's declared imports and the frozen contract:
each requires a NEW import-identity row (loader-acceptance experiment per row) plus an
in-module declaration. Worth running ONLY if §6.1 comes back all-negative AND a
second-handle route is still wanted; do NOT bundle these rows into the §6.1 launch
(one experiment per launch, per the team's isolation discipline).

### 6.3 Why the probe and not the composed runtime

The composed runtime must never bet its start (or its input wiring) on undecidable
semantics; the probe runs OUTSIDE it with bounded budget and its own sink, and its
records are directly comparable with the runtime's own sdiag refusal tags.
