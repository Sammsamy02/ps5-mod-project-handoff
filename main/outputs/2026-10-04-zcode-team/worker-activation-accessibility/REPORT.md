# REPORT — worker-activation-accessibility

Date 2026-10-04. Static source analysis only; no console, no network, no nested
subagents; writes confined to this directory. ~20 tool calls, under the 30-call bound.
CLAIM.json written first; this file last.

## Deliverable

`ACCESSIBILITY-ANALYSIS.md` (this dir): exact paths the composed runtime opens, the
failure mode + error identity for each, per-failure visibility, and the verdict on
whether file-access restriction explains the zero-output behavior.

## Task answers

1. **mfa paths and behavior** (vendored `modforge_activation.c`, f-r2-fix copy; same in
   e2e-final/d1-chain-relink/zip-to-mfb): POSIX `open(O_RDONLY|O_NOFOLLOW|O_CLOEXEC)` +
   `read` only — no directory enumeration. Opens `<root>/loadlist.txt`
   (:211-279), then every preloaded-role row (`lua_mod`/`config`/`script`, :281) via
   `read_exact` (:163-208) with strict size + EOF truth; non-preloaded roles are listed,
   never opened. All-or-nothing; error returned ONLY via the `mfa_error` struct
   (code + os_error + path + message); nothing is logged by mfa itself.
2. **Discovery chain** (`bootstrap/activated_mod_source.cpp:21-84`): consumes the
   activation in memory (title/engine/framework gates, forwards `ue4ss/*` preloaded
   files as logical keys); the `activation_root` is used only to name native dll
   candidates (none staged). `NativeModSource::Prepare` takes those bytes; the
   filesystem is never consulted again by the mod source.
3. **Bootstrap call site** (`bootstrap/ue4ss_backend.cpp:84-123` +
   `runtime_ack_supplier.cpp:68-95`): `mfa_activate` runs once during `module_start`
   (the only moment /data is readable — `modforge_activation.h:4-6`); the ack sink
   (`<root>/runtime-status.txt`, `mfa_status_open` :599-609) opens only AFTER the
   activation is consumed; `UE4SS.log` opens at `/data/plugins/` only when UE4SS is
   constructed.
4. **Failure visibility:** an unreadable `/data/modbridge/roots/PPSA33286/` yields
   `MFA_E_NO_LOADLIST` (-2, ENOENT) or `MFA_E_OPEN` (-6, EACCES/EPERM/ELOOP — os_error
   preserved), aborts all-or-nothing, and produces **no log artifact of its own**: no
   runtime-status.txt (sink not yet open), no UE4SS.log, no mod output. It is a
   near-silent failure unless the game-bound caller records the error or throws from a
   constructor (then `/data/townfall_supplier_composition.log` shows
   `constructor_return`/`PROBE_CTOR_EXCEPTION`, `module_entry.c:19-26`).
5. **Verdict:** file-access restriction is fully consistent with zero output, BUT it is
   not distinguishable from the measured upstream wiring state: composition-3 as linked
   lacks the ack supplier entirely, and per WIRING-SPEC §S5.5 / INTEGRATION-CHECKLIST
   STEP 6 the game-bound caller unit that actually invokes `mfa_activate` is still
   controller-owned work — a PRX that never opens the root and a PRX whose open is
   denied look identical from outside. The analysis gives a 4-step read-only
   distinguisher (module-log constructor trail → staged-PRX symbol table →
   ack-absent-with-activation-constructed signature → FTP row-size audit) and repeats
   the triage rule that FTP visibility ≠ game-process visibility, and roots/ files must
   be installer-committed (loadlist byte truth), never hand-placed.

## Key measured anchors

- `outputs/2026-10-04-zcode-team/worker-f-r2-fix/composition/vendored/modforge_activation.c`
  (:41-75 posix io, :211-279 loadlist, :163-208 read_exact, :599-609 mfa_status_open)
  and `modforge_activation.h:4-6` (module_start-only /data readability contract).
- `outputs/ps5-mod-bridge/native-runtime-startup/bootstrap/{activated_mod_source,ue4ss_backend,runtime_owner}.cpp`
  and `lifecycle/module_entry.c:48`.
- `outputs/2026-10-04-zcode-team/worker-post-promotion-runtime/{RUNTIME-STAGING-PLAN,FAILURE-TRIAGE}.md`
  (layout, MFA_E_* table, T5.5 wiring state, §2 exact console paths).
- `outputs/2026-10-04-zcode-team/worker-ack-integration-deep/controller-patch/02-composition-wiring/WIRING-SPEC.md`
  §S1-S5 and `worker-integration-checklist/INTEGRATION-CHECKLIST.md` STEP 6
  (game-bound caller unit not yet authored/composed).

## Limits

No console evidence was collected or claimed; everything is static reading of retained
sources and prior workers' measured reports. Whether the game process sandbox actually
denies `/data/modbridge` is undecidable from here — the analysis states exactly which
launch-time artifact would decide it.
