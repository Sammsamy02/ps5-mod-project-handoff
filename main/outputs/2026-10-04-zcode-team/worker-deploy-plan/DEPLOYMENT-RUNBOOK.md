# DEPLOYMENT-RUNBOOK — PS5 UE4SS runtime port: Mac builds → 8 capabilities live → Item Spawner menu

Worker: `worker-deploy-plan` · 2026-10-06 · **Planning only.** This worker made
no console contact, executed nothing, and staged nothing. Every command below is
drafted from retained sibling-worker artifacts and pinned evidence in this
worktree; every "expected" line is a **prediction** until a real launch observes
it. Nothing in this runbook certifies any phase; it defines the ordered
procedure, the evidence that would prove each phase, and the rollback when a
phase fails.

Execution ownership: **the controller (single session) performs every console
operation**; workers never touch the console (CONSOLE-SAFETY-PROCEDURE §0.3).
The user owns power, reboot, sign-in, and every game launch. Each launch in
this runbook requires its own explicit user authorization (standing gate
`game_file_mutations_permitted_now=false`).

---

## 0. Scope, endpoints, and the standing laws

### 0.1 Console facts (all retained evidence; BOOT-PROTOCOL §1, ROLLBACK-RUNBOOK §0)

| Item | Value |
|---|---|
| Console | ps5-host-not-configured.invalid (PS5, fw 13.600.007 family, etaHEN Beta 2.5 / 13.60 lineage) |
| Payload manager (pldmgr) | `http://ps5-host-not-configured.invalid:8084` — `/list_payloads`, `/manage:check`, `/manage:upload`, `/loadpayload:<path>` |
| FTP (ftpsrv) | `ftp://ps5-host-not-configured.invalid:2121` anonymous — STOR/RETR/DELE/RNFR/RNTO/MDTM; QUOTE commands (`MKD` unverified) |
| Game | Silent Hill: The Townfall, **PPSA33286**, UE 5.6, boots from the decrypted dump at `/data/homebrew/Silent Hill Townfall PPSA33286/` (shadow-mounted as `/app0` per launch by `shadowmountplus.elf` in pldmgr autoload) |
| pldmgr autoload | `/data/pldmgr/autoload.txt` (165 B, one payload name per line; `AUTOLOAD_ENABLED=1`, `AUTOLOAD_DELAY=5`); payload ELFs live at `/data/pldmgr/payloads/<name>/<name>.elf` |
| Kernel payload today | kstuff-family resident each boot (`kstuff state=1 … crypt=deb7/deb7` in etaHEN bootstrap log); kernel-side addresses are **per-boot**; there is **no in-tree unload path** — extension = rebuild + redeploy on next boot |
| ploader | `/data/pldmgr/payloads/ploader/ploader.elf`; config `/data/plugins/ploader.ini` (9-entry layout, see §3.3); injection ritual = arm ploader → user launches Townfall → collect logs ~2 min later |
| Evidence sinks (console) | S `/data/modbridge/roots/PPSA33286/runtime-status.txt` · L `/data/plugins/UE4SS.log` · D1 `/data/townfall_runtime_aligned_entry.log` · D2 observer log · C2 `/data/modbridge-ingame-facts.jsonl` · X `/data/modbridge-exec-write-probe.jsonl` · F `/data/modbridge-fc1pool-facts.jsonl` · CC `/data/modbridge-carrier-facts.jsonl` (grammars: MASTER-VERIFICATION-RUNBOOK §1) |
| Host workspace | `$W` = `/WORKSPACE/.codex/worktrees/translation-core/what`; team outputs `$T/` = `$W/outputs/2026-10-04-zcode-team/` |

Shell prefix for every command block (paths with spaces use `%20` in curl URLs):

```sh
HTTP=http://ps5-host-not-configured.invalid:8084
FTP=ftp://ps5-host-not-configured.invalid:2121
W=/WORKSPACE/.codex/worktrees/translation-core/what
T=$W/outputs/2026-10-04-zcode-team
GAME_RAW='/data/homebrew/Silent Hill Townfall PPSA33286'   # console path (raw)
GAME_URL='/data/homebrew/Silent%20Hill%20Townfall%20PPSA33286'  # console path (curl-quoted)
ROOTS=/data/modbridge/roots/PPSA33286
OVL=/data/modbridge/overlay/PPSA33286
PLUGINS=/data/plugins
SLOT=$PLUGINS/modbridge_supplier_composition.prx
CURL="curl -s --connect-timeout 10 --max-time 60"          # bounded fetches (ge.2 law)
CURL_BIG="curl -s --connect-timeout 10 --max-time 300"     # 194 MB class transfers
EVI=$T/worker-deploy-plan/evidence                          # local evidence copies (create per run)
```

### 0.2 The 8 PC-parity capabilities (assumed contract — RECONCILE before execution)

The sibling `worker-kernel-deploy` is still building the extended kernel module.
The capability enumeration below is this runbook's **assumed contract**, derived
from the kekcall arms documented across `worker-kernel-build` BUILD-GUIDE §4,
`worker-kernel-saferead` `safe_read_kernel.c`, and `worker-kernel-input`
INPUT-SERVICE §3.5 (resident arms today: ping −1, nr=1, nr=2, nr=3, nr=5,
nr=6; new arms nr=7/8/9). **Before Phase 1, reconcile this table against
worker-kernel-deploy's delivery manifest; if their enumeration differs, their
manifest wins and only the probe list below needs re-mapping.**

| # | Capability | Kernel surface | PC-parity feature it restores | Consumer |
|---|---|---|---|---|
| K1 | Hardware debug-register read | kekcall nr=1 | HW breakpoint state inspection | gdb stub, UE4SS |
| K2 | Hardware debug-register write | kekcall nr=2 | HW breakpoints | gdb stub |
| K3 | MSR read | kekcall nr=3 | Low-level CPU state | gdb stub, diagnostics |
| K4 | Remote syscall in target process | kekcall nr=5 | Cross-process syscall execution (VirtualAlloc/Protect class ops) | runtime exec-write path |
| K5 | Kernel snapshot | kekcall nr=6 | Kernel-state snapshot (constant exists; retail returns 0) | diagnostics |
| K6 | **Safe cross-process memory read** | kekcall nr=7 `safe_read` | ReadProcessMemory | shadow scanner, gdb stub, UE4SS layouts |
| K7 | **Safe memory probe/validate** | kekcall nr=8 `safe_probe` | VirtualQuery-class validation | scanner guards, layout gates |
| K8 | **Shared input state, any process** | kekcall nr=9 `get_input_state` | Any-process pad/keyboard read (no libScePad handle, no ALREADY_OPENED) | KekcallPadSource, companion tools |

