/* Private platform-boundary controls. Link only an owner.c copy compiled with
 * time/clock_gettime renamed to these functions; never change the OS clock. */
#define _POSIX_C_SOURCE 200809L
#include "../owner.h"
#include "entropy_poll.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int64_t ticks=0,wall_offset=0;
static unsigned source_calls=0,source_refused=0;
int owner_fixture_clock_gettime(clockid_t clock,struct timespec *out) {
 if(clock!=CLOCK_MONOTONIC)return -1;
 ticks+=1000000;out->tv_sec=ticks/1000000000;out->tv_nsec=ticks%1000000000;return 0;
}
time_t owner_fixture_time(time_t *out) {
 time_t value=(time_t)(1790906400+ticks/1000000000+wall_offset);
 if(out)*out=value;return value;
}
static int source(void *ctx,uint8_t out[32]) {
 (void)ctx;++source_calls;if(source_refused)return -1;
 memset(out,0x73,32);return 0; /* Deliberate unit-boundary fixture, not TLS seed. */
}
int main(int argc,char **argv) {
 if(argc!=2)return 2;
 mb_curl_runtime_params_v1 params={sizeof(params),1,NULL,source};
 if(mb_curl_owner_bind_v1(&params))return 2;
 int passed=0,begin_status=-99;
 if(!strcmp(argv[1],"forward-between-windows")) {
  if(mb_curl_owner_window_begin_v1(1000)||!mb_curl_owner_guard_v1()||mb_curl_owner_window_end_v1())return 2;
  ticks+=10000000000;wall_offset=300;
  int begin=mb_curl_owner_window_begin_v1(1000);
  begin_status=begin;
  passed=begin!=0&&mb_curl_owner_failed_v1();
 } else if(!strcmp(argv[1],"timeout-then-next-window")) {
  if(mb_curl_owner_window_begin_v1(1000))return 2;
  ticks+=2000000000;
  passed=!mb_curl_owner_guard_v1()&&mb_curl_owner_window_expired_v1()&&!mb_curl_owner_failed_v1();
  passed=passed&&!mb_curl_owner_window_end_v1()&&!mb_curl_owner_window_begin_v1(1000)&&mb_curl_owner_guard_v1();
  (void)mb_curl_owner_window_end_v1();
 } else if(!strcmp(argv[1],"source-failure")||!strcmp(argv[1],"outside-window")) {
  unsigned outside=!strcmp(argv[1],"outside-window");
  source_refused=1;
  if(!outside&&mb_curl_owner_window_begin_v1(1000))return 2;
  unsigned char bytes[32];memset(bytes,0xb7,sizeof(bytes));size_t actual=99;
  int rc=mbedtls_hardware_poll(NULL,bytes,sizeof(bytes),&actual);
  unsigned zero=1;for(unsigned i=0;i<32;++i)zero&=bytes[i]==0;
  passed=rc!=0&&actual==0&&zero&&mb_curl_owner_failed_v1()&&source_calls==(outside?0u:1u);
  if(!outside)(void)mb_curl_owner_window_end_v1();
  passed=passed&&mb_curl_owner_window_begin_v1(1000)!=0;
 } else return 2;
 mb_curl_runtime_result_v1 snapshot;mb_curl_owner_snapshot_v1(&snapshot);
 if(snapshot.window_active)(void)mb_curl_owner_window_end_v1();
 mb_curl_owner_close_v1();mb_curl_owner_snapshot_v1(&snapshot);
 printf("{\"case\":\"%s\",\"passed\":%s,\"begin_status\":%d,\"terminal\":%u,\"windows\":%llu,\"source_calls\":%u}\n",argv[1],passed?"true":"false",begin_status,snapshot.terminal,(unsigned long long)snapshot.entropy_windows,source_calls);
 return passed?0:1;
}
