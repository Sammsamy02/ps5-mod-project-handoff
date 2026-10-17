#include "runtime.h"
#include "owner.h"
#include <mbedtls/x509_crt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
static int fresh(void *context,uint8_t bytes[32]) {(void)context;return getentropy(bytes,32);}
int main(int argc,char **argv) {
 FILE *file;long count;size_t got;uint8_t *pem;int start,begin,parsed,end,close;
 mb_curl_runtime_params_v1 p={sizeof(p),1,NULL,fresh};mb_curl_runtime_result_v1 state;
 mbedtls_x509_crt certificate;
 if(argc!=2)return 10;
 file=fopen(argv[1],"rb");if(!file)return 11;
 if(fseek(file,0,SEEK_END))return 12;count=ftell(file);
 if(count<=0||count>1048576||fseek(file,0,SEEK_SET))return 13;
 pem=malloc((size_t)count+1);if(!pem)return 14;
 got=fread(pem,1,(size_t)count,file);fclose(file);if(got!=(size_t)count)return 15;
 pem[got]=0;
 start=mb_curl_runtime_start_v1(&p,&state);if(start)return 16;
 begin=mb_curl_owner_window_begin_v1(5000);if(begin)return 17;
 mbedtls_x509_crt_init(&certificate);
 parsed=mbedtls_x509_crt_parse(&certificate,pem,got+1);
 mbedtls_x509_crt_free(&certificate);
 end=mb_curl_owner_window_end_v1();close=mb_curl_runtime_close_v1(&state);
 free(pem);
 printf("{\"parse_return\":%d,\"error_hex\":\"0x%04x\",\"pem_bytes\":%zu,\"start\":%d,\"window_end\":%d,\"close\":%d,\"terminal\":%u}\n",parsed,(unsigned)(parsed<0?-parsed:parsed),got,start,end,close,state.terminal);
 return end||close||state.terminal?1:0;
}