Liveness gate (not counted as a capability): kekcall nr=−1 **ping** must return
0. An old resident module answers unknown nr with **ENOSYS 78** — that value is
the decisive "new module NOT resident" discriminator throughout Phase 1.

Ecosystem pieces that ride on these arms: shadow scanner (uses K6/K7 instead of
exec-text read workarounds), GDB stub (RSP server in a payload process over
kernel I/O), file overlay (container projection + optional kernel-assisted
serving), asset converter (host-side translation feeding the overlay).

### 0.3 Standing laws (binding on every step below; sources: CONSOLE-SAFETY-PROCEDURE, ROLLBACK-RUNBOOK §1/§7)

1. **One console actor** (the controller); one dispatch per explicit
   authorization; **≥30 s between any two dispatches**; a dispatch timeout is a
   crash signal — no retry, go to crash recovery.
2. **Records are the only truth.** ploader OK / HTTP 200 never means "the
   module ran"; judge only from the evidence sinks.
3. **SHT-strip readback law.** ftpsrv RETR of an fself `.prx` returns a
   decrypted+stripped *view* (size/sha WILL differ from the upload; PASS =
   PT_LOAD-segment equality vs the payload-of-record inner ELF, only the
   documented artifact classes may differ). Plain files (`.ini`, `.log`,
   `.txt`, `.jsonl`, plain-ELF `eboot.bin`) read back **byte-exact** — whole-file
   sha256 is the verification. Never verify an fself by whole-file sha; never
   re-pin a receipt to match a readback.
4. **Fresh-connection readbacks**: after any STOR wait 15–30 s, then RETR on a
   NEW curl session. Never trust the upload connection's echo.
5. **One variable per boot.** Each launch changes exactly one staged thing; a
   failed/reserved layout is never replayed; a re-rating needs NEW evidence with
   NEW ids.
6. **Game CLOSED for all file operations** on game/plugin paths; ini edits take
   effect only at the next ploader arm.
7. **No `/tmp` sinks** (console or host); host evidence lives under `outputs/…`.
8. **Pre-image backups**: before overwriting any owned console file, RETR the
   current bytes, STOR an on-console `.bak`, and record both hashes in the log.
9. **Crash discipline**: on refused ports / ping loss / timeout — STOP all
   contact; the user power-cycles; first contact after recovery is read-only
   (`/list_payloads` → FTP read-only → `/manage:check`); /data has survived
   every power loss to date (twice-proven).
10. **Never re-derive addresses/offsets at runtime**; report and reconcile with
    the director. No inline raw `syscall` sites in any module (prohibited class).

### 0.4 Phase map (each phase gated on the previous one's success criteria §7)

```
Phase 0  pre-flight: inventory, reachability, backups, receipts            (no launch)
Phase 1  kernel module: upload extended installer ELF → autoload swap →
         user cold boot → boot-time install → kekcall probe ladder         (1 reboot, 1 probe launch)
Phase 2  early loading: patched eboot (A1) + mb.prx → loader-order proof   (≤4 staged boots)
Phase 3  UE4SS runtime candidate with kernel integration                   (1–2 launches)
Phase 4  Item Spawner: mods staged → OPTIONS → menu → CROSS → spawn        (1 launch)
Phase 5  ecosystem: file overlay + gdb attach + asset conversion           (1 launch each)
```

---

## 1. Phase 0 — Pre-flight (controller, game closed, no launch)

### 0a. Local build inventory (pin hashes BEFORE anything touches the console)

Fill this receipt table from each sibling's delivery manifest at execution
time; a missing or mismatched artifact is a STOP, never "stage it anyway".

| Artifact | Local source (owner) | sha256 / identity | Console destination |
|---|---|---|---|
| Extended kernel installer ELF (`SampleApp.elf` class, embeds new kelf/uelf with nr=7/8/9) | `$T/worker-kernel-deploy/…` (build per `$T/worker-kernel-build/BUILD-GUIDE.md` §3; kelf/uelf sizes MUST have changed vs 1432/33320 B — §4.5) | `<pin from worker-kernel-deploy receipt>` | `/data/pldmgr/payloads/kstuff-ext/kstuff-ext.elf` |
| Kernel probe payload (kekcall ladder: ping, nr=7/8/9 self-tests, nr=1/2/3, nr=5) | `$T/worker-kernel-deploy/…` (pad-probe lineage, JSONL sink) | `<pin>` | `/data/pldmgr/payloads/kstuff-ext/kprobe.elf` (or a dispatch-once path) |
| GDB stub payload (RSP server, `-a` bind, port 2345) | `$T/worker-kernel-debugger/` (`ps5_backend.c` + `kio_shim.h` composed payload) | `<pin>` | `/data/pldmgr/payloads/modbridge-gdbstub/gdbstub.elf` |
| Patched eboot **A1** (10 bytes changed, base `afe0a2a5…`) | `$T/worker-early-load/eboot-earlyload-A1.bin` — sha `3374b5ab18f4c5feb58b07a951d845e6462e40dcf7d08bc9b35f6f612b411960` (**present locally, hash re-verified this session**) | see left | `$GAME_RAW/eboot.bin` (replaces) |
| Patched eboot **A2** (31 bytes, fallback ladder) | `$T/worker-early-load/eboot-earlyload-A2.bin` — sha `cbb4ee4dd2766e649f597490cf77ea0efa69fc90cc463c7415606b4993255e34` (**present locally, hash re-verified this session**) | see left | same, only on ladder step |
| `mb.prx` fake-self (primary) + plain `0xfe18` ELF (A/B) | `$T/worker-eboot-patch/…` (EARLY-LOAD-DESIGN §4.4; the fake-self wrapper is rank 1 — the only proven-good user-module load class on this console) | `<pin both; record upload sha AND expected view-readback sha>` | `$GAME_RAW/prx/mb.prx` + copy at `$GAME_RAW/sce_module/mb.prx` |
| UE4SS runtime candidate PRX (kernel-integrated) | `$T/worker-ue4ss-kernel/…` (composed fself; import audit per MAC-SESSION-BRIEF §4 — expect 197 rows incl. `scePadGetHandle` for the native fallback) | `<pin upload sha + expected readback oracle>` | `$SLOT` (route A) and/or `$GAME_RAW/prx/mb.prx` (route B) |
| Item Spawner mod tree | `$T/worker-itemspawner-bundle/main.lua` (canonical 21,415 B, sha `f58604…9658`), `UEHelpers.lua` (sha `5a8f66…15a7`), `enabled.txt` | see left | `$ROOTS/…` loadlist rows + `$PLUGINS/ue4ss/Mods/TownfallItemSpawner/…` |
| `PS5Pad.ini` (6 chords) | `$T/worker-real-input-pad/PS5Pad.ini` — 2960 B, sha `1d919fbf…` (`OPTIONS = F7`, CROSS=RETURN, D-pad arrows) | see left | `$PLUGINS/ue4ss/PS5Pad.ini` |
| Known-good rollback PRX (composition-5) | `$T/worker-composition-5-rebuild/prx-candidate-composition5-2/modbridge_supplier_composition.prx` — upload sha `04869ab3…` (30,786,775 B); readback oracle `$W/outputs/2026-10-04-zcode-controller/console/composition4-console-backup.prx` is the **composition-4** oracle `d4fc0acf…` (30,769,672 B) | see left | `$SLOT` (restore only) |

