#ifndef MB_CURL_RUNTIME_H
#define MB_CURL_RUNTIME_H
#include <stdint.h>

#define MB_CURL_DOWNLOAD_ABI_V1 1u
#define MB_CURL_DOWNLOAD_URL_MAX 8192u
#define MB_CURL_DOWNLOAD_CA_MAX 1048576u
#define MB_CURL_DOWNLOAD_TIMEOUT_MAX_MS 300000u

typedef enum mb_curl_download_status_v1 {
 MB_CURL_DOWNLOAD_OK=0, MB_CURL_DOWNLOAD_BAD_ARGUMENT,
 MB_CURL_DOWNLOAD_BAD_STATE, MB_CURL_DOWNLOAD_OWNER_FAILED,
 MB_CURL_DOWNLOAD_INIT_FAILED, MB_CURL_DOWNLOAD_SETUP_FAILED,
 MB_CURL_DOWNLOAD_TRANSPORT_FAILED, MB_CURL_DOWNLOAD_RESPONSE_REFUSED,
 MB_CURL_DOWNLOAD_LIMIT, MB_CURL_DOWNLOAD_CANCELLED,
 MB_CURL_DOWNLOAD_SINK_FAILED, MB_CURL_DOWNLOAD_SYNC_FAILED,
 MB_CURL_DOWNLOAD_TIMEOUT, MB_CURL_DOWNLOAD_CONSUMER_FAILED
} mb_curl_download_status_v1;

/* A zero return promises exactly 32 fresh bytes from the real platform source.
 * No deterministic source or game data may seed a production runtime. */
typedef int (*mb_curl_entropy_read32_fn_v1)(void *,uint8_t [32]);
typedef struct mb_curl_runtime_params_v1 {
 uint32_t struct_size,abi_version;
 void *entropy_context;
 mb_curl_entropy_read32_fn_v1 entropy_read32;
} mb_curl_runtime_params_v1;
typedef struct mb_curl_runtime_result_v1 {
 uint32_t struct_size,abi_version;
 uint32_t initialized,terminal,window_active,window_expired;
 uint64_t entropy_windows,entropy_callbacks,entropy_bytes,clock_samples;
} mb_curl_runtime_result_v1;
typedef int (*mb_curl_cancel_fn_v1)(void *,uint64_t received);

#ifdef __cplusplus
extern "C" {
#endif

/* One serialized process owner, with exclusive curl and PSA ownership. Start
 * once, perform sequential operations with fresh handles/deadlines/entropy
 * budgets, close once. Source and invalid/backward clock faults are terminal;
 * HTTP/TLS rejection and an ordinary job timeout do not reset that owner.
 * There is no reset/rebind/retry API. Calls and platform hooks must not race. */
int mb_curl_runtime_start_v1(const mb_curl_runtime_params_v1 *,mb_curl_runtime_result_v1 *);
int mb_curl_runtime_status_v1(mb_curl_runtime_result_v1 *);
int mb_curl_runtime_close_v1(mb_curl_runtime_result_v1 *);

#ifdef __cplusplus
}
#endif
#endif
