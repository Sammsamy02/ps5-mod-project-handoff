# INPUT-SERVICE.md — shared input kernel service (kekcall nr=9) for the PS5 UE4SS runtime

worker-kernel-input, 2026-10-06. **Research + design only.** Nothing here ran on
console, no engine contact, no game claims. Every in-tree fact cites its file;
every web fact cites its source in §10. Items marked **[VERIFY]** are inferred or
undocumented and must be confirmed (from a kernel dump for kernel-side items,
from a controlled probe for runtime items) before production use.

Mission context: ZCode priority #6 — shared input where ANY process can read
keyboard and controller events. The kernel safe-read worker
(`worker-kernel-saferead/`) established the kekcall channel (kstuff hooks
`sysent[SYS_getppid=39]` globally; userspace packs the kekcall number in
RAX[63:32]). This document designs the input-sharing service on that channel.

---

## 1. PS5 input architecture (research summary)

### 1.1 The full stack

```
DualSense over USB                       DualSense over Bluetooth
    xHCI controller IRQ                      BT radio (wlanbt)
    /dev/usb, /dev/usbctl  [PS5 Devices]    /dev/bt  [PS5 Devices]
          |                                        |
          hid  (kernel USB HID driver, /dev/hid)   bluetooth_hid (/dev/bluetooth_hid)
          |            \                           /
          |             `---- normalized pad events ----`
          v
   ctrlp / ctrlp_sync  (kernel controller-port driver; /dev/ctrlp*)
   [PS5 Devices: "Used by libScePad (controller)"]
          |  ioctl per read (batch), handle/open lifecycle
          v
   libScePad.sprx  (userland, inside EACH process; handle table,
                    process-wide read mutex, latest-state cache)
          |
   scePadOpen / scePadOpenExt* / scePadGetHandle / scePadRead / scePadReadState
```

Same shape for keyboard/mouse: `libSceKeyboard` (`sceKeyboardOpen/GetHandle/Init/
Read/ReadState/Close/GetKey2Char`, ps5rs stub surface) and `libSceMouse`, over
the same kernel hid/ctrlp plumbing. `SceKeyboardData` carries **raw USB HID
usage codes** (SharpProspero `Interop/Keyboard/Keyboard.cs`), confirming the
kernel HID layer sits under these libraries.

### 1.2 Userland facts that shape the design (all in-tree or sourced)

| Fact | Evidence |
|---|---|
| Pad state reads are **cached snapshots**, not queue consumers: duplicate-timestamp guard is required and sufficient | worker-real-input-pad `PAD-API-AND-INTEGRATION.md` §2 (`PS5PadSource.cpp:80-81` lineage); `scePadReadState` even synthesizes a fallback timestamp `sceKernelGetProcessTime()-1000` (ps5-native-gamepad-input-research INVESTIGATION.md) |
| Batch reads (`scePadRead`) submit **one request to the already-open platform input driver** per call, up to 64 records x 120 bytes; one process-wide mutex in libScePad | same INVESTIGATION.md |
| Pad sample layout (120 B): buttons u32 @0x00, sticks @0x04, triggers @0x08, quaternion @0x0C, accel @0x1C, gyro @0x28, touch @0x34+, connected @0x4C, timestamp u64 @0x50 | `PS5_PadData` (vendored `SDL_ps5joystick.h`); same layout in SharpProspero `Interop/Pad/Pad.cs` and the retained provider's static_asserts |
| `0x80000000` = **Intercepted** bit: system took the controller; consumers discard the sample | SharpProspero `ScePadButton.Intercepted`; PS5_PadData consumers in-tree |
| **Exclusivity lives in the open path**: `PS5_PAD_ERROR_ALREADY_OPENED 0x80920004` is returned by `scePadOpen` — the game's live handle blocks our process's open (launch-10 run3/run4 captured refusals) | `SDL_ps5joystick.h:10`; worker-pad-coexist `SDK-ANALYSIS.md` §1; worker-menu-input-map REPORT §2b |
| `scePadGetHandle(user,type,index)` is documented as "returns the handle an earlier scePadOpen produced … so a second consumer reaches the open device" — a **borrowed-handle** route that stays inside one process | SharpProspero `Pad.cs` (sdk summary); absent from Townfall's frozen import rows (pad-coexist §0.4) |
| `scePadSetProcessPrivilege(1)` lets a background daemon "receive controller input without being the foreground application" | SharpProspero `Pad.cs` |
| `scePadVirtualDevice*` family = input **emulation** (insert INTO a virtual pad) — the inverse of our read-only goal | pad-coexist `SDK-ANALYSIS.md` §0 |
| Keyboard/mouse need sysmodules `0x0106`/`0x00a9` loaded before init | ps5-native-gamepad-input-research README |

