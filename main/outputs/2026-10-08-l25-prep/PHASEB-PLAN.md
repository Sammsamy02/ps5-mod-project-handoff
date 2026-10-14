# PHASE B PLAN — kstuff-lite kernel bridge (#1 reads, #3 patching, #6 input)

Agent GD (ZCode), 2026-10-10. Claude round-5 Part 2, packet PHASE B. Companion
docs: `FZ-PHASEB-KSTUFF-LITE.md` (FZ's interface research — this plan extends
its §2 interface and §5 safety plan; evidence tags [SRC]/[LOG]/[META]/[POST]/
[MISS]/[SP]/[INF] below are defined there) and
`PHASEC-EARLY-LOADING-DESIGN.md` (the #4 packet, same round).

## 0. LAW (binding, every section of this doc)

1. **NOTHING runs on the console without Claude review + explicit user OK for
   that exact step.** This document is a DESIGN ONLY. Mac on battery during
   authoring: no builds, no console contact, no tree source edits. The only
   artifacts created are this file and its sibling doc.
2. The **kstuffSharp/InstallerApp kernel module stays QUARANTINED** (FW 10.01
   offsets crashed the 13.60 kernel Oct 6; POSTMORTEM rule 7). Phase B builds
   on the console's resident **stock kstuff-lite v1.11** only — we CALL its
   existing services; we never replace or modify it.
3. **Fake-sign / executable laws apply to every fetch and every byte we touch:**
   any executable-class file downloaded from the console is fetched with
   `-Q "SITE SELF"` (decrypt-off) for provenance; `7f454c46` in a download
   means decrypted view, never the real file; we NEVER upload any downloaded
   or modified executable back; modified executables need the make_fself
   fake-sign pipeline and are OUT OF SCOPE for Phase B (we patch runtime
   memory only — see §3).
4. **No rapid-fire launches** (ShadowMount kstuff pause 15–25 s, resumes only
   on clean game exit; a burst of failed launches leaves all games failing
   auth). One user-launched test per boot, at explicit bounds.
5. kstuff-lite has **no unload path** — once resident, only a reboot replaces
   it. Every design below assumes it is ALREADY resident from autoload (first
   line of autoload.txt, verified at every recorded bootstrap [LOG]) and we
   add nothing kernel-resident.

## 1. Readiness snapshot (what blocks Phase B today)

| Item | State | Evidence |
|---|---|---|
| kstuff-lite v1.11 binary on console | resident every boot, `state=1 readable=1 crypt=deb7/deb7` | [LOG] etaHEN startup |
| kstuff-lite v1.11 SOURCE on Mac | **MISSING — the open blocker** (only pinned FULL-kstuff `main.c` pre-v1.6.x lineage exists) | FZ §7.1 |
| kekcall calling convention | pinned (family source + project CRT shim `__sp_kekcall`) | [SRC main.c:922] + [SP PayloadCrtEmitter.cs] |
| Which kekcall numbers lite v1.11 services | **UNKNOWN** (FULL table known; lite subset unverified) | FZ §2.1 |
| 0xdeb7 syscall-range ABI | **UNKNOWN until source/binary pinned** | FZ §2.3 |
| Project client-side kekcall shim | exists (`SharpProspero.Link` CRT emitter; `mov r9,[rsp+8]; shl rax,32; or rax,0x27; call [rip+ptr_syscall]`) | [SP] |

Phase B cannot write client code against the interface until §2 (pinning)
runs. Everything after §2 is design-complete and gated on its outputs.

---

## 2. SOURCE PINNING PROCEDURE (console-off design; run only per §0 law)

Goal: **source <-> binary identity for the exact kstuff-lite v1.11 on the
console.** Byte-identical rebuild or bust — if the rebuild does not reproduce
the console bytes, the source is advisory-only and the interface is pinned
from the BINARY instead (§2 step P3), with writes additionally re-reviewed by
Claude. Never touch the quarantined kstuffSharp module at any step.

### P1. Fetch the binary read-only, decrypt-off (the PS5 fetch law)

The ftpsrv decrypt law applies to ELF/SELF payloads: a plain default download
of an executable-class file yields the DECRYPTED view (magic `7f454c46`),
fakes size, and hashes differently. `kstuff-lite_v1.11.elf` is an
executable-class payload, so provenance fetches MUST use decrypt-off:

```bash
# decrypt-OFF fetch (provenance copy — the real distributed bytes)
curl -sS --ftp-pasv -Q "SITE SELF" \
  -o kstuff-lite_v1.11.decrypt-off.elf \
  ftp://ps5-host-not-configured.invalid:2121/data/pldmgr/payloads/kstuff-lite/kstuff-lite_v1.11.elf
xxd -l 4 kstuff-lite_v1.11.decrypt-off.elf   # magic readback
shasum -a 256 kstuff-lite_v1.11.decrypt-off.elf
```

- Expected magic: `4f153d1d` or `5414f5ee` (signed SELF container) — this is
  the REAL file. If instead `7f454c46` appears in the decrypt-off fetch, the
  console copy is genuinely a plain-ELF payload: STOP and document (the
  catalog hash comparison below then decides identity); do not guess.
- Provenance hash to compare: pldmgr mirror metadata records
  `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779`
  for kstuff-lite v1.11 [META `worker-boot-protocol/probe/list_payloads.json`].
  - MATCH -> the console file IS the mirror's v1.11 distribution; pinned.
  - MISMATCH -> the console runs something else; STOP, report to Claude with
    both hashes; nothing downstream may proceed.

Optionally (analysis convenience ONLY, never re-uploaded, distinct name):
a second DEFAULT (decrypt-on) download `kstuff-lite_v1.11.decrypted-view.elf`
gives the plain-ELF view for §P3 disassembly when the provenance copy is a
SELF container. Both files stay Mac-local forever.

### P2. Version-string extraction

```bash
strings -a kstuff-lite_v1.11.decrypt-off.elf | \
  grep -iE 'kstuff|v1\.1[0-9]|version|echostretch|plk|lite' | sort -u
# same on the decrypted-view copy when it exists (SELF containers hide strings)
strings -a kstuff-lite_v1.11.decrypted-view.elf | grep -iE 'kstuff|version|v1\.' | sort -u
```

Record every hit verbatim in the session receipt. Cross-check against the
pldmgr catalog (version `1.11`, mirror last_update 2026-09-20 [META]) and the
resident-process facts (`kstuff.elf` pid, "initialization/remount/shell patch"
klog lines [LOG]). The version string is the cheap human-readable pin; the
sha256 in P1 is the authoritative one.

### P3. Static interface extraction from the fetched binary (binary ground truth)

Read-only Mac-local analysis; needed regardless of P4's outcome because the
RUNNING binary — not the source — is what services our calls:

1. Disassemble with the host `llvm-objdump -d` (aarch64; the pinned SDK at
   `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk`
   toolchain or Xcode LLVM both parse FreeBSD-flavored ELF).
2. Enumerate kekcall numbers by searching for the packed 64-bit immediates
   the family source constructs [SRC main.c:221-226]: `0x100000027`
   (READ_DR), `0x200000027` (WRITE_DR), `0x300000027` (RDMSR),
   `0x500000027` (REMOTE_SYSCALL), `0xffffffff00000027` (CHECK) — typically
   materialized via `movz/movk` pairs or `.rodata` constants. Every hit (or
   verifiable absence) is recorded: **this is the lite v1.11 kekcall
   inventory.**
3. Enumerate the `0xdeb7` sysent extension markers and any dispatch table in
   the binary; record the serviced syscall-number range and per-number
   argument load pattern (which of a1..a6 each handler consumes).
4. Record the gadget-offset facts for §3.1: locate kstuff's own
   `dlsym(0x1,"getpid")`-derived call and the exact stub offset it uses
   (family source says `+7` [SRC main.c:922]; the project CRT derived `+0xa`
   in a different loader context [SP]). The PINNED BINARY's own usage wins.

### P4. Source acquisition + rebuild identity (byte-identical or bust)

1. Acquire the EchoStretch/kstuff source at the exact v1.11 release (the
   upstream whose releases the itsPLK mirror distributes [META]). Honor the
   no-duplicate-fetch rule: reuse any already-fetched copy on the Mac before
   any new web fetch; otherwise request the fetch explicitly.
2. Build the lite target with the pinned SDK v0.43 toolchain, recording
   toolchain version, flags, and any stamp inputs in the receipt.
3. `shasum -a 256` the rebuild vs the P1 provenance copy:
   - **BYTE-IDENTICAL -> PASS.** Source is pinned to the console binary;
     §3 interface entries marked [PINNED-SRC] become authoritative.
   - **NOT identical -> BUST (do not iterate toward "close enough").**
     Distinguish toolchain drift from source drift: compare §P3's disasm of
     key functions against source-compiled objects. Either way the source is
     ADVISORY-ONLY: the interface remains pinned from the binary (P3), and
     any WRITE-capability use (§3, #3) additionally requires explicit Claude
     re-review with the divergence documented. Report the bust; do not
     silently proceed.

Known hazard, stated up front (FROZEN-KNOWN-GOOD §5 precedent): our own l26
builds are non-reproducible because stamps regenerate per build. If upstream
v1.11 embeds build timestamps, byte-identity can fail with fully-correct
source; that is still a BUST under this procedure — the binary stays the
ground truth and the report says exactly which bytes diverged.

### P5. Console acceptance of the pin — 3 observe-only passes across restarts

Per the mission's three-pass law, the pin itself is accepted only from THREE
console passes across THREE restarts, each pass OBSERVE-ONLY (no writes, no
new loads, no game launch required):

Per pass:
1. Fresh boot; kstuff-lite autoloads first, unchanged (autoload.txt
   untouched).
2. Verify residency read-only from the etaHEN bootstrap log:
   `kstuff state=1 readable=1 ... crypt=deb7/deb7` then `resident kstuff
   found` [LOG pattern].
3. Repeat the P1 decrypt-off fetch; magic + sha256 MUST equal the P1 values.
4. Repeat the P2 strings extraction; version record MUST match P2 exactly.

PASS = 3/3 passes identical hash + version + residency across 3 restarts.
Any variance -> STOP, report; the pin is not accepted. (This observes only
stock behavior — the KEKCALL_CHECK probe is a later, separately authorized
step under §4's L1 rung.)

---

## 3. INTERFACE DOC (extends FZ §2 to per-capability client contracts)

Everything here is design against the FAMILY interface [SRC] + project CRT
[SP]; entries become executable contracts only after §2 P3/P4 pin the lite
v1.11 subset. Quarantine reminder: none of this touches kstuffSharp.

### 3.0 Shared calling convention (the kekcall gate)

- Discovery: resolve a syscall gadget from the dynamic linker —
  `p = dlsym(handle 0x1, "getpid")`; the family uses `p + 7` [SRC
  main.c:922]; the project CRT's variant uses `+0xa` in loader contexts [SP].
  The exact offset for our client is taken from the PINNED BINARY's own usage
  (P3.4). A client MUST verify the gadget stub bytes against the expected
  signature before the first call and REFUSE on mismatch (fail-closed; the
  skeleton in §5 does this).
- Dispatch: syscall number `0x27` (SYS_getppid) with the kekcall sub-number
  packed in `RAX[63:32]`; arguments a1..a6 in the six argument registers.
  Project shim shape [SP `__sp_kekcall`]:
  `rax = (nr << 32) | 0x27; args shifted down one slot; call [ptr_syscall]`.
- Residency: `KEKCALL_CHECK = 0xffffffff00000027` returns nonzero when a
  kstuff-family module is resident; re-runs are CHECK-guarded no-ops [SRC
  main.c:924-928]. Console-verified resident at every boot [LOG].

### 3.1 Capability #1 — kernel READS: the actual read primitive ladder

| Rung | Primitive | Kernel mediation | Risk | Status |
|---|---|---|---|---|
| R0 | In-process reads (image-segment/VirtualQuery admission) | none | none | console-proven (the port's own runtime, e.g. slot-1 pool admission via image-segments, session log L551) |
| R1 | `KEKCALL_CHECK` | read-only gate probe | none (read) | design; first kernel-adjacent call allowed |
| R2 | Plain syscalls in OWN pid: `SYS_dynlib_get_info_ex 608` (module base/segments), sysctl `KERN_PROC` (pid enumeration) | none (stock syscalls) | none | family-proven channels [SRC] |
| R3 | `KEKCALL_REMOTE_SYSCALL 0x500000027`: `kekcall(pid, nr, args_ptr, 0,0,0, 0x500000027)` — execute syscall `nr` in an ARBITRARY pid | kstuff-resident kernel path | medium (runs code path in target pid) | family-proven [SRC]; args_ptr copy semantics must be pinned from lite binary (P3) |
| R4 | Kernel-memory reads via the `0xdeb7`-extended syscall range / kstuff `kmemcpy/copyout` | kstuff-resident kernel path | medium | **UNKNOWN whether lite services any read kekcall — pin from P3; do NOT assume FULL's surface** |

The actual cross-process READ primitive is R3: a remote syscall in the target
pid (e.g. remote `sysctl KERN_PROC_VMMAP` / `dynlib_get_info_ex`), with
results copied back through the args buffer — the same channel class the
family source uses to patch SceShellCore remotely. R4 exists only if P3
finds a read kekcall in the lite inventory. R0/R1/R2 need no new authority
at all; R3+ are gated per §4.

Design rule: every read client starts at R1 (CHECK) and may not climb a
rung until the rung below has 3 console passes (§4 ladder).

### 3.2 Capability #3 — code PATCHING (runtime memory only; disk NEVER)

- **Write path:** `SYS_mdbg_call 573` — the kernel write/patch channel the
  kstuff family itself uses to patch other processes (SceShellCore
  precedent) [SRC]. Called from our userspace payload once kstuff is
  resident; alternatively a write kekcall if the P3 inventory lists one.
- **ARM64 i-cache question (OPEN — pin before ANY instruction patch):**
  after writing instructions into a target process, the i-cache of the
  executing cores must be invalidated or the target may execute stale
  instructions. Design mandates, in order:
  1. Determine from the pinned source/binary whether the mdbg write path
     invalidates the target i-cache (upstream kstuff patches running code
     with it, which is suggestive but NOT proof for our call pattern —
     pin it).
  2. Until proven, restrict #3 to DATA-page patches (vtables, GOT/function
     pointers, flag words) — the port's own VMT-pump/pointer-redirect
     family needs no i-cache sync and is the proven first target class.
  3. Instruction patches only with a proven target-side sync mechanism
     (e.g. R3 remote syscall executing a cache-clearing self-modification
     stub in the target, or documented mdbg resume semantics).
- **Fake-sign law interaction (explicit):** patching RUNTIME MEMORY of a
  running process is NOT executable-file modification and does not involve
  fake-signing. The laws that DO bind: never write `eboot.bin`/`*.self`/
  `*.sprx`/`*.prx` FILES on disk; never upload any downloaded or modified
  executable back (the Oct 6 brick class); runtime patches are
  revert-first — save original bytes before patching, verify readback of
  the saved originals, revert on demand, receipt both directions.
- **Why this capability matters in-port:** the eboot text pages are
  EXEC-ONLY to the in-process runtime (VMMAP evidence; the detour verifier
  never succeeded on this target). An mdbg-mediated write from outside may
  reach pages the in-process runtime cannot — that is the #3 value
  proposition, and it stays behind the §4 write rung.

### 3.3 Capability #6 — controller kernel route: ASSESSMENT = userspace ScePad is sufficient; kernel route DECLINED

- What kstuff exposes for input at kernel level: **nothing** in the pinned
  family source — the kekcall table has no pad/input numbers (READ_DR/
  WRITE_DR/RDMSR/CHECK/REMOTE_SYSCALL only), and its IDT int1/int13 hooks
  are debug-register intercepts, not an input path [SRC].
- What the port already has: **userspace ScePad via PS5PadSource** —
  pad open/borrow with the >=10 s uptime rule, ~5 ms poll loop, L3+R3
  rebind with OPTIONS left to the game — console-proven across sessions
  (menu-up session used it; PS5PadSource.cpp contract: no busy loop, no
  extra thread).
- UE4SS semantics on PC are satisfied by USERSPACE OS input (UE4SS reads
  window messages / OS key events; there is no kernel input route on PC
  either). Parity therefore does not require a kernel route.
- A kernel input route would require NEW kernel-resident code (there is no
  existing service to call) — prohibited by our fail-closed bias (§0.5,
  §4 L4) and would risk hook conflicts with resident kstuff.
- **Verdict: #6 = DECLINED for this port; userspace ScePad retained.**
  Revisit trigger (documented, not planned): a future mod that needs input
  while another process owns the pad, or input injection into a
  non-focused process. That would be a NEW Claude review, not this phase.

---

## 4. SAFETY + RECOVERY PLAN (binding)

### 4.1 Per-capability risk ladder (climb one rung only after 3 console passes on the rung below)

| Level | Activity | Authorization |
|---|---|---|
| L0 | Observe residency (etaHEN log, decrypt-off fetch/hash/strings) | read-only; still needs user OK per session |
| L1 | Load OUR probe payload once; `KEKCALL_CHECK` (R1) | Claude review + user OK; no writes |
| L2 | Read rungs R2/R3 (own-pid info; remote syscall READS) | Claude review + user OK; saves backed up first |
| L3 | Runtime-memory DATA patches via mdbg (revert-first) | Claude review + user OK; regression launches after |
| L4 | ANY new kernel-resident code / modified kstuff build | **PROHIBITED in this phase** — separate explicit Claude + user authorization with its own recovery pre-proof (FZ §5 modified-build rules apply if ever reached) |

### 4.2 No-autoload law for anything kstuff-mediated

- `autoload.txt` is NEVER edited by Phase B work. Stock kstuff-lite stays
  first line; ploader stays last line. Nothing of ours ever enters
  autoload.
- All Phase B client payloads load per-launch: ONE
  `pldmgr /loadpayload` dispatch with receipt, after confirmed exit of any
  prior resident (the ploader lock discipline, agent Z2); never duplicate
  dispatch.

### 4.3 Kill-switch design

- Unmodded play: empty `ploader.ini` (the verified disarm; Z2 §safe-operator).
- Our client payloads are USERSPACE processes: they exit naturally
  (module_stop / lifecycle) or can be stopped at their explicit bounds via
  pldmgr `/process_kill?pid=<own payload pid>` — never any other process.
- The [KB] probe TU itself (§5) is compile-time OFF by default and
  runtime-double-gated; a shipped PRX without the gate cannot make any
  kekcall.

### 4.4 Crash-signature monitoring (klog, TCP 3232, NEW-file rule — never truncate)

Watch list per session (one-shot profile, agent Z1 patterns + kernel
additions):

```
"A user thread receives a fatal signal"      # user crash header
"# proc name: eboot"                          # crashed game process
"nonsleeping lock"                            # degraded dump walk
"SCE_SYSCALL_EVENT_APP_CRASH" / "App Crash : PID"
"dbggc_push_only_protection_fault_info_to_user"
"GPU Protection fault"
"0xa0020102"                                  # module load error class
"0xa002031f"
"coredump" / "panic"                          # KERNEL coredump — the Oct 6
                                              # kstuffSharp boot signature
"can't start" / readHeader / kern_get_self_auth_info  # bricked-exec symptom
"modbridge"                                   # our payload in a dump
```

- Any KERNEL coredump/panic hit => STOP. No relaunch (no-rapid-launch law:
  ShadowMount kstuff pause 15–25 s, resume only on clean exit); roll back
  (§4.5) and report. `kstuff_crash_detection=0` stays as-is; check
  `autotune.ini` after every crash [MISS l12-D, POST].
- Baselines for comparison: agent Z1's plugin-run crash profile and the
  Oct 6 POSTMORTEM kernel-crash timeline.

### 4.5 Rollback

- Our payload: exits (bounded lifecycle; module_stop path). Nothing of ours
  is kernel-resident, so payload exit fully removes our footprint.
- Kernel state: UNCHANGED by every L0–L3 activity (we only call existing
  resident services / patch target-process memory with revert receipts).
- Universal rollback: reboot (kstuff has no unload; a fresh boot restores
  the stock resident state).
- Stock `kstuff-lite_v1.11.elf` stays byte-intact on console at all times;
  the P1 sha256 is the integrity check re-verified every session.
- **Pre-proof required before the first L3 (write) test:** FTP (ftpsrv
  :2121) must demonstrably still come up on a crashed boot so recovery is
  possible; if that cannot be established, the L3 test is reported BLOCKED
  [MISS rule]. (L0–L2 need no such pre-proof — they cannot crash the
  kernel if the ladder is honored.)

### 4.6 The 3-restart acceptance law + session rules (mission §DEFERRED PLAN, restated)

- "Done" = 3 console passes across reboots with saved logs. Mac/host tests
  never count.
- One capability per session; written pass/fail check + revert path before
  testing; report status as console-proven / Mac-only / blocked.
- Saves backed up before any kernel test. One user-launched test per boot.
- After any kstuff-adjacent change (in practice: any L2+ pass), regression
  launches — Astro Bot, Stellar Blade, Townfall — as separate user-launched
  tests before continuing.

---

## 5. PREPARED PAYLOAD CODE — `[KB]` probe skeleton TU (DESIGN ONLY — do not add to the tree)

A new composition TU `typescan-r4/candidate/src/KernelBridgeProbes.cpp`,
ueselftest-style ([ST] family): one-shot narrow+wide `[KB]` lines, never
throws, fail-closed admission mirroring the P1 fault-ledger doctrine
(parked/P1-fault-ledger/NOTES.md): unknown/invalid => REFUSE, never
saturate, one-shot loud events, local state only, every gate default-OFF.

```cpp
// KernelBridgeProbes.cpp — Phase B [KB] probes (agent GD design, 2026-10-10).
// DESIGN ONLY: not in the tree. Nothing runs on console without Claude
// review + user OK. Never touches kstuffSharp (quarantined). Read ladder
// first; the write channel is a REFUSE stub until separately authorized.

#include <cstdint>
#include <cstdlib>
#include <cstring>
// Composition-provided (existing doctrine): Output::send narrow+wide,
// diagnostic_event, Unicode::DecodeUtf8 — same includes as ToolchainSelfTest.cpp.

#if defined(__PROSPERO__)
extern "C" void diagnostic_event(const char* tag, unsigned int value);
#endif

// ---- compile gate: default OFF -------------------------------------------
// Build with -DMODBRIDGE_KB_PROBES to compile any code at all. A normal
// PRX build contains zero kekcall-capable bytes from this TU.
#if defined(MODBRIDGE_KB_PROBES) && defined(__PROSPERO__)
#define MODBRIDGE_KB_ENABLED 1
#else
#define MODBRIDGE_KB_ENABLED 0
#endif

#if MODBRIDGE_KB_ENABLED

// ---- kekcall numbers (family interface; lite v1.11 subset MUST be pinned
// ---- from the fetched binary per PHASEB-PLAN §2 P3 before relying on any
// ---- number beyond CHECK):
constexpr uint64_t KB_GATE_SYSNR      = 0x27;                    // SYS_getppid carrier
constexpr uint64_t KB_KEKCALL_CHECK   = 0xffffffff00000027ull;   // residency probe (read-only)
constexpr uint64_t KB_KEKCALL_REMOTE  = 0x0000000500000027ull;   // R3 remote syscall (L2, design stub)

// ---- fail-closed admission state (P1 doctrine: refuse-on-unknown, never
// ---- saturate; one transitions state only via a verified precondition) ----
enum class KbLevel : unsigned { Off=0, L1Check=1, L2Reads=2, L3Writes=3 };
struct KbAdmission {
    bool gate_verified   = false;  // gadget stub signature matched
    bool check_result    = false;  // KEKCALL_CHECK returned nonzero
    unsigned refused     = 0;      // one-shot refusal counter, never poisons
    KbLevel armed_level  = KbLevel::Off;
};
static KbAdmission g_kb;
static bool g_kb_lines_fired[8] = {};   // one-shot [KB] line accounting

// ---- runtime double gate: even when compiled in, nothing runs unless the
// ---- environment explicitly arms it (default OFF both layers) -----------
static KbLevel kb_runtime_armed_level()
{
    const char* s = std::getenv("MODBRIDGE_KB_LEVEL");   // "1"|"2"|"3"; absent => Off
    if (!s || !s[0] || s[1]) return KbLevel::Off;
    switch (s[0]) {
        case '1': return KbLevel::L1Check;
        case '2': return KbLevel::L2Reads;
        case '3': return KbLevel::L3Writes;   // refuses below anyway until authorized
        default:  return KbLevel::Off;
    }
}

// ---- gate discovery + stub-signature verification (fail-closed) --------
// ptr_syscall = dlsym(0x1,"getpid") + off, off pinned from the lite v1.11
// binary's own usage (PHASEB-PLAN §3.0 / P3.4). The stub bytes MUST match
// the pinned signature before ANY call; mismatch => refuse loudly.
extern "C" void* sceKernelDlsym(int64_t handle, const char* name);
static bool kb_gate_discover(uintptr_t* out_gadget)
{
    constexpr uint8_t kStubSig[4] = {0x88,0xd2,0x3f,0xd3}; // PLACEHOLDER: replace
    //   with the pinned signature bytes from the lite binary BEFORE build.
    void* p = sceKernelDlsym(1, "getpid");
    if (!p) return false;
    uintptr_t g = (uintptr_t)p + KB_GADGET_OFFSET;  // KB_GADGET_OFFSET from P3.4 pin
    if (std::memcmp((const void*)g, kStubSig, sizeof kStubSig) != 0) return false;
    *out_gadget = g;
    return true;
}

// ---- the packed dispatch (mirrors the project CRT shim shape) ----------
static uint64_t kb_kekcall(uintptr_t gadget, uint64_t nr,
                           uint64_t a1=0,uint64_t a2=0,uint64_t a3=0,
                           uint64_t a4=0,uint64_t a5=0,uint64_t a6=0)
{
    uint64_t rax = (nr << 32) | KB_GATE_SYSNR;
    register uint64_t x0 __asm("x0") = a1; register uint64_t x1 __asm("x1") = a2;
    register uint64_t x2 __asm("x2") = a3; register uint64_t x3 __asm("x3") = a4;
    register uint64_t x4 __asm("x4") = a5; register uint64_t x5 __asm("x5") = a6;
    register uint64_t x8 __asm("x8") = rax;
    __asm volatile("blr %7" : "+r"(x0) : "r"(x1),"r"(x2),"r"(x3),"r"(x4),
                    "r"(x5),"r"(x8),"r"(gadget) : "memory","cc","x9","x10","x16","x17");
    return x0;
}

// ---- probe entry: called from the runtime's self-test point; one-shot,
// ---- never throws, refuses everything by default ------------------------
extern "C" void RC_kb_probes()
{
    const KbLevel armed = kb_runtime_armed_level();
    // [KB] armed line (index 0): states the compile+runtime gate verdict.
    if (!g_kb_lines_fired[0]) {
        g_kb_lines_fired[0] = true;
        // narrow template + wide one-shot, e.g.:
        //   "[KB] armed=1 level=1 pinned=lite-v1.11-sha<first8>" / refused variant
        ::diagnostic_event("kb_armed", (unsigned)armed);
    }
    if (armed == KbLevel::Off) { ::diagnostic_event("kb_refused_level0", 1); return; }

    uintptr_t gadget = 0;
    if (!kb_gate_discover(&gadget)) {           // fail closed: no verified gate, no calls
        if (!g_kb_lines_fired[1]) { g_kb_lines_fired[1] = true;
            // "[KB] gate refused: stub signature mismatch" (narrow+wide)
            ::diagnostic_event("kb_gate_refused", 1); }
        return;
    }
    g_kb.gate_verified = true;

    // R1: KEKCALL_CHECK — the ONLY kernel-adjacent call at level 1 (read).
    const uint64_t check = kb_kekcall(gadget, KB_KEKCALL_CHECK >> 32 /* packed above */);
    g_kb.check_result = (check != 0);
    if (!g_kb_lines_fired[2]) { g_kb_lines_fired[2] = true;
        // "[KB] check resident=1 ret=0x%llx" or "… resident=0" (refuse path)
        ::diagnostic_event("kb_check", (unsigned)(check != 0)); }
    if (!g_kb.check_result) return;             // not resident => refuse everything else

    // R2 (level>=2): own-pid module info via plain SYS_dynlib_get_info_ex(608)
    //                 — no kernel mediation; log "[KB] modinfo …" one-shot.
    // R3 (level>=2): remote-syscall READ stub — DESIGN ONLY in this skeleton;
    //                 refuses unless the lite ABI pin (P3) is compiled in:
    //                 "[KB] remote refused: lite abi unpinned" (one-shot).

    // L3 (any level): the WRITE channel is a REFUSE stub in this phase:
    if (!g_kb_lines_fired[3]) { g_kb_lines_fired[3] = true;
        // "[KB] patch-channel refused: L3 requires separate authorization"
        ::diagnostic_event("kb_patch_refused", 1); }
}
#endif // MODBRIDGE_KB_ENABLED
// Un-armed builds link an empty TU (mirrors the VmtTickPumpRetire fold shape).
```

Markers this TU will declare (see checklist): `[KB] armed=`, `[KB] gate
refused`, `[KB] check resident=`, `[KB] patch-channel refused` — narrow
utf-8 templates + wide one-shot literals, both encodings declared.

### 5.1 PRX-integration checklist (exact, from the l26 build discipline)

When (and only when) Claude approves building this TU — every step is a
hard gate; the lfb-211254 phantom-build class is what this checklist exists
to prevent:

1. **NEW_UNITS registration** in `outputs/2026-10-08-l26-runtime/
   build_variants.py` (new TUs CANNOT ride CHANGES rows — no BASE unit
   exists): add
   `'kbprobes':{'source':HERE/'typescan-r4/candidate/src/KernelBridgeProbes.cpp','template':'unreal-unrealinitializer','note':'agent GD Phase B [KB] kernel-bridge probes (default-OFF)'}`;
   the driver clones the template unit's argv, rewrites `-c/-o/-MF`, and
   APPENDS the object to the BASE link command (zip-remap pairing
   preserved). Census expected becomes `391+len(NEW_UNITS)`.
2. **Compile define** `-DMODBRIDGE_KB_PROBES` must appear in the unit's
   argv AND in the packaging driver's define list so `verify_build.sh`
   sees the markers; without the define the TU compiles empty and the
   marker gate fails (intended).
3. **CHANGES part-file**: append a scope id (e.g.
   `typescan-r4:scope_kb_kernel_bridge`) to the typescan-r4 CHANGES.json
   changes[] list so it lands in the [BUILD] stamp `changes=` field.
4. **markers-declared.json**: add the four `[KB]` marker ids with exact
   text + encodings (utf-8 for snprintf templates, utf-16le for wide
   STR literals; declare both where both exist) under
   `outputs/2026-10-08-l25-prep/harvest/markers-declared.json`.
5. **Snapshot discipline**: run `harvest/snapshot_tree.sh`, use ITS new
   snapshot id as `BRIDGE_SNAPSHOT_ID` for `--execute` (never reuse
   24025929 for a changed tree).
6. **verify_build.sh --manifest**: N/N markers REQUIRED in the built PRX
   (UTF-8 + UTF-16LE); nonzero exit = do not upload.
7. **Upload discipline** (per LAUNCH-READY ledger pattern): curl -T
   upload; decrypt-OFF readback magic `4f153d1d` AND full sha256
   byte-exact; prior resident PRX backed up first.
8. **Arm discipline**: ploader.ini enabled-state restored (size-exact),
   ONE loadpayload dispatch after confirmed exit of any resident (Z2
   procedure); klog tap on TCP 3232 to a NEW file BEFORE the launch; one
   user-launched test per boot; harvest UE4SS.log + supplier-diag jsonl.

---

## 6. Open gaps (Phase B readiness, honestly stated)

1. **kstuff-lite v1.11 source not on the Mac** — §2 P4 cannot run yet;
   until then every §3 contract is family-level [SRC], not lite-pinned.
2. **Lite kekcall subset + 0xdeb7 ABI unknown** — §2 P3 (binary inventory)
   closes this from the fetched binary even if P4 busts.
3. **Gadget offset duality (+7 vs +0xa)** — resolved by P3.4 pin; the
   skeleton refuses without it (placeholder signature must be replaced).
4. **mdbg write i-cache semantics unknown** — §3.2 pins it before any
   instruction patch; data-page patches first regardless.
5. **FTP-up-after-crash pre-proof** — required before the first L3 test;
   L0–L2 do not need it.
6. **#6 kernel input route** — DECLINED (§3.3); userspace ScePad retained;
   no gap for UE4SS parity.

— agent GD (ZCode), 2026-10-10. Design only; no builds, no console contact,
no tree edits. Nothing herein authorizes any console action.


## GP CORRECTIONS (2026-10-10 — source-verified, kstuff-lite-v1.11-VERIFIED.md)

Six corrections from reading the REAL v1.11 source; supersede conflicting
text above wherever they conflict:
1. CHECK polarity: resident => returns 0; NOT resident => parent pid. The
   [KB] skeleton above reads this BACKWARDS — invert before any use.
2. v1.11 has NO 0xdeb7 sysentvec patch (compiled out); fake sysent tables
   via sv_table pointer swap (explains etaHEN's varying native=/compat=).
3. NO 0xdeb7-extended syscall range exists — the R4 read rung is dead;
   cross-process reads are REMOTE_SYSCALL (sub 5) only.
4. ISA is x86-64, not aarch64: the skeleton's asm sketch and the i-cache
   precondition are void — x86-64 is coherent; upstream pattern is
   write -> verify -> target-side mprotect RX.
5. mdbg_call 573 is hooked-but-passthrough in v1.11 (still callable).
6. Unknown kekcall sub-numbers silently degrade to a real getppid — never
   probe support by return value.

Lite v1.11 exact kekcall table: 0 GETPPID passthrough; 1 READ_DR;
2 WRITE_DR; 3 RDMSR; 4 WRMSR reserved/unimplemented; 5 REMOTE_SYSCALL;
6 OBS-only snapshot (absent in release); 7 PPR plaintext control
(magic "PPRPLAIN"); 0xffffffff CHECK. Input/pad surface: ZERO pad code
(#6 decline stands). FW safety: fail-closed on unknown firmware with
13.60 as a first-class table (contrast: quarantined kstuffSharp's single
10.01 offset set). Rebuild determinism: embedded build path, clang
comment, empty submodules, SDK static libs, web-fetched syscall tables —
byte-identity unlikely; fallback pin = hash of .text + embedded payload.
