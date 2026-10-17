/*
 * modinst_download.h — E2E stage-1 download command for the composed installer.
 *
 * Origin: worker-e2e-final, 2026-10-04 (owned).
 *
 * Binds the composed installer's download stage to the REAL exclusive
 * curl-owner transport ABI v1 (vendored download/runtime.h and
 * download/curl_download.h, origin outputs/ps5-mod-bridge/native-host/download,
 * receipt callable_archive_download_transport @ ba0c4f1):
 *
 *     mb_curl_runtime_start_v1  ->  mb_curl_download_v1  ->  mb_curl_runtime_close_v1
 *     (start once)                 (one job per process)     (close once)
 *
 * Explicit-config-only semantics (receipt native_nexus_download_jobs:
 * "catalog_and_scan_jobs_explicit_config_only": true): every job fact comes
 * from one --job TSV file; unknown keys are refused; nothing is discovered.
 *
 * NO credentials in code: the URL must already be a provider-authorized
 * resource (the owner resolves entitlement/URIs — mb_nexus_resolve_download_v1
 * — under the signed-in user session and owns auth). This command never sees,
 * stores or logs credentials; the URL itself is never written to any record
 * (transport result facts are scalar only, per the ABI contract).
 *
 * Host test configuration links owned/mb_curl_stub_host.c (a contract-faithful
 * local-file stand-in for the owner) instead of the pinned curl closure; the
 * console link must add the pinned native download closure (see REPORT.md).
 */
#ifndef MODINST_DOWNLOAD_H
#define MODINST_DOWNLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

/* argv: download --job JOB.tsv   (state dir passed by the composed main)
 * Exit codes follow the composed core: 0 ok, 1 io/transport, 2 bad request.
 * On success the caller (composed main) registers the mod row downloaded. */
int mi_cmd_download(int argc, char **argv, const char *state_dir);

#ifdef __cplusplus
}
#endif

#endif /* MODINST_DOWNLOAD_H */