### 1.3 Where the pad state lives, and can the kernel module read it directly?

- The authoritative, always-current pad state is maintained **in the kernel** by
  the ctrlp driver (fed by hid / bluetooth_hid). libScePad is a per-process
  cache + handle/lifecycle layer on top. (PS5 Devices wiki: ctrlp is a kernel
  character device "used by libScePad"; the batch read is "one request to the
  already-open platform input driver".)
- **The ALREADY_OPENED exclusivity is enforced in the open/lifecycle path
  (libScePad + ctrlp open handling), not in the state store.** Reading the
  driver's state memory directly performs no open, holds no handle, and checks
  no privilege — there is nothing to be excluded from.
- The kelf already owns uniform, checked, cross-context kernel read primitives
  (`kpeek64_checked`, `copy_from_kernel`, DMAP physical reads) — see
  `worker-kernel-saferead/safe_read_kernel.c` §2. So: **yes, the module can
  read pad state without libScePad, provided it knows WHERE the state is.**
- Keyboard: same answer, same caveat. The kernel HID/ctrlp layer holds keyboard
  state (HID usage arrays); libSceKeyboard normalizes it per process.

### 1.4 What is NOT known (honest bounds)

1. **The ctrlp state's address/layout is not in hand.** The in-tree
   `KernelOffsetTables.cs` (5,597 lines, "empirical kdata-relative offsets")
   contains NO pad/hid/ctrlp/keyboard entries (verified by grep). Nothing
   public documents the ctrlp softc layout. Whether the state is a static
   kdata array or a heap softc, and its exact field offsets, must be derived
   per firmware from a kernel dump — exactly how every existing table entry
   (`Allproc`, `Sysents`, …) was derived.
2. Whether ctrlp keeps a sequence/lock word usable for tear detection, or only
   the per-sample timestamp. **[VERIFY on dump]**
3. Pad-port ↔ user-handle mapping semantics inside ctrlp (v1 exposes raw port
   index; the runtime picks the port the same way it picks the logged-in user
   today). **[VERIFY]**
4. Whether the PS button / system-intercept decision is made in ctrlp before
   the state store (we pass the Intercepted bit through either way).

---

## 2. Service requirements

1. ANY process — the game-hosted UE4SS PRX, a companion daemon, a probe PRX —
   can read the CURRENT controller state (keyboard: phase 2) with no libScePad
   handle, no user-service lease, no foreground status.
2. Read-only. v1 never writes input, never touches the game's own input path.
3. No new failure mode for the runtime: if the service is absent (old resident
   kstuff answers ENOSYS=78) or the running firmware has no derived offsets,
   callers must get a clean refusal and fall back to the existing native pad
   source (worker-pad-coexist staging).
4. Kekcall-context safety: no sleeping, no allocation, no locks, bounded work,
   checked reads only (a fault in this path is a kernel panic).
5. Menu/hotkey-class latency: consumed by the UE4SS UpdateThread poll (~200 Hz,
   5 ms sleep, UE4SSProgram.cpp:1682 lineage). Not a twitch-input replacement.

---

## 3. Recommended design — kekcall **nr=9 `get_input_state`** (pull model)

### 3.1 Why pull first (decision record)

- It is the **nr=7 safe_read recipe verbatim**: copyin request → kernel reads →
  copyout result via the existing MKTRAP continuation machinery
  (`safe_read_kernel.c` §6). Zero new kernel machinery classes.
- The poll loop already exists (UpdateThread → `Handler::process_event` →
  source `poll()`). Pad updates run ~243–250 Hz vs our ~200 Hz poll; a menu
  press lasts ≫ 5 ms, and the retained provider's edge semantics
  (duplicate-timestamp guard, rising edge, baseline swallow) plus the
  PadEdgeAdapter debounce operate unchanged on kernel-pulled samples.
- "Mirror to a shared page on every input event" cannot be implemented without
  first knowing when input events arrive — which requires the interrupt/poll
  hook of Alternative C (§4.3). The mirror is therefore a **superset of the
  riskiest alternative**, not a simpler one; it also re-opens the shared-page
  pmap plumbing the safe-read feasibility note already rejected (§7 of
  safe_read_kernel.c). Deferred with C as one package (§5.2).
- Stateless dispatcher: no per-process registration table, no lifecycle, no
  cleanup — the "any process" requirement is satisfied by the channel itself
  (the sysent hook is global; every process reaches the gate with no
  privileges — safe_read_kernel.c §6).

### 3.2 Userspace ABI

```c
// kekcall nr=9 — a1 = caller pointer to request (copyin'd like nr=5/nr=7)
#define KINPUT_KIND_PAD        0    // controller snapshot
#define KINPUT_KIND_KEYBOARD   1    // keyboard snapshot (phase 2)
#define KINPUT_F_STRICT        1    // refuse torn snapshots instead of flagging

struct kinput_request {
    uint32_t kind;          // KINPUT_KIND_*
    uint32_t index;         // pad port (0..3) / keyboard index
    uint64_t buf;           // CALLER VA to receive the snapshot
    uint32_t len;           // buffer size; must be >= sizeof(snapshot)
    uint32_t flags;         // KINPUT_F_*
};
// retval >= 0 : bytes copied to req->buf
// retval < 0  : -KINPUT_ERR_*
```

Normalized pad snapshot — a **service-defined, firmware-invariant** 136-byte
struct (NOT the raw kernel struct, so the ABI survives FW drift; the kernel→
normalized mapping is emitter code updated with the offset tables):

```c
struct kinput_pad_state {              // 136 bytes, v1 ABI "KIN1"
    uint32_t magic;            // 'KIN1' — caller sanity check
    uint32_t buttons;          // ScePadButton layout; 0x80000000 = Intercepted
    uint8_t  lx, ly, rx, ry;   // 0..255, center 128 (zero is NOT neutral)
    uint8_t  l2, r2;           // 0..255 analog triggers
    uint8_t  connected;        // 0/1
    uint8_t  torn;             // 1 if tear-retry budget exhausted (see 3.4)
    uint16_t seq;              // driver sample counter if one exists, else 0
    uint64_t timestamp_us;     // kernel stamp if available, else 0
    uint8_t  touch_fingers;    // 0..2
    uint8_t  _r0[3];
    uint16_t touch_x[2], touch_y[2];
    uint8_t  touch_id[2];
    uint8_t  _r1[2];
    uint32_t connection_gen;   // changes on reattach (ConnectedCount proxy)
    uint32_t _r2[3];
};
```

Error codes (namespace continuation of the safe-read family, which ends at
-15): `-16 KINPUT_ERR_BADARGS`, `-17 KINPUT_ERR_KIND` (unknown kind / phase-2
kind not built), `-18 KINPUT_ERR_PORT`, `-19 KINPUT_ERR_NOSTATE` (no derived
offsets for this firmware — clean refusal, callers fall back), `-20
KINPUT_ERR_TORN` (strict mode only), `-21 KINPUT_ERR_STRUCT` (state unreadable
— treat as transient), `-78` ENOSYS = old resident module.

### 3.3 Kernel-side state locator

Abstracted as: **resolved root pointer + fixed chain of checked reads,
baked per firmware at install time** (`AddSym`, exactly how `allproc` is
resolved — `KernelModuleInstaller` step 7 lineage). Two derivation paths; the
tables carry whichever the dump supports (both may coexist; A preferred):

- **Path A — static state root.** If ctrlp keeps its port state in a kdata
  static (common for console port arrays), the entry is one kdata-relative
  offset + intra-struct field offsets: `PadPortState(fw)`, `PadStateButtons`,
  `PadStateTimestamp`, … . Same table shape as `Allproc`.
- **Path B — fd-anchored softc walk.** If the state is a heap softc, anchor it
  through a process that holds ctrlp open (the game always does while
  libScePad is inited; SceShellCore almost certainly keeps one open too):
  `kproc_find_by_pid` (exists) → `proc+0x48 p_fd` (exists) → filedesc
  ofiles array **[VERIFY]** → `struct file` → device-indirect chain to
  `si_drv1` softc **[VERIFY — devfs vnode→v_rdev→cdev or Sony variant]** →
  port state. Every [VERIFY] constant comes from the same one-time dump
  analysis; the walk reuses the bounded-iteration discipline of the allproc
  walk (cap 4096, abort on unreadable link, never dereference an unpinned
  pointer without a checked read).

If neither path resolves for a firmware, nr=9 answers `-19` for that boot and
the runtime falls back. Unknown firmware never guesses.

### 3.4 Torn-read protocol (racy-but-safe)

The state is written asynchronously by the input path; we take no locks
(deadlock/panic territory in kekcall context). Instead:

1. Read the full snapshot via checked reads.
2. Re-read `buttons` and `timestamp`/`seq`. If unchanged → consistent, done.
3. Retry up to 3 times; then set `torn=1` (or return `-20` when
   `KINPUT_F_STRICT`). Digital edges are effectively immune (single u32,
   double-read protected); only analog/touch fields can tear, and only under
   an input storm — acceptable for menu-class consumers, and the flag makes
   it observable. If the dump reveals a driver seq/lock word, step 2 uses it
   directly **[VERIFY]**.

Cost per call: ~20 `kpeek64_checked`-class reads + one copyout. Same cost
class as nr=7 with a 136-byte payload — well under the 64 KiB budget.

### 3.5 Dispatch wiring (emitter form)

Follow the nr=1/nr=5/nr=7 recipe documented in `safe_read_kernel.c` §6 and the
emitter's `EmitKekcallHandler`:

1. New arm `nr == 9` in `EmitKekcallHandler`
   (tools/SharpProspero.Link/KernelModuleEmitter.cs; current resident arms:
   -1 ping, 1 dbregs read, 2 dbregs write, 3 rdmsr, 5 remote syscall, 6
   snapshot; 4 unused; 7/8 already proposed by the safe-read worker — nr=9 is
   free; the kernel-debugger gdbstub is a userspace RSP server and claims no
   number).
2. Request copyin leg → resolve locator (install-time symbol, no runtime
   search) → guarded snapshot read + tear check (3.4) into kelf BSS scratch →
   copyout leg to `req->buf` (the nr=1 redirect pattern: RIP=copyout,
   RDI=buf, RSI=scratch, RDX=len; zero `td_retval` first so a copyout fault
   surfaces as EFAULT).
3. Client header `kstuff_input_client.h` mirroring `safe_read_client.h`:
   same gadget derivation (`sceKernelDlsym("getpid") + 0xA`), same 15-
   instruction `__sp_kekcall`-identical shim, plus
   `kinput_pad_read(index, struct kinput_pad_state*)`.

Deployment constraint (unchanged from safe-read §7): the resident module's
dispatch table is frozen machine code; unknown nr → ENOSYS. Extension = edit
`KernelModuleEmitter.cs`, rebuild kelf/uelf (`build/build-kstuff.ps1`), deploy
on next boot (installer refuses mid-flight overwrite by design). One rebuild
can carry nr=7/8/9 together.

---

## 4. Alternatives compared (decision table)

| # | Approach | Fidelity | Any process? | New kernel machinery | New import rows | Verdict |
|---|---|---|---|---|---|---|
| A | **nr=9 pull of kernel state** (§3) | snapshot @ caller poll rate (~200 Hz) | YES — global sysent gate | none beyond nr=7 recipe + per-FW locator offsets | ZERO (syscall gadget only) | **RECOMMENDED v1** |
| B | `register_input_mirror(buf)` — kernel writes a page on every input event | event-exact | YES | requires knowing event time = Alternative C's hook, PLUS pmap sharing into each consumer (rejected in safe-read §7), PLUS user-page writes from driver context | zero | **REJECTED as primary** — strictly more risk than C for no consumer-visible gain; subsumed by C+drain in phase 2 |
| C | Hook ctrlp's input write path → kelf-internal ring → drain kekcall | lossless event stream | YES | NEW CLASS: driver-text code hook (sysent hook was a data-pointer swap; this is patching driver code) re-entrancy at input context; fault = panic | zero | **DEFERRED to phase 2/3** behind evidence that A misses real edges (see §5.2). Note the safety inversion: even then, the hook writes only to kelf-internal ring memory; userspace drains via copyout kekcall — NEVER to user pages from the input path |
| D | Borrowed handle: in-process `scePadGetHandle(user,type,index)` reusing the GAME's open handle | exact (same handle the game reads) | NO — game process only | none | +1 (`scePadGetHandle` not in Townfall's frozen rows) | Keep as the pad-coexist worker's separate userland probe; it cannot serve priority #6's any-process goal |
| E | `scePadSetProcessPrivilege(1)` + own `scePadOpen` from a companion daemon | exact | YES (daemon) | none | +2 | Undecidable in-tree whether privilege bypasses per-tuple exclusivity; secondary probe only, not the durable answer |