Sanity checks on the two eboot variants (host, before any upload):

```sh
shasum -a 256 $T/worker-early-load/eboot-earlyload-A1.bin $T/worker-early-load/eboot-earlyload-A2.bin
# expect 3374b5ab…b11960 (A1, 194,241,424 B) and cbb4ee4d…25e34 (A2, same size)
file $T/worker-early-load/eboot-earlyload-A1.bin   # ELF 64-bit LSB ... x86-64
```

### 0b. Console reachability + state (read-only)

```sh
curl -s "$HTTP/list_payloads" | head -c 400   # 200 + JSON catalog; else STOP (crash-recovery §e)
$CURL "$FTP/data/pldmgr/autoload.txt"          # current autoload (expect 9 lines, kstuff-lite_v1.11.elf first)
$CURL "$FTP/data/plugins/ploader.ini"          # 9-entry layout (see §3.3)
$CURL "$FTP$GAME_URL/eboot.bin" -o /dev/null -w '%{size_download}\n'  # presence probe (full RETR in Phase 2 pre-flight)
```

### 0c. Backups (pre-image law §0.3.8 — game closed)

```sh
TAG=$(date +%Y%m%d-%H%M%S); mkdir -p "$EVI/preflight-$TAG"
# autoload pre-image + on-console backup
$CURL "$FTP/data/pldmgr/autoload.txt" -o "$EVI/preflight-$TAG/autoload.pre"
$CURL -T "$EVI/preflight-$TAG/autoload.pre" "$FTP/data/pldmgr/autoload.txt.$TAG.bak"
# ploader.ini pre-image + on-console backup (same law as ROLLBACK-RUNBOOK §4.4)
$CURL "$FTP/data/plugins/ploader.ini" -o "$EVI/preflight-$TAG/ploader.ini.pre"
$CURL -T "$EVI/preflight-$TAG/ploader.ini.pre" "$FTP/data/plugins/ploader.ini.$TAG.bak"
# paks-dir census (overlay rollback oracle for Phase 5)
$CURL "$FTP$GAME_URL/townfall/content/paks/" > "$EVI/preflight-$TAG/paks-listing.txt"
shasum -a 256 "$EVI/preflight-$TAG"/*
```

**Phase 0 exit**: all receipts pinned; reachability 200; backups hashed and
logged; user authorization obtained for the Phase 1 reboot.

---

## 2. Phase 1 — Kernel module deployment

Goal: the extended kstuff kernel module (kekcall nr=7/8/9 arms) resident, ping
probe green, all 8 capability probes returning live records.

Mechanism facts (worker-kernel-saferead `safe_read_kernel.c` feasibility notes;
BUILD-GUIDE §4.5): the kelf is installed by the loader payload at **boot**; the
installer's `KEKCALL_CHECK(-1)` already-loaded gate **refuses to overwrite a
live module** (re-patching IDT/TSS/sysent mid-flight panics); there is **no
uninstall kekcall**. Therefore deployment = autoload swap + **cold boot**; the
new module must be the first kstuff-family installer to run in that boot.

### 1a. Upload the installer ELF (ONCE)

```sh
# controller-owned upload through the manager (catalog gains the entry):
curl -s -F "file=@$T/worker-kernel-deploy/kstuff-ext.elf" "$HTTP/manage:upload"
#   (fallback if the manager upload is not used: FTP STOR)
#   $CURL_BIG -T "$T/worker-kernel-deploy/kstuff-ext.elf" "$FTP/data/pldmgr/payloads/kstuff-ext/kstuff-ext.elf"
sleep 20   # readback law: fresh connection after 15–30 s
# readback to a HOST evidence path (never /tmp — law §0.3.7):
$CURL_BIG "$FTP/data/pldmgr/payloads/kstuff-ext/kstuff-ext.elf" -o "$EVI/p1-kstuff-ext-readback.elf"
shasum -a 256 "$EVI/p1-kstuff-ext-readback.elf"     # must equal the pinned upload sha (payload ELF = plain file law)
file "$EVI/p1-kstuff-ext-readback.elf"              # ELF 64-bit LSB pie executable, x86-64
```

If the readback hash differs from THIS attempt's receipt: STOP (channel
ambiguity, CONSOLE-SAFETY §d.3). Never re-pin.

### 1b. Swap the autoload entry (single-line diff discipline)

Autoload order matters: the extended installer must run **before** any other
kstuff-family payload in that boot (the first installer to run wins the
already-loaded gate). Current line 1 is `kstuff-lite_v1.11.elf`; replace exactly
that line, keep everything else byte-identical:

```sh
sed '1s/^kstuff-lite_v1\.11\.elf$/kstuff-ext\/kstuff-ext.elf/' \
    "$EVI/preflight-$TAG/autoload.pre" > "$EVI/p1-autoload.new"
diff "$EVI/preflight-$TAG/autoload.pre" "$EVI/p1-autoload.new"   # exactly ONE changed line
$CURL -T "$EVI/p1-autoload.new" "$FTP/data/pldmgr/autoload.txt"
$CURL "$FTP/data/pldmgr/autoload.txt" | diff - "$EVI/p1-autoload.new" && echo AUTOLOAD-SWAPPED-OK
```

Notes: keep `kstuff-lite_v1.11.elf` on disk (never delete — it is the rollback
image). If worker-kernel-deploy's manifest instead ships a dispatch-once route
(send with `payload --send` to elfldr and reboot), follow the manifest; the
autoload-swap route here is the one supported by the retained pldmgr evidence
(BOOT-PROTOCOL §1.1) and avoids any second-dispatch risk class.

### 1c. Reboot (USER action)

Ask the user for a **cold boot** (full restart from the power menu — not rest
mode). Rationale: kernel state is per-boot (BOOT-PROTOCOL §1.3); a fresh boot is
the only state in which the installer's gate finds no resident module. pldmgr
autoloads after `AUTOLOAD_DELAY=5`; ftpsrv/pldmgr answering again (~1–2 min) is
the recovery signal. **Nobody dispatches anything until the user confirms the
console is back.**

Post-reboot read-only check:

