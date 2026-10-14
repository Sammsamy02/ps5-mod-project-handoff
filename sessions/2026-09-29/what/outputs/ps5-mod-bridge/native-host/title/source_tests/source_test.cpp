#include "source_test.h"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <string>
#include <fcntl.h>
#include <unistd.h>

static size_t allocations,fail_allocation;
void *operator new(size_t n) {
    ++allocations;
    if(fail_allocation && allocations==fail_allocation) throw std::bad_alloc();
    if(void *p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void *operator new[](size_t n) {return ::operator new(n);}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete[](void *p) noexcept {std::free(p);}

static unsigned checks,failures,serial;
#define CHECK(x) do {++checks; if(!(x)) {++failures; std::printf("FAIL %u: %s\n",__LINE__,#x);}} while(0)
#define REQUIRE(x) do {++checks; if(!(x)) {++failures; std::printf("FAIL %u: %s\n",__LINE__,#x); return;}} while(0)
namespace fs=std::filesystem;
static fs::path base;
static const std::string id="Any_Custom.Title-42";
static const std::string good="{\"titleId\":\"Any_Custom.Title-42\",\"contentVersion\":\"observed.7\","
    "\"localizedParameters\":{\"defaultLanguage\":\"x-fixture\",\"x-fixture\":{\"titleName\":\"作品\"}}}";
enum Fault {NONE,SHORT,READ_ERROR,GROW,MUTATE_STAT,MUTATE_FILE,REPLACE_FILE,REPLACE_CANDIDATE,REPLACE_ROOT,OPEN_ERROR,CLOSE_ERROR,OVERSUPPLY,FIFO_RACE};
struct Trace {
    unsigned calls=0,opens=0,closes=0,reads=0,live=0;
    uint64_t requested=0,actual=0;
    int file_fd=-1; Fault fault=NONE; bool fired=false,safe=true;
    std::string file,candidate,root;
};
static bool safe_flags(int flags,bool dir) {
    return (flags&O_ACCMODE)==O_RDONLY && !(flags&(O_CREAT|O_TRUNC|O_APPEND)) &&
        (flags&O_NOFOLLOW) && (flags&O_CLOEXEC) && (dir?(flags&O_DIRECTORY):(flags&O_NONBLOCK));
}
static int t_lstat(void *v,const char *p,struct stat *s) {auto &t=*static_cast<Trace*>(v); ++t.calls; return ::lstat(p,s);}
static int t_open(void *v,const char *p,int f) {
    auto &t=*static_cast<Trace*>(v); ++t.calls; ++t.opens; t.safe &=safe_flags(f,true);
    const int fd=::open(p,f); if(fd>=0) ++t.live; return fd;
}
static int t_atstat(void *v,int d,const char *p,struct stat *s,int f) {
    auto &t=*static_cast<Trace*>(v); ++t.calls; t.safe &=f==AT_SYMLINK_NOFOLLOW;
    return ::fstatat(d,p,s,f);
}
static int t_atopen(void *v,int d,const char *p,int f) {
    auto &t=*static_cast<Trace*>(v); ++t.calls; ++t.opens;
    const bool file=!std::strcmp(p,"param.json"); t.safe &=safe_flags(f,!file);
    if(file && t.fault==OPEN_ERROR) {errno=EACCES; return -1;}
    if(file && t.fault==FIFO_RACE) {
        if(::unlink(t.file.c_str()) || ::mkfifo(t.file.c_str(),0600))std::abort();
    }
    const int fd=::openat(d,p,f); if(fd>=0) {++t.live; if(file)t.file_fd=fd;} return fd;
}
static int t_fstat(void *v,int d,struct stat *s) {
    auto &t=*static_cast<Trace*>(v); ++t.calls;
    const int rc=::fstat(d,s);
    if(!rc && d==t.file_fd && t.fault==MUTATE_STAT && t.reads) {
#if defined(__APPLE__)
        ++s->st_mtimespec.tv_nsec;
#else
        ++s->st_mtim.tv_nsec;
#endif
    }
    return rc;
}
static ssize_t t_read(void *v,int d,void *p,size_t n) {
    auto &t=*static_cast<Trace*>(v); ++t.calls; ++t.reads; t.requested+=n;
    t.safe &=n && n<=MB_TITLE_SOURCE_READ_REQUEST_LIMIT;
    if(t.fault==READ_ERROR) {errno=EIO; return -1;}
    if(t.fault==OVERSUPPLY) return static_cast<ssize_t>(n+1); // no excess buffer write
    const size_t request=t.fault==SHORT && !t.fired && n>1 ? n-1:n;
    const ssize_t rc=::read(d,p,request);
    if(rc>0)t.actual+=static_cast<uint64_t>(rc);
    if(!t.fired && rc>0 && t.fault!=CLOSE_ERROR) {
        t.fired=true;
        if(t.fault==GROW) {
            const int w=::open(t.file.c_str(),O_WRONLY|O_APPEND);
            if(w<0 || ::write(w," ",1)!=1 || ::close(w)) std::abort();
        } else if(t.fault==MUTATE_FILE) {
            const int w=::open(t.file.c_str(),O_WRONLY); struct stat before;
            if(w<0 || ::fstat(w,&before) || ::pwrite(w," ",1,static_cast<off_t>(good.size()-1))!=1)std::abort();
#if defined(__APPLE__)
            struct timespec times[2]={before.st_atimespec,before.st_mtimespec};
#else
            struct timespec times[2]={before.st_atim,before.st_mtim};
#endif
            ++times[1].tv_sec;
            if(::futimens(w,times) || ::close(w))std::abort();
        } else if(t.fault==REPLACE_FILE) {
            const std::string replacement=t.file+".owned-replacement";
            std::ofstream f(replacement); f<<good; f.close();
            if(::rename(replacement.c_str(),t.file.c_str())) std::abort();
        } else if(t.fault==REPLACE_CANDIDATE || t.fault==REPLACE_ROOT) {
            const std::string &path=t.fault==REPLACE_ROOT?t.root:t.candidate;
            if(::rename(path.c_str(),(path+".old").c_str()) || ::mkdir(path.c_str(),0700)) std::abort();
        }
    }
    return rc;
}
static int t_close(void *v,int d) {
    auto &t=*static_cast<Trace*>(v); ++t.calls; ++t.closes; --t.live;
    const int rc=::close(d);
    if(t.fault==CLOSE_ERROR && !t.fired) {t.fired=true; errno=EIO; return -1;}
    return rc;
}
struct Fixture {
    fs::path root; std::string root_text;
    Trace trace;
    mb_title_source_test_ops_v1 ops{sizeof(ops),1,&trace,t_lstat,t_open,t_atstat,t_atopen,t_fstat,t_read,t_close};
    mb_title_source_request_v1 request{sizeof(request),1,MB_TITLE_CACHE_USER,static_cast<uint32_t>(id.size()),id.data(),{0,0}};
    mb_title_source_budget_v1 budget{sizeof(budget),1,128,0,65537,0,{0,0}};
    mb_title_source_result_v1 result{};
    std::array<uint8_t,65536> workspace{};
    Fixture() {
        root=base/("case-"+std::to_string(++serial)); fs::create_directories(root/id);
        root_text=root.string(); trace.root=root_text; trace.candidate=(root/id).string(); trace.file=(root/id/"param.json").string();
        write(good);
    }
    void write(const std::string &s) {std::ofstream f(root/id/"param.json",std::ios::binary); f<<s; if(!f)std::abort();}
    mb_title_source_status capture() {
        std::memset(&result,0xa5,sizeof(result)); workspace.fill(0xa5);
        return mb_title_source_capture_fixture_v1(&request,&budget,workspace.data(),workspace.size(),&result,
            root_text.data(),root_text.size(),&ops);
    }
};
static bool zero_workspace(const Fixture &f) {for(uint8_t b:f.workspace)if(b)return false; return true;}
static void cleaned(const Fixture &f) {
    CHECK(f.result.cleanup_complete && f.trace.live==0);
    CHECK(f.result.operations_attempted==f.trace.calls && f.result.requested_read_bytes==f.trace.requested);
    CHECK(f.result.operations_reserved<=128 && f.result.requested_read_bytes<=65537 && f.trace.safe);
    CHECK(zero_workspace(f));
}
static void no_authority(const mb_title_metadata_result_v1 &m) {
    CHECK(!m.installed_observed && !m.running_observed && !m.build_fingerprint_available);
    CHECK(!m.engine_acceptance_verified && !m.runtime_supported && !m.installation_supported);
    CHECK(!m.authenticity_verified && !m.game_acceptance_verified && !m.reserved[0] && !m.reserved[1]);
}
static void no_publication(const Fixture &f) {
    const mb_title_metadata_result_v1 empty{};
    CHECK(!std::memcmp(&empty,&f.result.metadata,sizeof(empty)));
    CHECK(!f.result.source_complete && !f.result.identity_checked && !f.result.candidate_matches);
}
static void failure(Fixture &f,mb_title_source_status status,mb_title_source_reason reason) {
    CHECK(f.capture()==status); CHECK(f.result.status==static_cast<uint32_t>(status) && f.result.reason==static_cast<uint32_t>(reason));
    no_publication(f); cleaned(f);
}
static void complete_and_binding() {
    Fixture f; REQUIRE(f.capture()==MB_TITLE_SOURCE_OK);
    CHECK(f.result.source_scope==MB_TITLE_SOURCE_HOST_FIXTURE_SCOPE && f.result.requested_profile==MB_TITLE_CACHE_USER);
    CHECK(f.result.source_complete && f.result.identity_checked && f.result.candidate_matches);
    CHECK(f.result.metadata.metadata_complete && !std::strcmp(f.result.metadata.title_id,id.c_str()));
    CHECK(f.result.metadata.name_available && !std::strcmp(f.result.metadata.name,"作品"));
    CHECK(f.result.metadata.version_available && !std::strcmp(f.result.metadata.version,"observed.7"));
    CHECK(f.result.actual_read_bytes==good.size() && f.result.requested_read_bytes==good.size()+1);
    CHECK(f.result.operations_reserved==f.result.operations_attempted);
    no_authority(f.result.metadata); cleaned(f);
    std::printf("SOURCE_METADATA_SHA256 %s\n",f.result.metadata.metadata_sha256);
    Fixture wrong; wrong.write("{\"titleId\":\"Unrelated_1\"}");
    failure(wrong,MB_TITLE_SOURCE_ID_MISMATCH,MB_TITLE_SOURCE_CANDIDATE_MISMATCH);
    Fixture malformed; malformed.write("{\"titleId\":");
    failure(malformed,MB_TITLE_SOURCE_METADATA_REJECTED,MB_TITLE_SOURCE_DECODE_FAILED);
    CHECK(malformed.result.metadata_status==MB_TITLE_METADATA_MALFORMED);
    Fixture small; small.write("{\"titleId\":\"Any_Custom.Title-42\"}"); CHECK(small.capture()==MB_TITLE_SOURCE_OK);
    CHECK(!small.result.metadata.name_available && !small.result.metadata.version_available); no_authority(small.result.metadata); cleaned(small);
}
static void unsafe_paths_and_size() {
    Fixture baseline; REQUIRE(baseline.capture()==MB_TITLE_SOURCE_OK);
    Fixture filelink; fs::remove(filelink.trace.file); fs::create_symlink(baseline.trace.file,filelink.trace.file);
    failure(filelink,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_SYMLINK); CHECK(filelink.trace.reads==0);
    Fixture dirlink; fs::remove_all(dirlink.root/id); fs::create_directory_symlink(baseline.root/id,dirlink.root/id);
    failure(dirlink,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_SYMLINK); CHECK(dirlink.trace.reads==0);
    Fixture rootlink; fs::remove_all(rootlink.root); fs::create_directory_symlink(baseline.root,rootlink.root);
    failure(rootlink,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_SYMLINK);
    Fixture ancestor; fs::remove_all(ancestor.root); fs::create_directory_symlink(baseline.root,ancestor.root);
    ancestor.root_text+="/nested";
    failure(ancestor,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_SYMLINK);
    Fixture fifo; fs::remove(fifo.trace.file); REQUIRE(::mkfifo(fifo.trace.file.c_str(),0600)==0);
    failure(fifo,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_NOT_REGULAR); CHECK(fifo.trace.reads==0 && fifo.trace.file_fd<0);
    Fixture race; race.trace.fault=FIFO_RACE;
    failure(race,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_NOT_REGULAR); CHECK(race.trace.reads==0 && race.trace.file_fd>=0);
    Fixture directory; fs::remove(directory.trace.file); fs::create_directory(directory.trace.file);
    failure(directory,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_NOT_REGULAR);
    Fixture absent; fs::remove(absent.trace.file);
    failure(absent,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_STAT_FAILED); CHECK(absent.result.last_errno==ENOENT);
    Fixture large; large.write(good+std::string(65537-good.size(),' '));
    failure(large,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_FILE_TOO_LARGE); CHECK(large.trace.reads==0);
    Fixture exact; exact.write(good+std::string(65536-good.size(),' '));
    CHECK(exact.capture()==MB_TITLE_SOURCE_OK && exact.result.actual_read_bytes==65536);
    CHECK(exact.trace.reads==17 && exact.result.requested_read_bytes==65537); cleaned(exact);
}
static void partial_and_changed() {
    Fixture baseline; REQUIRE(baseline.capture()==MB_TITLE_SOURCE_OK);
    Fixture shortread; shortread.trace.fault=SHORT;
    failure(shortread,MB_TITLE_SOURCE_SHORT_READ,MB_TITLE_SOURCE_INCOMPLETE_READ);
    CHECK(shortread.result.actual_read_bytes==good.size()-1 && shortread.result.requested_read_bytes==good.size());
    Fixture readerr; readerr.trace.fault=READ_ERROR;
    failure(readerr,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_READ_FAILED);
    CHECK(readerr.result.actual_read_bytes==0 && readerr.result.last_errno==EIO);
    Fixture oversupply; oversupply.trace.fault=OVERSUPPLY;
    failure(oversupply,MB_TITLE_SOURCE_INTERNAL_ERROR,MB_TITLE_SOURCE_INTERNAL_FAILURE);
    CHECK(oversupply.result.actual_read_bytes==0);
    Fixture growth; growth.trace.fault=GROW;
    failure(growth,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_GROWTH_OBSERVED); CHECK(growth.result.actual_read_bytes==good.size()+1);
    Fixture mutation; mutation.trace.fault=MUTATE_STAT;
    failure(mutation,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_IDENTITY_CHANGED);
    Fixture diskmutation; diskmutation.trace.fault=MUTATE_FILE;
    failure(diskmutation,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_IDENTITY_CHANGED);
    Fixture replace; replace.trace.fault=REPLACE_FILE;
    failure(replace,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_IDENTITY_CHANGED);
    Fixture candidate; candidate.trace.fault=REPLACE_CANDIDATE;
    failure(candidate,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_IDENTITY_CHANGED);
    Fixture root; root.trace.fault=REPLACE_ROOT;
    failure(root,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_IDENTITY_CHANGED);
}
static void ledger_and_cleanup() {
    Fixture baseline; REQUIRE(baseline.capture()==MB_TITLE_SOURCE_OK);
    const uint64_t operations=baseline.result.operations_reserved;
    Fixture exact; exact.budget.remaining_operations=operations; exact.budget.remaining_read_bytes=good.size()+1;
    CHECK(exact.capture()==MB_TITLE_SOURCE_OK && !exact.budget.remaining_operations && !exact.budget.remaining_read_bytes);
    cleaned(exact);
    Fixture one_op; one_op.budget.remaining_operations=operations-1;
    failure(one_op,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_OPERATION_BUDGET); CHECK(one_op.trace.closes==one_op.trace.opens);
    Fixture one_byte; one_byte.budget.remaining_read_bytes=good.size();
    failure(one_byte,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_READ_BUDGET); CHECK(one_byte.trace.closes==one_byte.trace.opens);
    CHECK(one_byte.result.actual_read_bytes==good.size() && one_byte.result.requested_read_bytes==good.size());
    Fixture no_ops; no_ops.budget.remaining_operations=0;
    failure(no_ops,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_OPERATION_BUDGET); CHECK(no_ops.trace.calls==0);
    Fixture openerr; openerr.trace.fault=OPEN_ERROR;
    failure(openerr,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_OPEN_FAILED);
    CHECK(openerr.result.operations_reserved==openerr.result.operations_attempted+1 && openerr.result.last_errno==EACCES);
    Fixture closeerr; closeerr.trace.fault=CLOSE_ERROR;
    CHECK(closeerr.capture()==MB_TITLE_SOURCE_CLEANUP_FAILED && closeerr.result.reason==MB_TITLE_SOURCE_CLOSE_FAILED);
    CHECK(!closeerr.result.cleanup_complete && closeerr.trace.live==0 && closeerr.trace.closes==closeerr.trace.opens);
    no_publication(closeerr); CHECK(zero_workspace(closeerr));
    Fixture shared; shared.budget.remaining_operations=operations*2; shared.budget.remaining_read_bytes=good.size()*2+2;
    CHECK(shared.capture()==MB_TITLE_SOURCE_OK); const uint64_t first_ops=shared.budget.reserved_operations;
    shared.trace=Trace{};
    CHECK(shared.capture()==MB_TITLE_SOURCE_OK); CHECK(shared.budget.reserved_operations==first_ops*2);
    CHECK(shared.result.operations_reserved==first_ops && !shared.budget.remaining_read_bytes); cleaned(shared);
    shared.trace=Trace{};
    failure(shared,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_OPERATION_BUDGET);
    CHECK(shared.budget.reserved_operations==first_ops*2 && !shared.result.operations_reserved);
    std::printf("SOURCE_LEDGER baseline_operations=%llu actual_attempts=%u cleanup_calls=%u read_bytes=%llu\n",
        static_cast<unsigned long long>(operations),baseline.result.operations_attempted,baseline.trace.closes,
        static_cast<unsigned long long>(baseline.result.requested_read_bytes));
}
static void preflight_no_write() {
    Fixture baseline; REQUIRE(baseline.capture()==MB_TITLE_SOURCE_OK);
    const auto bad=[&](Fixture &f,const char *root,size_t root_bytes,uint8_t *work,size_t cap,mb_title_source_result_v1 *out) {
        std::memset(&f.result,0xa5,sizeof(f.result)); f.workspace.fill(0xa5);
        const auto old_result=f.result; const auto old_budget=f.budget;
        CHECK(mb_title_source_capture_fixture_v1(&f.request,&f.budget,work,cap,out,root,root_bytes,&f.ops)==MB_TITLE_SOURCE_BAD_ARGUMENT);
        CHECK(!std::memcmp(&old_result,&f.result,sizeof(f.result)) && !std::memcmp(&old_budget,&f.budget,sizeof(f.budget)));
        CHECK(f.workspace[0]==0xa5 && f.workspace.back()==0xa5 && !f.trace.calls);
    };
    Fixture abi; abi.request.abi_version=2; bad(abi,abi.root_text.data(),abi.root_text.size(),abi.workspace.data(),65536,&abi.result);
    Fixture reserved; reserved.request.reserved[0]=1; bad(reserved,reserved.root_text.data(),reserved.root_text.size(),reserved.workspace.data(),65536,&reserved.result);
    for(const std::string &candidate:std::array<std::string,7>{"", ".hidden","..","space id","slash/id","utf8-é",std::string(65,'a')}) {
        Fixture f; f.request.candidate=candidate.data(); f.request.candidate_bytes=static_cast<uint32_t>(candidate.size());
        bad(f,f.root_text.data(),f.root_text.size(),f.workspace.data(),65536,&f.result);
    }
    Fixture ledger; ledger.budget.remaining_operations=UINT64_MAX; ledger.budget.reserved_operations=1;
    bad(ledger,ledger.root_text.data(),ledger.root_text.size(),ledger.workspace.data(),65536,&ledger.result);
    Fixture byteledger; byteledger.budget.remaining_read_bytes=UINT64_MAX; byteledger.budget.reserved_read_bytes=1;
    bad(byteledger,byteledger.root_text.data(),byteledger.root_text.size(),byteledger.workspace.data(),65536,&byteledger.result);
    Fixture root; const char dotroot[]="/fixture/../escape"; bad(root,dotroot,sizeof(dotroot)-1,root.workspace.data(),65536,&root.result);
    Fixture cap; bad(cap,cap.root_text.data(),cap.root_text.size(),cap.workspace.data(),65535,&cap.result);
    Fixture alias; bad(alias,alias.root_text.data(),alias.root_text.size(),reinterpret_cast<uint8_t*>(&alias.result),65536,&alias.result);
    Fixture output_alias; bad(output_alias,output_alias.root_text.data(),output_alias.root_text.size(),output_alias.workspace.data(),65536,
        reinterpret_cast<mb_title_source_result_v1*>(output_alias.workspace.data()));
    Fixture ops; ops.ops.read_fd=nullptr; bad(ops,ops.root_text.data(),ops.root_text.size(),ops.workspace.data(),65536,&ops.result);
}
static void allocation_failure() {
    Fixture f; allocations=0; REQUIRE(f.capture()==MB_TITLE_SOURCE_OK); const size_t baseline=allocations;
    REQUIRE(baseline>0);
    Fixture failed; allocations=0; fail_allocation=1;
    const auto rc=failed.capture(); fail_allocation=0;
    CHECK(rc==MB_TITLE_SOURCE_METADATA_REJECTED && failed.result.metadata_status==MB_TITLE_METADATA_ALLOCATION_FAILED);
    no_publication(failed); cleaned(failed);
    Fixture recovery; CHECK(recovery.capture()==MB_TITLE_SOURCE_OK); cleaned(recovery);
    std::printf("SOURCE_ALLOCATOR PASS allocation_failure_contained=true cleanup_complete=true\n");
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    base=fs::canonical(argv[1]);
    const struct {const char *name; void (*run)();} groups[]={
        {"complete_and_candidate_binding",complete_and_binding},{"unsafe_paths_and_size",unsafe_paths_and_size},
        {"partial_and_changed",partial_and_changed},{"shared_ledger_and_cleanup",ledger_and_cleanup},
        {"preflight_no_write",preflight_no_write},{"allocation_failure",allocation_failure}};
    for(const auto &g:groups) {const unsigned prior=failures; g.run(); std::printf("GROUP %s %s\n",g.name,failures==prior?"PASS":"FAIL");}
    std::printf("SOURCE_TEST %s checks=%u failures=%u\n",failures?"FAIL":"PASS",checks,failures);
    return failures?1:0;
}