D and E remain worth running as cheap userland probes (they answer different
questions), but neither is an architecture: D is process-local, E is
undocumented. A is the only route that is simultaneously handle-free,
privilege-free, and process-agnostic.

---

## 5. Phase plan for keyboard and (conditional) event ring

### 5.1 Keyboard (phase 2)

`kind=KINPUT_KIND_KEYBOARD` returns the 96-byte `SceKeyboardData` shape
(SharpProspero `Keyboard.cs`: timestamp, intercepted, connected, length, LED
locks, modifier mask, `KeyCode[16]` raw USB HID usages). Kernel-side locator:
same derivation step over the hid/ctrlp keyboard state **[VERIFY]**. UE4SS
mapping needs a HID-usage → `Input::Key` table (KeyDef has PC-style names;
a usage→key table is straightforward and host-testable, but it is NEW runtime
code and thus its own reviewed packet). Keyboard sharing has the same
any-process value (companion UI typing) and the same bypass property.

### 5.2 Event ring escalation (phase 3, conditional trigger)

Escalate to Alternative C only if a console run shows the pull route missing
real user edges (symptom: physical press with no `pad_event_edge` tag while
torn counters stay low). Design sketch (for the future packet, not this one):
kelf-BSS ring 256 × 136 B, single-writer (input hook) / multi-reader (drain
kekcall nr=10 with per-consumer cursor, copyout up to N records), seq-stamped
slots, overflow counter. The hook itself is a text patch on the ctrlp write
function with per-FW offset — a new machinery class requiring its own
review; it must be uninstall-safe (original bytes captured like the IDT gates
already are).

