/*
 * modinst_zip_bridge.h — F-R1 bridge: quarantine -> modforge bundle.
 *
 * Origin: worker-zip-to-mfb, 2026-10-04 (owned).
 *
 * `prepare-archive --mod ID` already materializes a verified quarantine
 * (member-*.bin + manifest-v1.bin) through the REAL pinned intake. This
 * adapter composes the quarantine into a kind-1 modforge bundle
 * (<state>/bundles/<ID>.mfb) — the SAME single mfb format the supplier path
 * uses (worker-7 modinst make-bundle / modforge bundle builder) — so
 * `install --mods <ID>` resolves a real bundle instead of refusing with
 * "not a modforge bundle" (worker-e2e-rehearsal finding F-R1).
 *
 * Design/mapping rules and defaults: BRIDGE-DESIGN.md in this directory.
 * The bridge runs ONLY from the composed core's prepare-archive dispatch,
 * AFTER a successful intake and BEFORE the downloaded->prepared records
 * transition, so a refusal leaves the record at `downloaded` and the
 * quarantine retained. No new bundle format is introduced.
 */
#ifndef MODINST_ZIP_BRIDGE_H
#define MODINST_ZIP_BRIDGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 1 exactly when --mod was given and --no-auto-bundle was not (the
 * archive-sourced-mod default: auto-bundle ON, explicit opt-out). */
int mi_bridge_wants_auto(int argc, char **argv);

/* The --mod value, or 0 when absent. */
const char *mi_bridge_mod_arg(int argc, char **argv);

/* Compose <state_dir>/bundles/<mod>.mfb from the quarantine's
 * manifest-v1.bin (the intake's own durable member map). Returns 0 and
 * fills bundle_out (the bundle path) on success. Non-zero: refusal or io
 * failure — the specific reason is printed, a stage-2 fail e2e status
 * event is emitted, the quarantine is retained and NOTHING under the mod
 * root is touched. Exit-code class matches the composed core: 2 = bad
 * request (refusal), 1 = io. */
int mi_bridge_run(int argc, char **argv, const char *state_dir,
                  char *bundle_out, size_t bundle_out_n);

#ifdef __cplusplus
}
#endif

#endif /* MODINST_ZIP_BRIDGE_H */
