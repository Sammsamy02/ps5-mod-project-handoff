#include "CodeSignatureProfile.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <cstdio>
#include <cassert>
struct Ops {
 const char* image;unsigned opens=0,reads=0,closes=0;
 int open(const char*){++opens;return ::open(image,O_RDONLY);}
 int64_t pread(int fd,void*p,size_t n,uint64_t off){++reads;return ::pread(fd,p,n,static_cast<off_t>(off));}
 int close(int fd){++closes;return ::close(fd);}
 uint64_t now_ms(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
};
int main(int argc,char**argv){if(argc!=2)return 2;Ops ops{argv[1]};rc::codecache::Cache cache;const auto started=ops.now_ms();
 const auto r=cache.prepare(rc::codeprofile::kProfile,0x400000,0x400000,0xc0602c8,1,ops);
 if(!r.ok){std::printf("FAIL reason=%s bytes=%llu errno=%d\n",r.reason,(unsigned long long)r.bytes_read,r.read_error);return 1;}
 assert(cache.metadata.ready&&cache.metadata.count==2&&cache.metadata.size==194241424&&r.bytes_read==194241424&&r.verified_pins==2);
 assert(std::strcmp(cache.metadata.sha256,"3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc")==0);
 const auto*a=cache.prefix(0x63d590,rc::fnameprofile::kPrefix,32);const auto*b=cache.prefix(0x890aa0,rc::codeprofile::kScoPrefix,25);
 assert(a&&b&&a->offset==0x241590&&b->offset==0x494aa0&&a->collected+b->collected==57&&cache.metadata.segments[a->segment].index==0&&cache.metadata.segments[b->segment].index==0);
 assert(ops.opens==1&&ops.closes==1);
 std::printf("PASS production C++ retained-image prepare bytes=%llu pins=%zu cached_bytes=57 offsets=0x241590/0x494aa0 opens=%u closes=%u reads=%u elapsed_ms=%llu timing=Mac-only\n",(unsigned long long)r.bytes_read,r.verified_pins,ops.opens,ops.closes,ops.reads,(unsigned long long)(ops.now_ms()-started));
}