---

## 6. UE4SS runtime integration

### 6.1 New PlatformInputSource: `KekcallPadSource`

New unit in the pad-provider lineage
(`ue4ss-native-pad-provider-1/port/KekcallPadSource.{hpp,cpp}`), one injected
backend seam in the established style:

```cpp
struct KinputBackend {                    // the ONLY kernel-service surface
    long (*kekcall)(int nr, long a1, ...);          // shim from client header
    // host tests inject a fake; console wires kstuff_input_client.h
};
```

- `activate()`: (1) `kekcall(-1)` ping == 0; (2) one `nr=9` read of port 0
  must return >= 0 (or `-19 NOSTATE` refused). Both pass ⇒ `is_available()`.
  **No PadServiceLease, no sceUserService, no scePadOpen — the ALREADY_OPENED
  failure class does not exist on this route.**
- `poll(subscriptions, allow)` (called once per UpdateThread tick,
  ~200 Hz, through the unchanged `Handler::process_event` chain): one `nr=9`
  read → fill a `PS5_PadData`-shaped local (field-for-field from
  `kinput_pad_state`) → **retained edge semantics verbatim**: duplicate-
  timestamp guard (`timestamp_us`/`seq`), first-sample baseline swallow,
  rising-edge emission `down && !m_down[i] && allow && subscribed[key]`,
  held-state tracked while gated, `connected==0` or `Intercepted` bit ⇒ clear
  state without events. PadEdgeAdapter debounce stays downstream unchanged.
