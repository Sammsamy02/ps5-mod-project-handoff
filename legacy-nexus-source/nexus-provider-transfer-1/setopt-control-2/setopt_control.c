#include "runtime.h"
#include "owner.h"
#include <curl/curl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/random.h>
static int fresh(void *context,uint8_t bytes[32]) {
 (void)context;return getentropy(bytes,32);
}
int main(void) {
 mb_curl_runtime_params_v1 p={sizeof(p),1,NULL,fresh};mb_curl_runtime_result_v1 state;
 CURL *easy;CURLcode http_get,netrc;int start,begin,end,close;
 start=mb_curl_runtime_start_v1(&p,&state);if(start)return 10;
 begin=mb_curl_owner_window_begin_v1(5000);if(begin)return 11;
 easy=curl_easy_init();if(!easy)return 12;
 http_get=curl_easy_setopt(easy,CURLOPT_HTTPGET,1L);
 netrc=curl_easy_setopt(easy,CURLOPT_NETRC,(long)CURL_NETRC_IGNORED);
 curl_easy_cleanup(easy);end=mb_curl_owner_window_end_v1();
 close=mb_curl_runtime_close_v1(&state);
 printf("{\"http_get\":%d,\"netrc_ignored\":%d,\"start\":%d,\"window_end\":%d,\"close\":%d,\"terminal\":%u,\"initialized\":%u}\n",(int)http_get,(int)netrc,start,end,close,state.terminal,state.initialized);
 return http_get==CURLE_OK&&netrc==CURLE_UNKNOWN_OPTION&&!end&&!close&&!state.terminal&&!state.initialized?0:1;
}
