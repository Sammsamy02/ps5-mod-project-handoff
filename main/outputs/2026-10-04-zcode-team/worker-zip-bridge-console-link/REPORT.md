# REPORT.md — worker-zip-bridge-console-link: bridge TU in the console installer payload

Origin: worker-zip-bridge-console-link, 2026-10-04 (ZCode team, PS5 mod-manager).
Goal: close worker-zip-to-mfb's remaining console-only step — link the F-R1
archive->bundle bridge TU (`owned/modinst_zip_bridge.{c,h}`) into the
CONSOLE-flavor installer payload composed from
`worker-installer-composition/compose_installer.py` (read-only), with the
import provider check green and the 75/75 bridge suite still green via the
composed copy. Host-side only; no network; no console contact; no original
file modified; everything under
`outputs/2026-10-04-zcode-team/worker-zip-bridge-console-link/`.

## 1. Result

**Console payload linked with the bridge TU; provider check green; 75/75 host
regression green via this directory's composed binary.**

```
python3 compose_console_bridge.py --flavor console   # payload step (deliverable)
python3 compose_console_bridge.py --flavor host      # suite binary
sh test/run_bridge_tests.sh                          # PASS=75 FAIL=0, exit 0
```

### New console payload (the deliverable)

| Artifact | Value |
|---|---|
| ELF | `composition/build/console/modbridge-installer.payload.elf` |
| sha256 | `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9` |
| bytes | 681632 |
| dynamic imports | 116, **missing=[]** (nm abort-on-missing provider check passed) |
| import delta vs pre-bridge baseline | **none** (identical 116-name set) |
| pre-bridge baseline | worker-installer-composition payload: sha `55ae402c95d2bc7e2036a5486b298d950c77c1cdd329aa894979fc9b4b70217d`, 647928 B, 116 imports |
| size delta | +33704 B (bridge TU + patched core dispatch) |
| link map | `composition/build/console/modbridge-installer.payload.map` — `modinst_zip_bridge.ps5.o` sections present; `mi_bridge_run` / `mi_bridge_wants_auto` / `mi_bridge_mod_arg` defined by the bridge object and referenced from `modinst_core.ps5.o` |
| bridge TU compile receipts | `composition/receipts/console-compile-modinst_zip_bridge.{stdout,stderr}` both **0 bytes** (clean under the console core_flags) |
| import providers receipt | `composition/receipts/console-import-providers.json` (per-import provider libs; top: libSceLibcInternal/libc_stub_weak 79, libkernel* 37, libScePosixForWebKit 3) |

No new dynamic imports: the bridge TU consumes the same pinned ps5io/libc
surface as the other owned TUs, so the console payload's external contract is
unchanged — only static content grew.

### Host regression via the composed copy

`test/run_bridge_tests.sh` is a verbatim copy of worker-zip-to-mfb's suite
(only the header comment differs; `BIN` auto-resolves to THIS directory's
`composition/build/host/modinst_composed`). Result: **PASS=75 FAIL=0,
exit 0** (`test/bridge-e2e-summary.log`, full transcript
`test/bridge-e2e.log`). Transcript sanity matches the delivered run: zb-neg
3/3 with the 1 intentional failed end (refusal); zb-off 1 unmatched begin =
the known open F-R2 install-refusal behavior, unchanged.

## 2. Patch chain (recorded, anchored, sha-pinned; originals never touched)

`compose_console_bridge.py` (this dir) replays the delivered lineage and
appends this worker's corrections; every step is in
`receipts/console-bridge-script-patches.json`:

1. **Chain root**: vendor `worker-installer-composition/compose_installer.py`,
   sha cross-checked against worker-e2e-final
   `e2e-receipts/script-patches.json` `vendored_script_sha256`
   (`e9074052…` — matches the current file).
2. **E2E layer**: vendor `worker-e2e-final/compose_e2e.py`, sha cross-checked
   against worker-zip-to-mfb `e2e-receipts/bridge-script-patches.json`
   `vendored_driver_sha256` (`3f310101…` — the exact bytes that produced the
   green 75/75 delivery). Honest note: e2e-final's own
   `adapted_script_sha256` pin (`606a8a98…`) covers an earlier revision of
   compose_e2e.py; it was superseded before delivery, so the zip-to-mfb pin
   is the correct provenance anchor and the divergence is recorded here.