- Port selection: poll ports 0..3 at activate, lock onto the first connected
  (mirrors the native provider's first-logged-in-user selection); re-scan on
  `connected==0` for one poll, then back off (bounded, same-thread — the
  coexist retry discipline).

### 6.2 Config surface (one file, unchanged shape)

```ini
[PS5Pad]
User = -1                ; retained (native route only)
Source = auto            ; auto | kernel | native  (auto = kernel if alive, else native)
[Bindings]
OPTIONS = F7             ; unchanged — profile decides chords, mod stays untouched
...
```

`NativePadInput.cpp` gains source ordering: `Source=auto` tries
`KekcallPadSource` first (availability = ping + one read), else attaches the
existing coexist native source (deferred-attach retry staging). A kernel
refusal (`-19`/ENOSYS) is a NORMAL fallback event, never a throw — this
preserves outcome A in all combinations (contrast: current native path throws
on pad failure, worker-menu-input-map REPORT §2b).

### 6.3 Diagnostics (closes rung-4 visibility for kernel-route runs)

Emit on the shared supplier-diag channel (R4 vocabulary): `kinput_alive`
(ping), `kinput_ok` / `kinput_torn` / `kinput_stale` (timestamp age > 1 s at
poll) counters, `kinput_port` on lock/relock, and — the rung-4 tag the
menu-input-map worker identified as MISSING (G-AP1) — one `pad_event_edge
key=<K>` record per emitted edge. The composed runtime finally gets a direct
"controller input ARRIVED at the handler" signal.

### 6.4 Packaging win

The kernel route needs **zero libScePad/libSceUserService import rows** (raw
syscall gadget only, resolved via `sceKernelDlsym("getpid")`). The pad rows
(14, Townfall-declared) remain needed only for the native fallback source.
No loader-acceptance experiment is added by the kernel route itself.

### 6.5 Any-process consumers (priority #6 payoff)

Because the gate is global, the same `kstuff_input_client.h` drops into: the
game-hosted PRX (above), a future companion daemon (menu/UI typing via
keyboard kind), and standalone probe PRXs — each with no pad privilege, no
foreground status, no handle. The first any-process proof point is Stage V of
§7 (probe PRX outside the game reads the pad while the game plays).

---

## 7. Implementation plan (stages, each independently gated)

| Stage | Deliverable | Gate / evidence |
|---|---|---|
| 0 | This document | design review |
| 1 | **Offset derivation note**: from the dump sources used for `KernelOffsetTables`, derive Path A/B locator constants for the target FW family; record ctrlp state layout + seq word presence; extend `KernelOffsetTable` + installer `AddSym("kinput_pad_state", …)`. Plus reference-C prototype `kinput_kernel.c` (safe_read style, runnable nowhere, review-only) | every offset traceable to a dump address; no offsets ⇒ design stops honestly at stage 1 |
| 2 | Emitter port: nr=9 arm in `KernelModuleEmitter.cs` + kelf/uelf rebuild carrying nr=7/8/9 in one shot; `kstuff_input_client.h` | code review vs reference C; ENOSYS-free dispatch smoke on host is impossible — gate is review only until deploy |
| 3 | Deploy on next boot (installer gate; never overwrite live module) + `KEKCALL_CHECK(-1)` ping | ping == 0 with new build resident |
| 4 | Standalone probe PRX (pad_probe lineage, `/data` JSONL sink): ping → one read → 30 s press routine; record torn/stale counters | JSONL shows live button bits tracking physical presses; no game involvement required |
| 5 | Console ladder inside a game session: probe PRX co-loaded (ISOLATED from the composed runtime — one experiment per launch) reading pad WHILE the game holds its handle — the any-process proof | JSONL buttons track presses during gameplay |
| 6 | `KekcallPadSource` + `Source=auto` ordering in the composed runtime; host tests (§8) | host suite green |
| 7 | Composed-runtime console run with `pad_event_edge` tags | rung-4 evidence: OPTIONS press → tag → menu callback |

Keyboard (phase 2) and ring (phase 3) get their own packets after Stage 7.

## 8. Test plan

Host (host-1 recipe, ASan+UBSan, real Handler + real KekcallPadSource, fake
KinputBackend): rising edge per retained semantics; duplicate-timestamp
suppression; baseline swallow on attach; `torn=1` samples still emit
consistent DIGITAL edges (analog tearing must not fabricate or drop button
edges); `Intercepted` sample discarded; disconnect → state cleared, no
phantom edges on relock; ping fail → unavailable → auto falls back to native
source; `-19`/ENOSYS → same fallback; backoff discipline on port relock;
lifecycle: every kekcall on the driver thread only.

Console (stages 4-7 above; each one bounded launch, one variable per launch
per team discipline).

## 9. Risks & unknowns

1. **The design hinges on Stage 1** (offset derivation). If no dump source
   yields the ctrlp locator, the kernel route is blocked and the fallbacks are
   D (in-process) / E (privilege) — both weaker. This is the single biggest
   unknown and it is stated, not hidden.
2. Torn analog/touch fields under input storms (flagged, strict mode
   available); digital edges double-read protected.
3. Firmware drift → per-FW tables + clean `-19` refusal on unknown FW (never
   guess; same policy as allproc).
4. Kekcall-context discipline: any sleeping/locking/unbounded walk in the nr=9
   arm is a kernel panic — the review checklist must enforce checked reads,
   bounded retries (3), no allocation.
5. Port↔user mapping and Intercepted semantics in ctrlp are [VERIFY]; v1
   passes Intercepted through and exposes raw ports.
6. Read-only scope is a hard boundary: no vibration/lightbar/trigger-effect
   write path, no virtual-device injection, no suppression of the game's own
   input (UE4SS keybinds stay non-exclusive notifications).
7. Deploy friction: kernel service ships only with a kstuff rebuild + reboot
   (same as safe-read); until then every caller sees ENOSYS and must fall
   back — the runtime design already treats this as normal.

## 10. Sources

In-tree (worktree paths):
- `outputs/2026-10-04-zcode-team/worker-kernel-saferead/safe_read_kernel.c` + `safe_read_client.h` — kekcall channel, nr=7/8 recipe, feasibility notes (§7)
- `outputs/2026-10-04-zcode-team/worker-pad-coexist/SDK-ANALYSIS.md`, `REPORT.md` — open-class verdict, retry staging, V0–V5 probe matrix
- `outputs/2026-10-04-zcode-team/worker-real-input/PAD-API-AND-INTEGRATION.md`, `worker-real-input-pad/PAD-API-AND-INTEGRATION.md`, `worker-real-input/INPUT-MAP.md` — provider API, edge semantics, dispatch chain
- `outputs/2026-10-04-zcode-team/worker-menu-input-map/REPORT.md` — rung table, G-AP1 gap, UpdateThread cadence
- `outputs/2026-10-04-zcode-team/worker-pad-hardware-probe/REPORT.md` — probe PRX lineage, JSONL sink, isolation rules
- `.superpowers/native-dependency-spike/sharpprospero-legacy-ctors-1/source/` — `Interop/Pad/Pad.cs`, `Interop/Keyboard/Keyboard.cs`, `samples/prospero-input/`, `src/SharpProspero/Payload/Kernel/KstuffSharedArea.cs`, `KernelOffsetTables.cs` (no input symbols — verified), `tools/SharpProspero.Link/KernelModuleEmitter.cs` (dispatch arms), `PayloadCrtEmitter.cs` (`__sp_kekcall`)
- `worker-pad-coexist/staging/.../SDL_ps5joystick.h` — `PS5_PadData`, button bits, error codes

Web:
- PS5 Devices — psdevwiki.com/ps5/Devices (ctrlp/ctrlp_sync "used by libScePad"; hid; bluetooth_hid; bt; wlanbt; usbctl)
- PS4 Devices — psdevwiki.com/ps4/Devices (hid, bluetooth_hid, ctrlp "like on PS Vita DevKit")
- blackbearreloaded/ps5-native-gamepad-input-research — README + INVESTIGATION.md (driver-request batching, read-core mutex, 120 B records, ReadState snapshot/timestamp fallback, 243–250 Hz, keyboard/mouse batching, sysmodules 0x0106/0x00a9)
- claimore22/ps5rs — libSceKeyboard stub surface (`sceKeyboardGetHandle` included)
- OpenOrbis PS4 Toolchain Pad docs; Andy Nguyen RomHack 2024 PS4 kernel talk (Orbis kernel = FreeBSD-derived with custom drivers)
