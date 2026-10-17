#ifndef MB_CURL_DOWNLOAD_H
#define MB_CURL_DOWNLOAD_H
#include <stddef.h>
#include <stdint.h>
#include "runtime.h"
#include "../archive/private/file_sink.h"
typedef struct mb_curl_download_params_v1 {
 uint32_t struct_size,abi_version;
 const char *url;
 const uint8_t *ca_pem;
 size_t ca_bytes;
 /* Optional trusted DNS mapping in CURLOPT_RESOLVE syntax; never a trust
  * override. The URL hostname is still authenticated against ca_pem. */
 const char *resolve;
 uint32_t timeout_ms,connect_timeout_ms;
 mb_download_file_sink_params_v1 sink;
 void *cancel_context;
 mb_curl_cancel_fn_v1 cancelled;
} mb_curl_download_params_v1;
typedef struct mb_curl_download_result_v1 {
 uint32_t struct_size,abi_version;
 mb_curl_download_status_v1 status;
 uint32_t complete,http_status;
 int32_t curl_code;
 uint32_t reserved;
 uint64_t received_bytes;
 mb_download_file_sink_result_v1 file;
} mb_curl_download_result_v1;

/* One serialized process owner, with exclusive curl and PSA ownership. Start
 * once, perform sequential operations with fresh handles/deadlines/entropy
 * budgets, close once. Source and invalid/backward clock faults are terminal;
 * HTTP/TLS rejection and an ordinary job timeout do not reset that owner.
 * There is no reset/rebind/retry API. Calls and platform hooks must not race.
 * Params/callback contexts remain valid and immutable throughout each call.
 * URL must be a provider-authorized HTTPS resource: this producer does not
 * obtain Nexus entitlement, emit account headers, follow redirects or install.
 * Body bytes stream through a fixed 64 KiB carry to the borrowed unpublished
 * sink. A complete receipt requires real transfer success, sync and readback.
 * Failed prefixes stay private/caller-owned. URL, query and raw provider/TLS
 * errors are never returned or logged. No native runtime is claimed here. */
#ifdef __cplusplus
extern "C" {
#endif
int mb_curl_download_v1(const mb_curl_download_params_v1 *,mb_curl_download_result_v1 *);
#ifdef __cplusplus
}
#endif
#endif
