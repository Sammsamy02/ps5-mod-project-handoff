#ifndef MB_CURL_DOWNLOAD_OWNER_H
#define MB_CURL_DOWNLOAD_OWNER_H
#include "curl_download.h"

/* Private lifecycle around the pinned profile's hardware/time hooks. Bind is
 * process-single-use. A window is valid only while the one synchronous owner
 * runs initialization, a download or final cleanup. Per-window reservations
 * cap entropy calls at 256 / 8192 bytes; lifetime counters never reset. Actual
 * source/clock faults latch terminally. Window deadlines remain local so the
 * next independent download can run after an ordinary timeout. */
int mb_curl_owner_bind_v1(const mb_curl_runtime_params_v1 *);
int mb_curl_owner_window_begin_v1(uint32_t timeout_ms);
int mb_curl_owner_window_end_v1(void);
int mb_curl_owner_guard_v1(void);
int mb_curl_owner_failed_v1(void);
int mb_curl_owner_window_expired_v1(void);
void mb_curl_owner_fail_v1(void);
void mb_curl_owner_close_v1(void);
void mb_curl_owner_snapshot_v1(mb_curl_runtime_result_v1 *);
#endif
