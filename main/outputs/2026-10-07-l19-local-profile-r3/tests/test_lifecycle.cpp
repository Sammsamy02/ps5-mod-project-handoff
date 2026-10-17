// New lifetime/thread regressions exercise the real extracted bridge and
// selected RuntimeOwner, with Sony calls adapting to real host descriptors.
#include "kernel_adapters.inc"
#include "runtime_owner.hpp"
extern "C" int mfa_status_open(mfa_status*,const char*,long,uint64_t){return -1;}
extern "C" int mfa_status_write(mfa_status*,const char*,int,const char*){return -1;}
extern "C" void mfa_status_close(mfa_status*){}
namespace modbridge::native {
#include "supplier_stop.inc"
static int stop_mode=0;
static bool StopRuntimeWithAck(std::unique_ptr<RuntimeOwner>& owner){
    if(stop_mode==1)throw 7;
    bool stopped=RealStopRuntimeWithAck(owner);return stop_mode==2?false:stopped;
}
}
static std::unique_ptr<modbridge::native::RuntimeOwner>& owner_slot(){
    static std::unique_ptr<modbridge::native::RuntimeOwner> owner;return owner;
}
static constexpr int MRS_ERR_STOP_EXN=5;
static std::atomic_bool* fallback_done=nullptr;
static int premature_fallbacks=0;
static void observed_fallback_release(){if(fallback_done&&!fallback_done->load())++premature_fallbacks;bridge_copyin_directory_release();}
#include "stop_extract.inc"
static void complete(){bridge_copyin_initializer_complete();}
static void leave(){bridge_copyin_initializer_leave();}
static void explicit_d(uint32_t b_code){
#include "control_d_extract.inc"
}
static size_t sink_files(){size_t n=0;for(const auto& p:std::filesystem::directory_iterator(dir))if(p.path().filename().string().find("modbridge-copyin-p")==0)++n;return n;}
static void clean_files(){for(const auto& p:std::filesystem::directory_iterator(dir))std::filesystem::remove(p.path());}
struct WorkerState {
    std::atomic_bool ready{false},finish_checks{false},checks_done{false},stop{false},done{false};
    bool successful=false,checks_cleanup=false,ledger_retained=false;
    int mode=0;
};
struct Backend final:modbridge::native::RuntimeBackend {
    std::shared_ptr<WorkerState> s;
    explicit Backend(std::shared_ptr<WorkerState> value):s(std::move(value)){}
    void initialize(std::function<void()> ready) override {
        struct DoneScope{WorkerState& state;~DoneScope(){state.done.store(true);}} done{*s};
        s->successful=bridge_kernel_write_copyin_probe(guest,32)==1;
        // A fault remains relevant after check completion and FD release.
        scripted_writes={int64_t(int32_t(0x8002000eu))};write_calls=0;
        s->ledger_retained=bridge_kernel_write_copyin_probe(guest+0x1000,32)==0;
        s->ready.store(true);ready();
        while(!s->finish_checks.load())std::this_thread::yield();
        complete();s->checks_cleanup=live_fds.empty();
        s->ledger_retained=s->ledger_retained&&bridge_copyin_range_faulted(guest+0x1000,32)==1;
        s->checks_done.store(true);
        while(!s->stop.load())std::this_thread::yield();
        if(s->mode==3)throw std::runtime_error("initializer failed after checks");
    }
    void request_stop() noexcept override{s->finish_checks.store(true);s->stop.store(true);}
};
static void wait_for(std::atomic_bool& flag){while(!flag.load())std::this_thread::yield();}
int main(){
    char temp[]="/tmp/l17-lifecycle-XXXXXX";dir=mkdtemp(temp);for(size_t i=0;i<sizeof(backing);++i)backing[i]=uint8_t(i);
    reset();check(unlink_calls==64&&sink_files()==0,"each unique startup sink is unlinked immediately after its open");
    check(bridge_kernel_write_copyin_probe(guest,32)==1,"unlinked sink descriptor remains usable for fresh copyin and pread");
    std::set<uint32_t> acquired;for(const auto& e:events)if(e.first=="copyin_startup_fd")acquired.insert(e.second);
    check(acquired.size()==64,"startup acquisition diagnostics record all 64 retained descriptor IDs");
    complete();check(live_fds.empty(),"initializer completion closes all unused bank descriptors before runtime shutdown");
    reset();bridge_copyin_directory_release();restored=false;unlink_error=int32_t(0x80020001u);bridge_copyin_directory_prepare();restored=true;
    bridge_copyin_initializer_enter();
    check(unlink_calls==128&&sink_files()==64&&event("copyin_file_op")==5&&event("copyin_file_errno")==EPERM,"denied unlink logs raw error and retains every owned file");
    check(bridge_kernel_write_copyin_probe(guest,32)==1,"denied unlink retains usable descriptor without reopen");
    complete();clean_files();
    for(int position:{1,2,31,64})for(int raw:{-1,int32_t(0x80020001u),-EMFILE}){
        reset();bridge_copyin_directory_release();restored=false;create_calls=0;create_error=raw;create_errno=EACCES;fail_create_at=position;
        bridge_copyin_directory_prepare();restored=true;
        check(live_fds.empty()&&create_calls==position&&event("copyin_startup_ordinal")==uint32_t(position)&&event("copyin_file_errno")==uint32_t(raw==-1?EACCES:raw==-EMFILE?EMFILE:EPERM),"acquisition error at requested position logs ordinal and closes partial bank");
    }
    reset();bridge_copyin_directory_release();restored=false;parent_error=-1;bridge_copyin_directory_prepare();restored=true;
    check(live_fds.empty()&&event("pool_copyin_parent_errno")==EACCES,"parent acquisition failure owns no descriptors");
    reset();const int consumed=g_copyin_handles[0].fd;recycle_target=consumed;close_error=1;
    check(bridge_kernel_write_copyin_probe(guest,32)==-1&&sentinel_fd==consumed,"close error test reuses consumed FD number with an unrelated sentinel");
    close_error=0;complete();complete();leave();modbridge_runtime_stop();
    check(sentinel_fd>=0&&::fcntl(sentinel_fd,F_GETFD)!=-1&&closes[consumed]==1,"completion leave and repeated stop never close or retry reused consumed FD");
    if(sentinel_fd>=0)::close(sentinel_fd);sentinel_fd=-1;
    reset();const int unused=g_copyin_handles[0].fd;recycle_target=unused;close_error=1;complete();close_error=0;complete();leave();modbridge_runtime_stop();
    check(sentinel_fd==unused&&::fcntl(sentinel_fd,F_GETFD)!=-1&&closes[unused]==1,"bank cleanup consumes FD before failed close and never retries reused number");
    if(sentinel_fd>=0)::close(sentinel_fd);sentinel_fd=-1;
    reset();scripted_writes={int64_t(int32_t(0x80020001u)),0};
    check(bridge_kernel_write_copyin_probe(guest,32)==-1&&bridge_kernel_write_copyin_probe(guest,32)==-1&&bridge_kernel_write_copyin_probe(guest,32)==1&&std::set<int>(write_fds.begin(),write_fds.end()).size()==3,"unavailable attempts consume distinct handles and later fresh retry can recover");
    reset();scripted_writes.assign(64,int64_t(int32_t(0x80020001u)));bool all_refuse=true;
    for(int i=0;i<64;++i)all_refuse=all_refuse&&bridge_kernel_write_copyin_probe(guest,32)==-1;
    check(all_refuse&&bridge_kernel_write_copyin_probe(guest,32)==-1&&write_calls==64&&std::set<int>(write_fds.begin(),write_fds.end()).size()==64,"64 unavailable retries exhaust bank without recycle or late acquisition");
    reset();check(bridge_kernel_write_copyin_probe(guest,32)==1,"owner establishes fresh capture before foreign-call checks");
    const auto nonce=g_copyin_probe_nonce;const auto captured_slot=g_write_copyin_capture_next;const auto fault_count=g_copyin_faults.count;const auto event_count=events.size();
    int foreign_probe=0,foreign_fault=0,foreign_capture=1;const uint8_t* foreign_bytes=backing;uint32_t foreign_len=99;
    std::thread other([&]{foreign_probe=bridge_kernel_write_copyin_probe(guest,32);foreign_capture=bridge_write_copyin_capture(guest,&foreign_bytes,&foreign_len);foreign_fault=bridge_copyin_range_faulted(guest,32);complete();});other.join();
    check(foreign_probe==-1&&foreign_capture==0&&foreign_bytes==nullptr&&foreign_len==0&&foreign_fault==1&&g_copyin_probe_nonce==nonce&&g_write_copyin_capture_next==captured_slot&&g_copyin_faults.count==fault_count&&events.size()==event_count&&live_fds.size()==64,"foreign probe capture fault query and completion refuse before unsynchronized state or telemetry");
    const auto before_d=events.size();std::thread foreign_d([]{explicit_d(1);});foreign_d.join();
    check(events.size()==before_d&&g_copyin_probe_nonce==nonce,"explicit Control D foreign refusal reads no unsynchronized telemetry and emits no stale mirror");
    complete();leave();
    for(int mode=0;mode<4;++mode){
        reset();bridge_copyin_directory_release();restored=false;bridge_copyin_directory_prepare();restored=true;
        auto state=std::make_shared<WorkerState>();state->mode=mode;
        fallback_done=&state->done;premature_fallbacks=0;
        owner_slot()=std::make_unique<modbridge::native::RuntimeOwner>(std::make_unique<Backend>(state));owner_slot()->start();wait_for(state->ready);
        const auto before=g_copyin_probe_nonce;int refused=bridge_kernel_write_copyin_probe(guest,32);
        check(state->successful&&refused==-1&&g_copyin_probe_nonce==before,"actual RuntimeOwner publishes initializer ownership; startup thread cannot probe");
        state->finish_checks.store(true);wait_for(state->checks_done);
        check(state->checks_cleanup&&state->ledger_retained&&live_fds.empty(),"actual worker completion promptly releases bank and retains its fault ledger");
        modbridge::native::stop_mode=mode<3?mode:0;int result=modbridge_runtime_stop();
        check(state->done.load()&&!owner_slot()&&live_fds.empty()&&premature_fallbacks==0&&result==(mode==1?5:mode==2?1:0),"supported actual-owner stop or exceptional destruction joins before fallbackrelease");
        fallback_done=nullptr;
        check(bridge_kernel_write_copyin_probe(guest,32)==-1,"initializer ownership is revoked on actual worker return or exception");
    }
    // Wrong callers overlap fresh owner work under TSan. They may only read
    // the synchronized owner gate; no diagnostic vector or bank access.
    reset();std::atomic_bool strangers_done{false},stranger_started{false};std::atomic_int refused{0};
    bool owned=bridge_copyin_initializer_enter()==1;
    std::thread stranger([&]{while(!strangers_done.load()){const uint8_t* p=nullptr;uint32_t n=0;if(bridge_kernel_write_copyin_probe(guest,32)==-1&&bridge_write_copyin_capture(guest,&p,&n)==0&&bridge_copyin_range_faulted(guest,32)==1)++refused;stranger_started.store(true);}});
    wait_for(stranger_started);
    if(owned){for(int i=0;i<64;++i)bridge_kernel_write_copyin_probe(guest,32);} // red baseline has no synchronized gate
    strangers_done.store(true);stranger.join();complete();leave();
    check(owned&&refused.load()>0&&live_fds.empty(),"concurrent foreign-call stress preserves supported initializer lifecycle");
    bridge_copyin_directory_release();clean_files();std::filesystem::remove(dir);std::printf("failures=%d\n",failures);return failures?1:0;
}
