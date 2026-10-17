# ACCESSIBILITY-ANALYSIS — Can the game process read /data/modbridge/roots/PPSA33286/?

Worker: worker-activation-accessibility, 2026-10-04. Static analysis of the retained
sources only; no console contact, no execution. Question: the composed runtime reads
`/data/modbridge/roots/PPSA33286/` during startup — what exactly does it open, what
happens per failure mode, and could a file-access restriction explain the observed
zero-output behavior (no UE4SS.log, no runtime-status.txt, no mod output)?

## 1. What the runtime actually opens (exact paths, exact syscalls)

All activation file access is the vendored C component
(`outputs/2026-10-04-zcode-team/worker-f-r2-fix/composition/vendored/modforge_activation.c`,
identical logic in worker-e2e-final / worker-d1-chain-relink / worker-zip-to-mfb copies).
It uses plain POSIX `open`/`read`/`close` through `mfa_io` (`posix_open` :41-48,
`posix_read` :50-61) with `O_RDONLY | O_NOFOLLOW | O_CLOEXEC`. **It never enumerates a
directory** — there is no `opendir`/`getdents` anywhere. Accessibility is therefore
needed only for `open()` on these exact paths (plus traverse permission on each parent
component of `/data → modbridge → roots → PPSA33286`):

| # | path opened | syscall/flags | where | when |
|---|---|---|---|---|
| 1 | `<root>/loadlist.txt` = `/data/modbridge/roots/PPSA33286/loadlist.txt` | `open` RO, then read to EOF | `read_loadlist` :211-279 | once, inside `mfa_activate` during `module_start` |
| 2 | `<root>/<row path>` for **every loadlist row whose role is preloaded** (defaults: `lua_mod`, `config`, `script` — `mfa_default_roles` :281) | `open` RO, read exactly the row's size, then 1 extra byte to confirm EOF (`read_exact` :163-208) | same | same, immediately after loadlist parse |
| 3 | `<root>/runtime-status.txt` | `open` `O_WRONLY\|O_CREAT\|O_TRUNC\|O_NOFOLLOW\|O_CLOEXEC` 0644 | `mfa_status_open` :599-609 (via `RuntimeAckSink::Open`, `runtime_owner.cpp:130-153`) | during `module_start`, ONLY AFTER the activation was consumed (`ue4ss_backend.cpp:100-102`) |
| 4 | `<working directory>/UE4SS.log` = `/data/plugins/UE4SS.log` | log device create/write | `UE4SSProgram.cpp:255-257` (per worker-post-promotion-runtime trace) | during UE4SS backend construction — never reached if activation failed |

Rows with non-preloaded roles (`native_plugin`, paks) are **listed from the loadlist
text only and never opened** at activation time. So a hypothetical read restriction on,
say, a future `dlls/` subtree would not surface during activation at all.

Timing constraint (measured contract, `modforge_activation.h:4-6`): the runtime reads
the root **once, during `module_start`** — "that is the only moment /data is readable
from the game process (a fresh open afterwards fails with ENOENT)". Everything above
must succeed inside that window; the preloaded bytes are then owned memory for the
process lifetime, and only the still-open `runtime-status.txt` descriptor keeps working
later.

Note for the current staging: `loadlist.txt` is the installer's byte-truth. Files placed
by FTP must match the loadlist row sizes exactly (`779 / 588 / 186 / 2960 / 21415 / 0 /
9962` in the current plan); mfa is all-or-nothing on any mismatch. If the root was
populated by hand without the installer-generated loadlist, failure is the *expected*
result (see §2), not evidence of a sandbox problem. `O_NOFOLLOW` also means any
symlinked staging path fails as ELOOP → `MFA_E_OPEN`.

## 2. Failure modes per open, and their error identity

`mfa_activate` (:311+) is strictly all-or-nothing: any failure releases the whole
activation and returns one `mfa_error {code, os_error, line, path, message}` so "the
game runs unmodded rather than half-modded" (`modforge_activation.h` contract).

| observable on the console | errno class | mfa result | message in `err` |
|---|---|---|---|
| root path absent / empty dir | `ENOENT` | `MFA_E_NO_LOADLIST` (-2) "no-loadlist" | "no loadlist.txt (nothing installed)" (:221-223) |
| parent dir traversal denied, sandbox EPERM/EACCES, symlink (ELOOP), not a dir (ENOTDIR) | `EACCES`/`EPERM`/`ELOOP`/`ENOTDIR` | `MFA_E_OPEN` (-6) "open" | "cannot open loadlist.txt" (:224) — **os_error preserved, so a denial is distinguishable in memory from "nothing installed"** |
| listed file present but unreadable (same denial classes) | as above | `MFA_E_OPEN` (-6) | "cannot open listed file" (:170-174) |
| read() fails mid-file (I/O error, revoked mount) | `EIO`… | `MFA_E_READ` (-7) | "read failed" |
| real bytes shorter / longer than the row says | — | `MFA_E_SIZE` (-8) | "file is shorter/longer than the load list says" (:186, :203) |
| loadlist grammar/size-limit/title problems | — | `MFA_E_LOADLIST`/`MFA_E_LIMIT`/`MFA_E_TITLE` | per `modforge_activation.h:113-134` |

Critical visibility fact: **`mfa_activate` reports its error ONLY through the returned
`mfa_error` struct.** It writes nothing to any log, emits no diagnostic event, and the
ack sink does not exist yet (row 3 opens only after success). So the *file system
layer's* verdict on accessibility is invisible unless the caller records it.

## 3. Who sees the error, and what each zero-output artifact means

