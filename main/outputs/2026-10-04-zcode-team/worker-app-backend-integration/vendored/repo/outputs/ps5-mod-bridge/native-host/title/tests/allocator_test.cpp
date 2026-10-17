#include "metadata.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

static size_t attempts,fail_at;
void *operator new(size_t bytes) {
    ++attempts;
    if(fail_at && attempts==fail_at) throw std::bad_alloc();
    if(void *p=std::malloc(bytes ? bytes : 1)) return p;
    throw std::bad_alloc();
}
void *operator new[](size_t bytes) {return ::operator new(bytes);}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete[](void *p) noexcept {std::free(p);}

static const uint8_t json[]="{\"titleId\":\"Owned_6\",\"contentVersion\":\"next.7\","
    "\"localizedParameters\":{\"defaultLanguage\":\"x-Test\",\"x-Test\":{\"titleName\":\"Owned fixture name\"}}}";
int main() {
    mb_title_metadata_input_v1 input{sizeof(input),1,json,sizeof(json)-1,1,0};
    mb_title_metadata_result_v1 out;
    attempts=0;
    if(mb_title_metadata_decode_v1(&input,&out)!=MB_TITLE_METADATA_OK || !attempts) return 1;
    const size_t total=attempts;
    const size_t points[]={1,total/2 ? total/2 : 1,total};
    for(size_t point:points) {
        attempts=0; fail_at=point;
        const auto rc=mb_title_metadata_decode_v1(&input,&out);
        fail_at=0;
        mb_title_metadata_result_v1 empty;
        std::memset(&empty,0,sizeof(empty)); empty.struct_size=sizeof(empty); empty.abi_version=1;
        empty.status=MB_TITLE_METADATA_ALLOCATION_FAILED; empty.reason=MB_TITLE_METADATA_ALLOCATION_FAILURE;
        if(rc!=MB_TITLE_METADATA_ALLOCATION_FAILED || std::memcmp(&out,&empty,sizeof(out))) return 1;
        std::printf("ALLOCATION_FAILURE attempt=%zu return=%u cleared=true\n",point,(unsigned)rc);
        if(mb_title_metadata_decode_v1(&input,&out)!=MB_TITLE_METADATA_OK || !out.metadata_complete) return 1;
    }
    std::printf("ALLOCATOR PASS baseline_attempts=%zu failure_points=3 recovery=true\n",total);
}
