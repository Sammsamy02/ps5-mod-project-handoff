#include <stddef.h>
struct mb_curl_provider_transfer_params_v1;
struct mb_curl_provider_transfer_result_v1;
extern int mb_curl_provider_transfer_v1(const struct mb_curl_provider_transfer_params_v1 *, struct mb_curl_provider_transfer_result_v1 *);
int main(void) { return mb_curl_provider_transfer_v1(NULL, NULL) == 1 ? 0 : 1; }
