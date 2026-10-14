#include "provider_transfer.h"
/* Explicit RED-only compatibility shim ignores POST body and invokes old GET. */
int mb_curl_provider_post_transfer_v1(const mb_curl_provider_post_transfer_params_v1 *p,mb_curl_provider_transfer_result_v1 *r){return mb_curl_provider_transfer_v1(&p->provider,r);}
