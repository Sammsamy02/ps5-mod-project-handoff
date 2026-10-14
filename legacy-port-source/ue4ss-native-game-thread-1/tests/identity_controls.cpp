#if defined(IDENTITY_BASELINE)
#include "baseline_identity.hpp"
#else
#include <Unreal/Platform/NativeGameThreadIdentity.hpp>
#endif
#include <atomic>
#include <cstdio>
#include <vector>
#include <thread>
using RC::Unreal::NativeThreadDetail::Identity;
using RC::Unreal::NativeThreadDetail::Publication;
static int checks, failures;
static void check(bool value,const char* name) {
    ++checks;
    if(!value) { ++failures; std::printf("FAIL %s\n",name); }
}
int main() {
    Identity identity;
    check(!identity.initialized(),"unknown identity");
    check(identity.id_raw()==std::thread::id{},"unknown raw ID");
    check(!identity.in_current_raw(),"unknown raw current is false");
    bool threw=false;
    try { (void)identity.id(); } catch(const std::runtime_error&) { threw=true; }
    check(threw,"unknown checked ID throws");
    threw=false;
    try { (void)identity.in_current(); } catch(const std::runtime_error&) { threw=true; }
    check(threw,"unknown checked current throws");

    // Owned host threads only: no claim these are engine or game threads.
    std::atomic_bool start{false}, release{false}, publication_done{false};
    std::atomic_int reader_failures{0}, admitted_actions{0};
    Publication first=Publication::Unavailable, repeat=Publication::Unavailable;
    std::thread publisher([&] {
        while(!start.load(std::memory_order_acquire)) std::this_thread::yield();
        first=identity.publish_current();
        repeat=identity.publish_current();
        if(identity.in_current_raw()) ++admitted_actions;
        publication_done.store(true,std::memory_order_release);
        while(!release.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    const auto publisher_id=publisher.get_id();
    std::vector<std::thread> readers;
    for(int n=0;n<8;++n) readers.emplace_back([&] {
        while(!start.load(std::memory_order_acquire)) std::this_thread::yield();
        for(int i=0;i<2000;++i) {
            const auto value=identity.id_raw();
            if(value!=std::thread::id{} && value!=publisher_id) ++reader_failures;
            if(identity.in_current_raw()) ++reader_failures;
            (void)identity.initialized();
        }
        // Same predicate used by the native ProcessEvent early gate; this is
        // an owned counter, not fake Lua or a fabricated engine hook.
        if(identity.in_current_raw()) ++admitted_actions;
    });
    start.store(true,std::memory_order_release);
    for(auto& reader:readers) reader.join();
    while(!publication_done.load(std::memory_order_acquire)) std::this_thread::yield();
    check(reader_failures.load()==0,"concurrent raw readers see unknown or single published ID");
    check(identity.initialized(),"publication visible after reader joins");
    check(identity.id()==publisher_id,"checked ID is actual publishing thread");
    check(!identity.in_current_raw(),"different caller refused");
    check(identity.publish_current()==Publication::DifferentThread,"other publisher cannot replace identity");
    check(identity.id_raw()==publisher_id,"identity immutable after rejected publisher");
    release.store(true,std::memory_order_release);
    publisher.join();
    check(first==Publication::Published && repeat==Publication::AlreadyCurrent,"first and repeated publication classification");
    check(admitted_actions.load()==1,"only known publishing thread admits owned action");

    Identity contenders;
    std::atomic_bool race_start{false}, race_release{false};
    std::atomic_int completed{0}, published{0}, rejected{0};
    std::vector<std::thread> workers;
    for(int n=0;n<8;++n) workers.emplace_back([&] {
        while(!race_start.load(std::memory_order_acquire)) std::this_thread::yield();
        const auto result=contenders.publish_current();
        if(result==Publication::Published) ++published;
        else if(result==Publication::DifferentThread) ++rejected;
        ++completed;
        while(!race_release.load(std::memory_order_acquire)) std::this_thread::yield();
    });
    race_start.store(true,std::memory_order_release);
    while(completed.load()!=8) std::this_thread::yield();
    check(published.load()==1 && rejected.load()==7,"concurrent publication has exactly one winner");
    const auto winner=contenders.id_raw();
    bool actual_owned=false;
    for(const auto& worker:workers) actual_owned |= worker.get_id()==winner;
    check(actual_owned,"winner ID belongs to an actual live owned thread");
    race_release.store(true,std::memory_order_release);
    for(auto& worker:workers) worker.join();
    std::printf("identity_controls checks=%d failures=%d owned_threads_only=1\n",checks,failures);
    return failures?1:0;
}
