/* An actual C translation unit tests the public ABI and bounded writes. */
#include "core_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void put(unsigned char *b, size_t p, unsigned v) {
    for (unsigned i=0;i<4;++i) b[p+i]=(unsigned char)(v>>(8*i));
}
int main(int argc, char **argv) {
    unsigned char b[163]={0};
    char out[16384]; size_t n=123;
    assert(MB_CORE_INPUT_LIMIT_BYTES==8388608 && MB_CORE_REPORT_CAPACITY==2048);
    if (argc==2 && !strcmp(argv[1],"bounds")) {
        memset(out,'x',sizeof(out));
        assert(mb_core_selftest(out,sizeof(out),NULL)==MB_CORE_INVALID_ARGUMENT && !out[0]);
        assert(mb_core_selftest(NULL,1,&n)==MB_CORE_INVALID_ARGUMENT && !n);
        assert(mb_core_inspect_dds(b,0,0,NULL,1,&n)==MB_CORE_INVALID_ARGUMENT && !n);
        assert(mb_core_inspect_dds(b,0,0,out,sizeof(out),NULL)==MB_CORE_INVALID_ARGUMENT && !out[0]);
        assert(mb_core_inspect_dds(NULL,1,1,out,sizeof(out),&n)==MB_CORE_INVALID_ARGUMENT && !n && !out[0]);
        unsigned char one=0;
        assert(mb_core_inspect_dds(&one,8388609,8388609,out,sizeof(out),&n)==MB_CORE_INPUT_LIMIT && !n && !out[0]);
        unsigned char *limit=calloc(8388608,1); assert(limit);
        assert(mb_core_inspect_dds(limit,8388608,8388608,out,sizeof(out),&n)==MB_CORE_OK && n>0);
        free(limit);
        assert(mb_core_inspect_dds(NULL,0,0,NULL,0,&n)==MB_CORE_BUFFER_TOO_SMALL && n>0);
        size_t need=n; char *guard=malloc(need+3); assert(guard);
        memset(guard,'x',need+3);
        assert(mb_core_inspect_dds(NULL,0,0,guard+1,need,&n)==MB_CORE_BUFFER_TOO_SMALL && n==need);
        assert(guard[0]=='x' && guard[1]==0 && guard[need+1]=='x' && guard[need+2]=='x');
        assert(mb_core_inspect_dds(NULL,0,0,guard+1,need+1,&n)==MB_CORE_OK && n==need);
        assert(strlen(guard+1)==need && guard[0]=='x' && guard[need+1]==0 && guard[need+2]=='x');
        free(guard);
        assert(mb_core_selftest(NULL,0,&n)==MB_CORE_BUFFER_TOO_SMALL && n>0);
        need=n; memset(out,'x',sizeof(out));
        assert(mb_core_selftest(out,need,&n)==MB_CORE_BUFFER_TOO_SMALL && n==need && !out[0] && out[need]=='x');
        assert(mb_core_selftest(out,need+1,&n)==MB_CORE_OK && n==need && strlen(out)==need && out[need+1]=='x');
        return 0;
    }
    if (argc==1 || !strcmp(argv[1],"selftest")) {
        assert(mb_core_selftest(out,sizeof(out),&n)==MB_CORE_OK);
        char previous[MB_CORE_REPORT_CAPACITY]; strcpy(previous,out);
        assert(mb_core_selftest(out,sizeof(out),&n)==MB_CORE_OK && !strcmp(previous,out));
    } else {
        memcpy(b,"DDS ",4); put(b,4,124); put(b,12,7); put(b,16,5); put(b,76,32); put(b,80,4);
        memcpy(b+84,"DXT1",4);
        size_t available=sizeof(b); unsigned long long total=sizeof(b);
        if (!strcmp(argv[1],"empty")) available=total=0;
        else if (!strcmp(argv[1],"invalid")) b[0]='X';
        else if (!strcmp(argv[1],"unsupported")) memcpy(b+84,"ZZZZ",4);
        else if (!strcmp(argv[1],"opaque")) { b[84]=0xff;b[85]=0;b[86]=0x80;b[87]=1; }
        else if (!strcmp(argv[1],"contradiction")) total=1;
        else if (!strcmp(argv[1],"truncated")) available=total=159;
        else if (!strcmp(argv[1],"prefix")) available=128;
        assert(mb_core_inspect_dds(b,available,total,out,sizeof(out),&n)==MB_CORE_OK);
    }
    assert(strlen(out)==n); puts(out); return 0;
}