The activation call chain during `module_start` is:
`module_start` (`lifecycle/module_entry.c:48`, diagnostics via `diagnostic_event` →
`/data/townfall_supplier_composition.log`) → game-bound caller unit (owns the
`mfa_config` with `root = /data/modbridge/roots/PPSA33286`, calls `mfa_activate`) →
`StartActivatedRuntimeWithAck` → `PrepareActivatedUE4SSRuntime`
(`bootstrap/ue4ss_backend.cpp:84-123`) → `MakeActivatedUE4SSModSource`
(`bootstrap/activated_mod_source.cpp:21-84`) → `UE4SSProgram`.

Per-failure visibility:

1. **Activation fails (unreadable root, missing/mismatched files):** no ack file at all
   (`ue4ss_backend.cpp:100-102` opens the sink only after the activation is consumed —
   recorded verbatim in worker-post-promotion-runtime/FAILURE-TRIAGE.md T6), no
   `UE4SS.log` (UE4SSProgram never constructed), no mod prints. The only places the
   error can appear are (a) the caller unit's own handling and (b) the module
   diagnostic log **if** the caller throws from a constructor
   (`module_entry.c:19-26` records `constructor_return` / `PROBE_CTOR_EXCEPTION`) or
   explicitly forwards `err.message`/`err.os_error` to `diagnostic_event`. The vendored
   design default is *silent degradation to unmodded* — a genuine sandbox denial would
   therefore be **near-silent**: `runtime-status.txt` absent is the symptom, not a
   record of the error.
2. **Activation succeeds but root not writable:** `mfa_status_open` fails → the ack
   sink is deliberately *inert* (`runtime_owner.cpp:141-146` records
   "mfa_status_open failed: N", startup is never blocked). Session runs unacknowledged:
   `UE4SS.log` + mods appear, `runtime-status.txt` never does.
3. **`/data/plugins` not writable:** `UE4SS.log` open fails (T5.1 in FAILURE-TRIAGE);
   module diagnostics healthy, activation fine, but the runtime log is absent.
4. **Wiring state (measured, currently the proven explanation):** the *game-bound
   caller unit that calls `mfa_activate` with the root string is controller-owned
   wiring that is not yet in the linked candidates*. Composition-3 as linked defines
   `PrepareActivatedUE4SSRuntime` but NOT `MakeRuntimeAckConfig`/
   `StartActivatedRuntimeWithAck` (worker-post-promotion-runtime REPORT §4.2, from the
   nm evidence of the staged candidate); composition-4 added the supplier unit
   (`worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp`), but
   WIRING-SPEC §S5.5 and INTEGRATION-CHECKLIST STEP 6 both state the caller that
   invokes `mfa_activate` + the supplier is "authored with the next game-bound module's
   caller unit". In that state the PRX **never opens /data/modbridge at all** —
   healthy module log, no ack file, no UE4SS.log. FAILURE-TRIAGE T5.5 explicitly warns
   not to misread this as T1/T6.

## 4. Verdict: could a file-access issue explain the zero-output behavior?

**Yes — it is fully consistent, and it would be near-silent by design.** If the game
process cannot read `/data/modbridge/roots/PPSA33286/`:

- `open("…/loadlist.txt")` fails with `ENOENT` (→ `MFA_E_NO_LOADLIST`) or
  `EACCES`/`EPERM` (→ `MFA_E_OPEN`); activation aborts all-or-nothing;
- the ack sink never opens (no `runtime-status.txt`), UE4SS is never constructed (no
  `UE4SS.log`, no Lua prints), and the only in-memory distinguisher —
  `err.os_error == EACCES/EPERM` vs `ENOENT` — dies with the call unless the
  game-bound caller unit records it into `/data/townfall_supplier_composition.log` or
  throws.

**But it is not the only zero-output explanation, and today's static evidence points
one step upstream first:** the staged candidates demonstrably lack (composition-3) or
do not yet prove (composition-4) the game-bound caller that performs `mfa_activate`
during `module_start`. A PRX that never attempts the read and a PRX whose read is
denied produce **identical observable output: nothing**. Also note FTP readability
proves nothing about the game process — `ftpsrv` runs outside the game sandbox; "the
files ARE on disk" does not answer the accessibility question.

Read-only distinguishers, in order (all from FAILURE-TRIAGE.md):

1. Freshness + content of `/data/townfall_supplier_composition.log` for THIS launch:
   constructor event trail reaching the activation-construction index (T2) vs absent
   module log (T1) vs healthy-but-shallow log with no activation attempt (wiring
   state, T5.5).
2. Symbol table of the actually staged PRX (RETR + sha `b8bfae77…` composition-3 /
   `de372302…` composition-4): does it define `StartActivatedRuntimeWithAck`, and does
   any unit reference `mfa_activate`? Absent ⇒ the root was never read; file-access
   analysis is moot for that candidate.
3. If the caller is proven wired: absence of `runtime-status.txt` while the module log
   proves the activation constructor ran is the specific signature of an activation
   failure — and with FTP-verified on-disk bytes matching the loadlist, a sandbox
   read denial is the remaining explanation (errno would be EACCES/EPERM, not ENOENT).
4. Row-size audit via FTP RETR (779/588/186/2960/21415/0/9962 vs loadlist rows): a
   mismatch predicts `MFA_E_SIZE`, an FTP-visible loadlist absence predicts
   `MFA_E_NO_LOADLIST` — both explain zero output without any sandbox involvement.

Recovery discipline stands (FAILURE-TRIAGE T6/T8): fix roots only through the
installer verbs (register → prepare → install), never hand-edit `/data/modbridge/**`;
never re-rate unchanged evidence; one launch per staged build.