```sh
curl -s "$HTTP/list_payloads" | head -c 200    # 200 = manager back
$CURL "$FTP/data/pldmgr/autoload.txt"          # unchanged since 1b
$CURL "$FTP/data/etaHEN/startup-1360-port.log" | tail -5   # HEN bootstrap generations (best-effort
                                                              # kernel-state line; OnionHEN is the live HEN —
                                                              # its log name may differ; record what exists)
```

### 1d. Verify the module loaded — the kekcall probe ladder

Run the kernel probe payload ONCE (single dispatch, own authorization, ≥30 s
from any other dispatch):

```sh
curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/kstuff-ext/kprobe.elf"   # ONE dispatch only
sleep 35   # ≥ observed dispatch bound before the first FTP read; no polling loop
$CURL "$FTP/data/kstuff-ext-probe.jsonl" -o "$EVI/p1-kprobe.jsonl"        # sink per manifest
```

Required records (each line strict JSON <1 KiB, probe-lineage envelope):

| Step | Call | PASS evidence | FAIL meaning |
|---|---|---|---|
| 1 | `kekcall(-1)` ping | `rc=0` | module not resident (or gate dead) — module problem, do not proceed |
| 2 | nr=7 `safe_read` self-test (own pid, own mapped page) | bytes read == known pattern, `rc>=0` | `rc=78` (ENOSYS) ⇒ OLD module still resident (autoload order / boot not cold) |
| 3 | nr=8 `safe_probe` known-good + known-bad address | good⇒valid, bad⇒refused | arm present but page-walk wrong — STOP, reconcile |
| 4 | nr=9 input read port 0 during a 30 s press routine | `buttons` bits track physical presses; `kinput_torn` low/0 | `-19 NOSTATE` = offsets not derived for this FW (clean refusal, INPUT-SERVICE §3.3) — K8 degraded, everything else may proceed |
| 5 | nr=1/2 debug-reg roundtrip; nr=3 rdmsr (per manifest) | written==read back; msr value sane | HW-breakpoint class degraded |
| 6 | nr=5 remote syscall (target pid, benign e.g. `getpid`) | correct pid echoed | K4 degraded |

Honest gate: steps 1–3 green are **mandatory** (K6/K7 are the foundation the
scanner, stub, and runtime integration all consume). Steps 4–6 may degrade
cleanly (record the refusal codes) — the phase is then "live with N/8
capabilities", recorded as such, never silently upgraded later.

### 1e. Rollback (Phase 1)

No runtime unload exists; rollback is boot-scoped:

1. Restore the autoload pre-image: `$CURL -T "$EVI/preflight-$TAG/autoload.pre" "$FTP/data/pldmgr/autoload.txt"` + fresh readback diff.
2. User cold-boots again. `kstuff-lite_v1.11.elf` reinstalls at boot (it was never deleted).
3. Verify with the probe: ping `rc=0` AND nr=7 `rc=78` ⇒ old module restored (that ENOSYS is the *expected* rollback signature — new arms gone, base arms alive).
4. If the console crashed at any point: STOP all contact, crash-recovery ritual (CONSOLE-SAFETY §e), reconcile before anything else. A kernel-class crash during install is evidence about the module — capture autoload state, HEN logs, and do NOT re-run the installer without a new reviewed revision.

---

## 3. Phase 2 — Early loading deployment (patched eboot + mb.prx)

Goal: prove the framework's `module_start` runs in the kernel dynlib phase —
**before relocations, `DT_INIT` (UE5 static-init walker), `e_entry`, and all
engine code** (EARLY-LOAD-DESIGN §2.4) — replacing the measured ~70.7–71.8 s
ploader gap with ≈T+0 placement.

Design facts pinned by `worker-early-load` (all byte-verified offline):
variant **A1** recycles `dynamic[236]` `DT_DEBUG`→`DT_NEEDED` + value
`0x8741`, writes `mb.prx\0` into the 7-byte strtab gap at `0x09317b61`, bumps
`DT_STRSZ` (`0xb93c200`); **A2** adds the Sony metadata triple (recycling
`DT_RELACOUNT`/`DT_FINI`/`0x6100003f`); **A2b** = A2 minus the `0x6100003f`
recycle. The on-console root eboot is a **plain ELF 0xFE10** the loader already
boots — no FSELF re-signing needed.

### 2a. Pre-flight (STOP on mismatch — the patch is base-locked)

```sh
$CURL_BIG "$FTP$GAME_URL/eboot.bin" -o "$EVI/p2-eboot-current.bin"   # 194 MB RETR is proven (~7 s)
shasum -a 256 "$EVI/p2-eboot-current.bin"
# MUST equal afe0a2a5fc9e1c17b215bdbb3de3c80eb0b3e1aef5c794aea491805899030b7 (194,241,424 B)
#   plain-ELF RETR is byte-identity (law §0.3.3). Any other hash ⇒ STOP; re-derive offsets from
#   the actual bytes; never stage A1/A2 against a different base.
```

### 2b. On-console pristine backup + staging (game closed; ONE boot = ONE variable)

```sh
# pristine stays on console before the first stage (EARLY-LOAD §7):
$CURL_BIG -T "$EVI/p2-eboot-current.bin" "$FTP$GAME_URL/eboot.bin.pristine-afe0a2a5"
# stage mb.prx (fake-self primary) in BOTH loader-searchable locations (fmod precedent):
$CURL_BIG -T "$T/worker-eboot-patch/mb.prx"  "$FTP$GAME_URL/prx/mb.prx"
$CURL_BIG -T "$T/worker-eboot-patch/mb.prx"  "$FTP$GAME_URL/sce_module/mb.prx"
# stage the patched eboot LAST (it is the only activation switch):
$CURL_BIG -T "$T/worker-early-load/eboot-earlyload-A1.bin" "$FTP$GAME_URL/eboot.bin"
sleep 25
# readbacks — two different laws:
$CURL_BIG "$FTP$GAME_URL/eboot.bin" -o "$EVI/p2-eboot-staged-readback.bin"
shasum -a 256 "$EVI/p2-eboot-staged-readback.bin"    # byte-exact law ⇒ must equal A1 sha 3374b5ab…
$CURL_BIG "$FTP$GAME_URL/prx/mb.prx" -o "$EVI/p2-mb-readback.prx"
#   fself ⇒ VIEW-hash law: compare PT_LOAD segments vs payload-of-record inner ELF;
#   whole-file sha WILL differ — that difference is expected, not a failure (law §0.3.3).
```

### 2c. What changes on the console

`/app0/eboot.bin` now carries a 58th `DT_NEEDED "mb.prx"`; at process creation
the kernel dynlib loop loads `prx/mb.prx` in table order (after libc, before
relocations). Nothing else on the console changes: ploader entries, plugins,
roots, and settings are untouched — a rollback of eboot.bin alone makes
`mb.prx` inert files.

