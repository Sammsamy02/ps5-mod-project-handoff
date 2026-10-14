/* mfw8_service.h — drop-in modforge-service/1 backend over records+actions+caps.
 *
 * worker-8-app-backend (2026-10-04). Implements the vendor ABI exactly as
 * mod_service/vendor/modforge_service.h declares it (mfs_open / mfs_request /
 * mfs_close + mfs_owned release), so it can be built into the static archive
 * that native-host/build.py --mod-service-profile force-loads (STARTUP.md)
 * and becomes the backend behind host.c MB_HOST_MOD_SERVICE (host.c:442-455)
 * and the read routes http.c:771-799.
 *
 * Method subset (honest; service.describe lists exactly this):
 *   service.describe, records.list, mod.register_download, mod.prepare,
 *   install.apply, mod.uninstall, job.status, job.cancel, job.list,
 *   capabilities.query, game.status, mod.forget
 * Everything else -> unknown_method. install.plan, game.register, FOMOD
 * sub-methods stay unimplemented until their vertical lands.
 *
 * Delegation rules (the whole point — no second implementation of semantics):
 *   mod.prepare        -> installer `prepare` (bundled) or `prepare-archive`
 *                         with the proven auto-bridge (bundle-less downloaded
 *                         original from the intake dir)
 *   install.apply      -> installer `install` (transactional root swap)
 *   mod.uninstall      -> installer `install --remove`
 *   recovery           -> installer `recover` at open (SERVICE.md mfs_open)
 *   running            -> derived only from <roots>/<title>/runtime-status.txt
 *
 * Job model honesty: actions execute synchronously inside the request (the
 * serialized service mutex); replies carry the FINAL job object with the
 * installer's own exit classification. job.cancel on a missing job refuses
 * unknown_job; on a terminal job it is a documented no-op replying the
 * current state. There is no queued job to cancel in this model.
 *
 * records.list shape: "mods" carries exactly the rows the app's
 * mod_service/register_bridge.cpp record() validation accepts (download_id,
 * title_id, state, source{provider,domain,mod_id,file_id}); rows registered
 * outside the app (installer CLI) are reported in "installer_records" so
 * nothing is hidden and the app path never 502s on them.
 *
 * Config (mfs_open_params.config_json): flat object; required keys
 *   "state_dir", "mod_root_base"; optional "installer_path", "intake_dir".
 * Unknown keys are ignored (the app ships richer configs); unknown DOES NOT
 * enable anything. The service never writes outside state_dir and mod roots.
 *
 * Threading: one pthread mutex serializes request/open. */
#ifndef MFW8_SERVICE_H
#define MFW8_SERVICE_H

#include <stddef.h>
#include "modforge_service.h"

/* Direct ABI passthrough (same signatures as the vendor header). */
int mfw8_mfs_open(const mfs_open_params *params, mfs_service **out, mfs_owned *error);
mfs_owned mfw8_mfs_request(mfs_service *svc, mfs_view request_json);
void mfw8_mfs_close(mfs_service *svc);

/* Compile-time adapter so the object satisfies mb_mod_service_operations
 * (mod_service/owner.h) with the exact expected names. */
#ifndef MFW8_NO_MFS_ALIASES
int mfs_open(const mfs_open_params *params, mfs_service **out, mfs_owned *error);
mfs_owned mfs_request(mfs_service *svc, mfs_view request_json);
void mfs_close(mfs_service *svc);
#endif

#endif