3. **Verbatim replay**: worker-zip-to-mfb's 4 recorded splices
   (inner-name, owned-authored-list, host-target, patches-append) applied
   unchanged; the intermediate driver text hashes EXACTLY to zip-to-mfb's
   delivered `adapted_driver_sha256` (`897cf0eb…`) — logged as
   `REPLAY-PROOF` in the receipt. Byte-exact provenance, not a re-derivation.
4. **This worker's 3 corrective splices**:
   - `zb-console-inner-name` — distinct inner script name
     (`composition_zb_console_inner.py`, this dir);
   - `zb-console-targets-seq` + `zb-console-target-def` — add
     `("modinst_zip_bridge", HERE / "owned/modinst_zip_bridge.c", core_flags)`
     to the CONSOLE target list via one more exact-anchor splice tuple
     (`S_CONSOLE_TARGETS`) in the driver's splice sequence (the console list
     only exists at inner-generation time, so the fix extends the same
     mechanism the delivered chain uses).

Owned inputs: the 11 delivered files vendored from `worker-zip-to-mfb/owned/`,
each sha-verified against zip-to-mfb's `owned-manifest.json`, and the 9
pre-bridge files additionally against worker-e2e-final's
`owned-manifest.json` (`receipts/vendored-owned.json`). The adapted inner
builder then applies the full 45-patch modinst.c chain (base 31 + e2e 6 +
bridge 4) at build time; `composition/receipts/patches.json` has every anchor
and hash for THIS build.

## 3. Portability finding (recorded, fixed in the chain — bridge TU itself needed NO changes)

**Finding**: the delivered chain wires `modinst_zip_bridge.c` into the HOST
target list only, while the `zb-dispatch-bridge` modinst.c patch makes the
patched `modinst_core.c` reference `mi_bridge_wants_auto` / `mi_bridge_run` /
`mi_bridge_mod_arg` on BOTH flavors. `compose_bridge.py --flavor console` as
delivered would therefore abort at link with undefined `mi_bridge_*`
(reproduced logically by target-list inspection of the delivered inner
script; not run to failure — the fix was applied instead).

**Fix**: the `S_CONSOLE_TARGETS` splice adds the bridge TU to the console
list with the same `core_flags` as the other owned TUs. The bridge TU then
compiled clean under console flags (zero-warning receipts) and linked with
zero import delta — `modinst_zip_bridge.{c,h}` are console-portable as
delivered; the defect was in the composition wiring, not the TU. The
correction is recorded in
`receipts/console-bridge-script-patches.json` → `corrections[0]`.

## 4. Remaining console-only steps (controller-owned; this worker touched no console)

1. **Stage + launch**: FTP the payload to the console and run the established
   launch ritual (ploader arm -> game launch -> injection; freshness oracle =
   runtime log mtime). Not performed here (out of this worker's scope).
2. **Stage-1 download on console is still open**: the download TU remains
   host-linked only (e2e-final design; console payload has no download
   command — import set identical to baseline proves it). Console E2E stage 1
   needs the worker-ps5io-rdwr native download closure merged (integration
   queue item) or archives staged by another route; then the staged E2E run
   per worker-e2e-rehearsal READINESS.md GO conditions.
3. **On-console truth**: `running=1` here is the host simulate-runtime
   ack-file proof only; the injected runtime's `module_start` ack remains the
   real acceptance (unchanged runbook).
4. **Bridge v1 bounds still apply** (BRIDGE-DESIGN.md §6): <=256 members,
   <=64 MiB member bytes, <=1 MiB manifest; larger archives are refused
   honestly. Streaming variant is future work. F-R2 (install's dangling begin
   on refusal) remains open and is not a bridge defect.

## 5. Bounds

31 tool calls used (budget <=35); no nested subagents; no network; no console
contact; originals untouched (vendored copies + recorded patch chains only);
all writes inside this directory.