### 2d. Verify the framework loads before the engine

User launches Townfall (normal launch — **no ploader arm needed for this
proof**; if ploader stays armed, its injections are unchanged and clearly
separable in the logs by their ~70 s timing). Evidence, steplog discipline
(worker-ploader-70s PATCH 03 style): the mb.prx diagnostic writes one
timestamped `module_start` line and one first-engine-contact line to its `/data`
sink (exact sink path per the worker-eboot-patch manifest; expect
`/data/mb-earlyload.log`):

```sh
$CURL "$FTP/data/mb-earlyload.log" -o "$EVI/p2-boot1-earlyload.log"
```

**PASS** = `module_start` timestamp **strictly earlier** than the earliest
engine marker, and both present. Cross-checks: D1 gains its `module_start_entered`
pair only if ploader also ran (it should be ~70 s LATER than the mb.prx line —
the direct before/after proof of the 70 s win); the game visibly reaches its
menu (engine booted normally).

**Failure discrimination (EARLY-LOAD §7 ladder; one boot each):**

| Observation | Meaning | Next boot stages |
|---|---|---|
| Game boots, NO mb.prx line anywhere | loader ignored metadata-less NEEDED (A1) | **A2** (metadata triple) |
| A2: game fails to start (boot abort) | kernel dlm rejected the table surgery (most likely the `0x6100003f` recycle) | **A2b** |
| A2/A2b: module absent still | loader ignores the entry shape entirely | **format A/B**: plain `0xfe18` mb.prx ↔ fake-self |
| Any boot abort at any rung | loader rejected the patched eboot | rollback (2e); reconcile offline; new revision only |
| Crash/freeze during dynlib phase | module_start fault in kernel context | rollback; the mb.prx revision is the suspect — capture everything |

### 2e. Rollback (Phase 2)

```sh
$CURL_BIG -T "$EVI/p2-eboot-current.bin" "$FTP$GAME_URL/eboot.bin"   # restore pristine (10/31 bytes)
sleep 25
$CURL_BIG "$FTP$GAME_URL/eboot.bin" -o "$EVI/p2-eboot-restore-readback.bin"
shasum -a 256 "$EVI/p2-eboot-restore-readback.bin"    # must equal afe0a2a5… exactly
# mb.prx copies may remain (inert once the eboot is restored) — optionally rename away:
$CURL -Q "RNFR $GAME_RAW/prx/mb.prx" -Q "RNTO $GAME_RAW/prx/mb.prx.disabled" "$FTP/"
```

Emergency note: if the game will not boot far enough for FTP, the console's
ftpsrv/pldmgr live in the HOMEWARE stack, not the game process — they stay up
across a game boot failure; a full console hang goes to crash recovery §0.3.9.
The pristine backup on-console plus the local `$EVI` copy make the eboot
restorable from either side.

---

## 4. Phase 3 — UE4SS runtime deployment (kernel-integrated candidate)

Goal: the new candidate runs in the game, reaches `mods-started`, and its logs
prove it consumes the Phase-1 kernel services instead of the old workarounds.

### 3a. Two routes, ordered by one-variable law

- **Route A (run first): ploader slot, early loading OFF.** Restore the
  pristine eboot (Phase 2e one-time, or keep a boot on pristine) and stage the
  candidate through the proven ploader channel. This isolates the
  kernel-integration variable from the early-load variable: same injection
  channel as every prior launch, one new thing (the candidate bytes).
- **Route B (after A is green): mb.prx swap.** Replace the diagnostic mb.prx
  with the candidate as `prx/mb.prx` on the patched eboot — the end-state
  architecture (runtime at T+0, no ploader dependency for the runtime). This
  boot changes exactly one thing relative to the last green Route-A boot: the
  loading channel.

### 3b. Route A staging (fself readback law throughout)

```sh
# pre-image (slot currently composition-5):
$CURL_BIG "$FTP$SLOT" -o "$EVI/p3-slot-pre.prx"; shasum -a 256 "$EVI/p3-slot-pre.prx"   # record (view law)
$CURL_BIG -T "$EVI/p3-slot-pre.prx" "$FTP${SLOT}.bak-<TAG>"
# stage candidate (expect 8 OK / no FAIL at injection per USER-LAUNCH-GUIDE):
$CURL_BIG -T "$T/worker-ue4ss-kernel/<candidate>.prx" "$FTP$SLOT"
sleep 25
$CURL_BIG "$FTP$SLOT" -o "$EVI/p3-slot-post.prx"
# PASS = PT_LOAD segments byte-identical to the candidate's payload-of-record inner ELF
#        (whole-file sha comparison is MEANINGLESS for an fself — law §0.3.3)
```

ploader.ini needs no edit (composition line is already `= 2`; probe rows stay
`= 0`). Verify with a fresh RETR + sha that the ini is byte-unchanged.

### 3c. What log lines prove kernel-service usage

Baseline ladder first (CP0→CP3 of MASTER-VERIFICATION-RUNBOOK §3, unchanged by
kernel integration): D1 `module_start_entered` → S `activated` → L UE4SS banner
(`root directory: /data/plugins`, `mods directories: [0] ue4ss/Mods`,
`Setting up mods...`) → S `engine-ready` (+ `GameEngine::Tick address (vtable:
0x33ac8c0; scan: 0x33ac8c0)`) → S `mods-started`.

Kernel-integration-specific lines (grammars marked ● are pinned in sibling
designs; ○ are this runbook's placeholders — **reconcile ○ against the
worker-ue4ss-kernel delivery manifest before rating any launch**):

| Evidence line (in L = `/data/plugins/UE4SS.log` unless noted) | Proves | Status |
|---|---|---|
| ○ `[kernel] kekcall ping ok build=<id>` (or equivalent banner line) | runtime found the Phase-1 gate | reconcile |
| ○ `[kernel] safe_read selftest ok (<n> bytes)` | K6 consumed (replaces exec-span read workarounds — the class that produced the igf6 interruption) | reconcile |
| ● `kinput_alive` / `kinput_ok` / `kinput_port` sdiag records; `pad_event_edge key=<K>` per press (INPUT-SERVICE §6.3) | K8 consumed: input arrives without any `scePadOpen` (no `pad_open_class`, no `0x80920004` ALREADY_OPENED retry ladder at all) | pinned design |
| ○ scanner line showing shadow/pattern scans resolved via nr=7/8 (e.g. `[shadow] pattern <name> @ 0x… via safe_read`) | K6/K7 consumed by the shadow scanner | reconcile |
| `GameEngine::Tick address …` both fields `0x33ac8c0` | layout path healthy (unchanged from prior ladder; scan preferred under `EngineTickResolveMethod=Scan`) | pinned |

Negative discriminators: any `rc=78`/ENOSYS-style refusal in the runtime's
kernel diag = old module resident (Phase 1 regression — re-run the kprobe
ladder read-only); `pad_input_deferred… 0x80920004` sequences = the runtime
fell back to the native pad route (kernel input refused — check `-19 NOSTATE`
shape); `ExecuteInGameThread: … hook is not available` = a restage of old ini
bytes happened (MAC-SESSION-BRIEF §5 distinct-failure list).

### 3d. Rollback (Phase 3)

1. Soft disable (game closed): single-character ini edit `= 2` → `= 0` on the
   composition line (ROLLBACK-RUNBOOK §4.4 procedure verbatim: pre-image RETR,
   on-console `.bak`, one-line diff, STOR, fresh byte-exact readback).
2. Slot restore to known-good composition-5: STOR
   `$T/worker-composition-5-rebuild/prx-candidate-composition5-2/modbridge_supplier_composition.prx`
   (upload sha `04869ab3…`), fresh readback classified per the view law.
3. Route B extra: revert `prx/mb.prx` to the Phase-2 diagnostic bytes or rename
   away; eboot stays patched only if the diagnostic boot was already green.

---

## 5. Phase 4 — Mod loading and the Item Spawner menu test

Goal: rungs 4+5 — visible menu + spawned item. Everything below condenses
MASTER-VERIFICATION-RUNBOOK §§3–5; that document owns the full failure tables.

### 4a. Stage the mod tree (game closed; plain files ⇒ byte-exact readbacks)

```sh
# mods folder under the UE4SS working root:
$PLUGINS/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua     # sha f58604…9658 (canonical 21,415 B)
$PLUGINS/ue4ss/Mods/TownfallItemSpawner/Scripts/UEHelpers.lua # sha 5a8f66…15a7 (shared-mod layout)
$PLUGINS/ue4ss/Mods/TownfallItemSpawner/enabled.txt
$PLUGINS/ue4ss/PS5Pad.ini                                     # 2960 B, sha 1d919fbf…, OPTIONS=F7 + 5 nav chords
# plus the loadlist rows in $ROOTS/loadlist.txt (lua_mod role) per the activation contract
```

Every file: STOR → wait 15–30 s → fresh RETR → whole-file sha equality (all
plain files). Any mismatch is a STOP.

### 4b. The launch and the press (user ritual)

User: Townfall fully closed → (Route A: ploader armed by controller) → launch
normally → ~1 min: expect the OK injection lines, no FAIL → **load a save** →
wait ~30 s → **press OPTIONS once** (brief press-and-release; edge-triggered,
exactly one event; holding does not re-fire).

### 4c. Evidence ladder (checkpoint order; fetch ~60 s after launch, once)

```sh
python3 $T/worker-post-launch-tools/post_launch_check.py     # Step A classifier (S+L+D1 one pass)
# manual fallbacks per §2 of MASTER-VERIFICATION-RUNBOOK (one bounded curl per sink)
```

| CP | Gate | Decisive evidence |
|---|---|---|
| CP4 | mod loaded | exactly one fresh `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` in L |
| CP5 | OPTIONS opens menu | two fresh lines, in order: `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<P>` then `[Lua] [TownfallItemSpawner] Menu created.` — **and** the user sees the overlay (12 rows + controls line; game input locked while open). With no save: the expected guard line `Scan failed: … Item database not ready. Load a save first.` (valid negative test T3, not a fault) |
| CP6 | CROSS spawns | `[Lua] [TownfallItemSpawner] Spawned <ItemName> x<quantity> ID=<inventoryID>` **and** the user sees the pickup actor at the aim point (log without actor = investigate; actor without log = impossible by construction) |
| (kernel bonus) | K8 end-to-end | one `pad_event_edge key=F7`-class record attributable to the same press (closes gap G-AP1 — first direct "controller input ARRIVED" signal) |

Failure routing (condensed; full tables in MASTER-VERIFICATION-RUNBOOK §6):
`Error loading/executing script` / `module 'UEHelpers' not found` → CP4 script
layer (fix mod staging, NOT the PRX); `WidgetBlueprintLibrary not found.` /
`UI creation failed: <err>` → CP5 reflection/layout; `Failed at <stage>` (ground
trace / load pickup class / spawn actor / configure pickup) → CP6 stage table;
zero reaction to OPTIONS → §4.3d ladder (banner? ini? clean edge? liveness?)
before any escalation. Every fix is a distinct revision; never re-rate
unchanged evidence, never re-stage identical failed bytes.

---

## 6. Phase 5 — Full ecosystem activation

Each sub-phase is its own authorized launch. File operations game-closed.

### 5a. File overlay (drop a test mod file → engine loads it)

Channel: container projection (OVERLAY-DESIGN approach 1 — the shipped L1;
engine-native PAK/IoStore discovery, zero new runtime code, add-only owned
names, transactional staging). The engine scans `/app0/townfall/content/paks/`
(PAK-OVERLAY §1 proved the scan chain and the `_P` priority recognition).

```sh
# 1. drop zone (installer-owned; create via installer verb — MKD over QUOTE is unverified):
#    $OVL/overlay.txt, $OVL/paks/, $OVL/~mods/, $OVL/configs/, $OVL/textures/
# 2. place a SMALL ORIGINAL-ONLY test trio (P1 recipe: re-stage one original container
#    under owned names — no new bytes, engine-side effect is guaranteed if mounted):
#    pakchunk9zz-ModBridge_<uuid>_P.{pak,utoc,ucas}   (naming law: reserved 999x band + _P suffix)
# 3. installer verb project-overlay: classify → journal → STOR add-only into
#    $GAME_RAW/townfall/content/paks/ → full sha256 readback of every projected file
# 4. launch; collect evidence
```

**PASS (the W1 gate)** = engine-side proof of mount — in order of decisiveness:
(1) a user-visible original-asset effect; (2) mounted-pak enumeration from the
running runtime's log; (3) a decisive diagnostic-render-control result. A
silent launch with files present is **not** evidence ("filesystem visibility
does not establish that Unreal loaded the container" — the §1.4 lesson).
Rollback: delete exactly the owned projected names (journal is the commit
truth); originals were never touched (`original_game_files_overwritten:false`
discipline).

### 5b. Debugger (connect from the Mac, inspect the live game)

The RSP server runs in a payload process; memory/register access rides the
Phase-1 kernel services through `ps5_backend.c` / `kio_shim.h` (KIO_READ_MEM/
WRITE_MEM/GET_REGS; nr=7-class read path).

```sh
# console: stage + dispatch the stub ONCE (own authorization; binds per manifest):
curl -s "$HTTP/loadpayload:/data/pldmgr/payloads/modbridge-gdbstub/gdbstub.elf"
#   (stub must be started with the explicit bind address -a 0.0.0.0:2345 for LAN use;
#    127.0.0.1 default is host-only by design)
```

From the Mac (lldb speaks the same RSP; the stub's host tests used exactly this):

```
lldb
(lldb) gdb-remote ps5-host-not-configured.invalid:2345        # attach; expect stop reply T05, arch x86_64
(lldb) register read rip rax rsp           # 57-reg x86-64 file served from td_frame/pcb_save
(lldb) memory read 0x400000                # game image base → expect 7f 45 4c 46 … (ELF magic)
(lldb) disassemble -s <rip>                # live x86-64 decode of game code
(lldb) platform select remote-freebsd      # target identity x86_64-unknown-freebsd12
```

**PASS** = clean attach + correct registers + game-memory read that matches
known bytes (ELF header at the image base; a known static string) + `detach`
leaves the game running. Honest bounds (worker-kernel-debugger §1): phase-1
execution semantics are inspect-only — `?`/`c`/`s` answer immediate `T05`
stops; breakpoints (`Z0`) work only if the manifest ships the writable-text
patch + int3 machinery, else `E01` — inspect-only is the designed PASS bar
here. `k`/`vKill` translate to detach; the stub can never kill the game.
Rollback: dispatch nothing further; the stub exits with its payload process;
no console-side residue.

### 5c. Asset conversion (convert a test texture → verify it loads)

Host-side pipeline (ASSET-CONVERT §4–§6): pick a small **original PS5 texture
package** as the test subject (not a PC sRGB variant — unpinned mappings fail
closed); roundtrip it through the converter
(`unpack → neutral BC → GFX10 SW_64KB_S per-mip swizzle → patch Texture2D
platform data → repack`), requiring `deswizzle(swizzle(x)) == x` per-mip by
SHA-256 BEFORE any console contact. Then:

```sh
# project the converted container through the SAME overlay channel as 5a
#   (owned pakchunk9zz-ModBridge_<uuid>_P trio, journal + readback)
# launch → the W1-class evidence bar applies again, plus:
```

**PASS** = the texture visibly changes in-game (or the runtime's mount
enumeration shows the package resolved from the overlay container) — a
roundtrip hash match alone is bytes-evidence, never engine acceptance (repo
law). Rollback: identical to 5a (delete owned names). If the engine mounts the
container but renders garbage: descriptor/swizzle defect — new revision, never
a blind re-stage; the failure artifact (screenshot + mount lines) is the
evidence.

---

## 7. Risk register

Severity: blast radius × likelihood. "Signal" = the observable that fires the
row; every row's rollback references the phase section. Crash-class rows go
through CONSOLE-SAFETY §e before ANY retry.

| # | Phase | Risk | Signal | Mitigation | Rollback |
|---|---|---|---|---|---|
| R1 | 1 | Installer finds a resident module (autoload order; another kstuff-family payload won the gate) → extended arms never install | kprobe: ping 0 but nr=7 → 78 | extended installer FIRST in autoload.txt; cold boot (not warm); probe ladder before any dependent phase | restore autoload pre-image + cold boot (§1e) |
| R2 | 1 | Firmware-offset mismatch (13.600.007 family) → install aborts or kernel instability | install-time abort record; console hang/panic class | worker-kernel-deploy pins offsets per FW; probe ladder is read-only; novel-ops-LAST ordering | §1e + crash recovery §0.3.9; new module revision only after root-cause |
| R3 | 1 | Double-dispatch / <30 s spacing → console crash (the only observed self-inflicted console-killer) | dispatch timeout, port refusal | ONE dispatch per authorization; ≥30 s spacing; timeout = failure signal, never retry | crash recovery; log + reconcile |
| R4 | 1 | Second IDT/sysent patch from two modules (order-dependent uninstall hazard) | unexpected second installer run | never dispatch a second installer in one boot; the built-in gate refuses — honor the refusal, do not "work around" it | cold boot; single-installer autoload |
| R5 | 2 | Loader rejects patched eboot → game boot abort (A1 ignored, A2 abort, A2b abort) | game fails to start; engine never logs | base-locked patch script refuses wrong base pre-flight; one-variant-per-boot ladder A1→A2→A2b→format-A/B | restore pristine eboot (§2e, byte-exact verified); /data survives game boot failure |
| R6 | 2 | 194 MB STOR/RETR corruption or stale-cache readback | readback sha ≠ this-attempt receipt | fresh-connection readbacks; size+sha vs THIS receipt; unique destination names for high-stakes writes | STOP on mismatch (never re-pin); re-derive from actual bytes |
| R7 | 2 | mb.prx fself verified by whole-file sha → false-alarm re-upload churn | "hash mismatch" panic | view-hash law: PT_LOAD equality only; expected readback sha written down BEFORE upload | none needed (law prevents the false alarm) |
| R8 | 2 | module_start fault in kernel dynlib context → crash at process create | console/game crash before any engine log | minimal import surface (libkernel+libc NIDs only); module_start defers engine work to readiness thread | §2e pristine restore; mb.prx revision suspect — capture, reconcile |
| R9 | 3 | Candidate import drift (scePadGetHandle 196→197 or other UND delta) → loader FAIL line | FAIL at injection; module never runs | `launch11_one_pass.py verify` + import audit before staging (any drift beyond the receipted row = STOP) | slot restore to composition-5 (§3d.2) |
| R10 | 3 | GATE-1-class walkguard refusal returns (exec-text read) | init unwinds at `Finding GameEngine::LoadMap` | kernel-integration candidate routes reads via nr=7 (the point of K6); no-jmp-follow fix lineage in the build | prior known-good slot; reconcile candidate lineage |
| R11 | 3 | Runtime falls back to native pad (kernel input refused: −19/ENOSYS) | `pad_input_deferred… 0x80920004` sequences instead of `kinput_*` | fallback is BY DESIGN non-fatal; record refusal code; K8 degraded is honest | none (menu still reachable via native route); offsets revision for −19 |
| R12 | 4 | Menu/spawn engine paths never executed live (LineTraceSingle marshalling = rank-3 risk) | `Failed at ground trace` (or per-stage shape) | stage-named failure table; distinct revision per fix; host smoke shapes pre-pinned | mod-side fix only — NEVER re-stage the PRX for a CP4/CP6 script failure |
| R13 | 4 | Zero reaction to OPTIONS (input-dead vs drain-dead ambiguity) | no new L lines on press | §4.3d ladder (banner → ini sha → clean edge → liveness → known non-causes); with K8 live, `pad_event_edge` disambiguates instantly | escalate with D1/S/L tails; no press-spam |
| R14 | 5a | Engine does not mount staged containers (W1 open) | silent launch, files present | original-only test trio first (guaranteed engine-side effect IF mounted); naming-law calibrated from the title's own `_P` vocabulary | delete owned projected names (journal); W1 stays honestly open |
| R15 | 5a | Overlay write touches originals | listing vs paks census diff | add-only owned names; `original_game_files_overwritten:false`; pre-phase paks census (§0c) is the oracle | restore from census-identified `.original-<uuid>.bak` backups |
| R16 | 5b | gdb stub memory read faults on unmapped game memory | `E01` to the debugger | partial-transfer reporting policy (never fabricate); pid ACL; readonly build variant if unstable | detach; inspect-only PASS bar stands |
| R17 | 5b | RSP port exposed on LAN | unexpected connections | stub binds 0.0.0.0 ONLY for the authorized test window; prefer console-firewall/LAN-trust posture; `-a` explicit | stop stub payload after evidence capture |
| R18 | 5c | sRGB/format mapping unpinned for target → garbage texture | mounted but visually wrong | fail-closed converter (missing_capability, never a guess); original-package test subject; per-mip roundtrip hash before staging | §5a rollback; converter revision |
| R19 | all | Evidence-sink staleness misread as success/failure | mtime/freshness drift | freshness oracle (D1 launch count, S session pid/ms, stale-file rename before runs); one bounded fetch per sink per launch | re-fetch once after 60 s; then reconcile |
| R20 | all | Console crash mid-phase | ports refused / ping loss | §0.3.9 ritual; user power-cycles; /data persistence twice-proven | read-only recovery, reconcile, then the phase's own rollback |

---

## 8. Success criteria (what evidence proves each phase)

No phase is claimed without its named artifact captured under `$EVI/` (host)
with sha256 receipt. Host tests, drafted logs, and predictions never advance a
phase (do-not-claim guard).

| Phase | Claim | Required evidence (artifact) |
|---|---|---|
| 0 | Ready | receipt table §0a fully pinned (or explicit STOP); reachability 200; backups hashed (`preflight-$TAG/`) |
| 1 | Extended kernel module live, N/8 capabilities | kprobe JSONL: ping `rc=0`; nr=7 self-test ≥0 bytes verified; nr=8 good/bad discrimination; nr=1/2/3/5/9 records or clean refusal codes; N/8 recorded honestly (`p1-kprobe.jsonl`) |
| 2 | Framework loads before engine | `mb-earlyload.log`: `module_start` timestamp strictly earlier than first engine marker, same boot; game reaches menu (engine healthy); variant ladder row recorded (`p2-boot<N>-earlyload.log`) |
| 3 | Kernel-integrated runtime at mods-started | S: fresh `activated`→`engine-ready`→`mods-started`; L banner; Tick line `0x33ac8c0`/`0x33ac8c0`; ≥1 kernel-consumption line per §3c (● grammars verbatim; ○ reconciled to the manifest first); slot readback classified per view law |
| 4 | Menu visible + item spawned | CP4 banner exactly once; CP5 `<N> spawnable pickups found.` + `Menu created.` + user-visible overlay; CP6 `Spawned <Item> x<N> ID=<id>` + user-visible actor; (bonus) `pad_event_edge` for the same press |
| 5a | Overlay loads files | W1-class engine-mount proof (visible effect or mount enumeration) — NOT file visibility; journal + readback receipts |
| 5b | Debugger inspects live game | lldb transcript: attach stop reply, register read, `memory read 0x400000` = `7f 45 4c 46…`, disassembly, clean detach, game alive after |
| 5c | Converted texture loads | converter roundtrip hashes (per-mip) + engine-side visible/enum evidence via the 5a channel; roundtrip hash alone explicitly NOT acceptance |

**Final ecosystem state (all phases green)**: 8/8 kernel capabilities live ·
runtime at T+0 via DT_NEEDED · Item Spawner menu opens on OPTIONS and spawns on
CROSS · overlay delivers files the engine mounts · Mac-side debugger inspects
the live game · converted PC textures load through the overlay. Each claim
backed by its artifact above; anything short stays honestly unclaimed.

---

## 9. Provenance index (primary sources, all in this worktree)

| Fact class | Source |
|---|---|
| Console endpoints, autoload, pldmgr, kstuff residency, per-boot kernel state, /proc denial | `worker-boot-protocol/BOOT-PROTOCOL.md` + `probe/` |
| Readback laws, ini discipline, ploader mechanics, known-good artifacts, pre-flight law | `worker-rollback-runbook/ROLLBACK-RUNBOOK.md` |
| Safety gates, dispatch discipline, crash recovery, prohibitions | `worker-console-safety/CONSOLE-SAFETY-PROCEDURE.md` |
| Kernel build pipeline, deploy-on-next-boot, KEKCALL gate, extension recipe | `worker-kernel-build/BUILD-GUIDE.md`; `worker-kernel-saferead/safe_read_kernel.c` |
| kekcall nr=9 ABI, KekcallPadSource, kinput diag grammars | `worker-kernel-input/INPUT-SERVICE.md` |
| GDB stub usage, host-test evidence, kernel integration plan | `worker-kernel-debugger/REPORT.md` + `gdbstub.c`/`ps5_backend.c`/`kio_shim.h` |
| eboot patch bytes, ladder A1/A2/A2b, mb.prx deployment, staging/rollback | `worker-early-load/EARLY-LOAD-DESIGN.md` (+ `patch-apply.py`, `patch-verification.txt`) |
| Engine pak scan chain, `_P` priority, naming law | `worker-kernel-pak/PAK-OVERLAY.md` |
| Overlay drop-folder, projection map, W1 gate | `worker-file-overlay/OVERLAY-DESIGN.md` |
| Asset conversion pipeline, fail-closed rules | `worker-asset-convert/ASSET-CONVERT.md` |
| Menu/spawn checkpoints, log grammars, failure tables, mod shas | `worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md`; `worker-spawn-playbook/SPAWN-PLAYBOOK.md`; `worker-user-launch-guide/USER-LAUNCH-GUIDE.md` |
| B1/B2/B3 blockers + staged fixes, candidate build matrix, import audit | `outputs/2026-10-06-claude-cloud/MAC-SESSION-BRIEF.md` |

**Honest limits of this runbook.** No command here has been executed. The ●
log grammars are pinned in sibling designs; the ○ ones are placeholders to
reconcile against worker-kernel-deploy / worker-eboot-patch /
worker-ue4ss-kernel delivery manifests, which did not exist at writing time.
The 8-capability table is an assumed contract (§0.2). Every "expected"
observation is a prediction until the first live launch produces it; this
document defines procedure and evidence, and certifies nothing.
