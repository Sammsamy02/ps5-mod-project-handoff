// test_host_core.cpp -- host tests for NativeModHostCore.hpp + HookAdmission.hpp.
// Mods here are hand-written C tables (no SDK) so every validation path and
// every ordering rule is exercised directly. Mac-only.
#include "fake_engine.hpp"

#include <atomic>
#include <chrono>
#include <thread>

using namespace RC::Platform;
using namespace RC::Platform::NativeMods;

namespace
{
    struct Probe
    {
        std::vector<std::string> events;
        mbm_status fail_on_update{MBM_OK};
        int updates{};
        bool destroyed{};
        const mbm_host_v1* host{};
        uint64_t token{};
        std::atomic<bool> in_callback{false};
        std::atomic<bool> release{false};
        bool stop_self_in_update{};
        Host* host_obj{};
    };
    Probe* g_probe{};
    mbm_mod_v1 g_table{};
    uint32_t g_magic = MBM_ABI_MAGIC, g_version = MBM_ABI_VERSION, g_size = sizeof(mbm_mod_v1);
    bool g_null_destroy{}, g_entry_registers{}, g_entry_fails{};

    auto p(void* i) -> Probe& { return *static_cast<Probe*>(i); }
    mbm_status on_ps(void* i) { p(i).events.push_back("program_start"); return MBM_OK; }
    mbm_status on_ui(void* i) { p(i).events.push_back("unreal_init"); return MBM_OK; }
    mbm_status on_upd(void* i)
    {
        auto& pr = p(i);
        pr.events.push_back("update");
        ++pr.updates;
        if (pr.stop_self_in_update)
        {
            auto st = pr.host_obj->stop(pr.token);
            pr.events.push_back(std::string("stop_from_inside=") + to_string(st));
            pr.events.push_back(std::string("destroyed_inside=") + (pr.destroyed ? "1" : "0"));
        }
        return pr.fail_on_update;
    }
    mbm_status on_destroy(void* i) { p(i).destroyed = true; p(i).events.push_back("destroy"); return MBM_OK; }
    const char* last_err(void*) { return "probe failure text"; }

    mbm_status entry(const mbm_host_v1* host, uint64_t token, const mbm_mod_v1** out)
    {
        g_probe->host = host;
        g_probe->token = token;
        if (g_entry_registers)
        {
            uint64_t id{};
            host->register_key_down(host->ctx, token, 0x78, 0, [](void*) -> mbm_status { return MBM_OK; }, nullptr, &id);
        }
        if (g_entry_fails) return MBM_E_MOD_FAULT;
        g_table = {};
        g_table.magic = g_magic;
        g_table.abi_version = g_version;
        g_table.struct_size = g_size;
        g_table.name = "probe";
        g_table.version = "1";
        g_table.instance = g_probe;
        g_table.on_program_start = &on_ps;
        g_table.on_unreal_init = &on_ui;
        g_table.on_update = &on_upd;
        g_table.destroy = g_null_destroy ? nullptr : &on_destroy;
        g_table.last_error = &last_err;
        *out = &g_table;
        return MBM_OK;
    }
    void reset_globals()
    {
        g_magic = MBM_ABI_MAGIC;
        g_version = MBM_ABI_VERSION;
        g_size = sizeof(mbm_mod_v1);
        g_null_destroy = g_entry_registers = g_entry_fails = false;
    }
    mbm_status tick_counter(void* user, float) { ++*static_cast<int*>(user); return MBM_OK; }
    mbm_status task_counter(void* user) { ++*static_cast<int*>(user); return MBM_OK; }
} // namespace

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Checks c;
    std::printf("== admission table ==\n");
    {
        HookAdmission::Facts none{};
        int refused_code = 0;
        for (uint32_t fam = 1; fam < MBM_HOOK_FAMILY_COUNT; ++fam)
        {
            c.ok(HookAdmission::kRules[fam].family == fam, ("rule order matches family " + std::to_string(fam)).c_str());
            auto d = HookAdmission::decide(fam, none);
            if (std::string(d.route_name) == "code-detour-only") ++refused_code;
        }
        c.ok(refused_code == 7, "seven code-detour-only families (PE/PI/PLSF/CFBN/PCE/LPE/CODE_ADDRESS)");
        HookAdmission::Facts all{true, true, true, true, true, true, true, true};
        // code-detour families stay refused whatever the facts say
        for (uint32_t fam : {MBM_HOOK_PROCESS_EVENT_GLOBAL, MBM_HOOK_PROCESS_INTERNAL_GLOBAL, MBM_HOOK_PROCESS_LOCAL_SCRIPT_GLOBAL,
                             MBM_HOOK_CALL_FUNCTION_BY_NAME, MBM_HOOK_PROCESS_CONSOLE_EXEC, MBM_HOOK_LOCAL_PLAYER_EXEC, MBM_HOOK_CODE_ADDRESS})
        {
            auto d = HookAdmission::decide(fam, all);
            c.ok(d.route == MBM_ROUTE_REFUSED && !d.admitted(), ("code-detour family stays refused with all facts: " + std::to_string(fam)).c_str());
        }
        // pending-value families stay unadmitted even when the value is present (not implemented in v1)
        for (uint32_t fam : {MBM_HOOK_GAME_VIEWPORT_TICK, MBM_HOOK_STATIC_CONSTRUCT_OBJECT, MBM_HOOK_GNATIVES_SLOT})
            c.ok(!HookAdmission::decide(fam, all).admitted(), ("pending-value family not admitted: " + std::to_string(fam)).c_str());
        c.ok(!HookAdmission::decide(MBM_HOOK_UFUNCTION, none).admitted(), "ufunction refused while thunk registry not ready");
        c.ok(HookAdmission::decide(MBM_HOOK_UFUNCTION, all).route == MBM_ROUTE_DATA_ONLY_PARTIAL, "ufunction admitted as data-only partial");
        c.ok(HookAdmission::decide(MBM_HOOK_GAME_THREAD_TICK, none).admitted(), "game-thread tick admitted before pump live (queued)");
        c.ok(!HookAdmission::decide(MBM_HOOK_GAME_THREAD_TICK, none).live, "game-thread tick not live before pump");
        c.ok(!HookAdmission::decide(MBM_HOOK_ENGINE_TICK, none).admitted(), "engine tick refused without VMT pump (launch-14 state)");
        c.ok(HookAdmission::decide(MBM_HOOK_ENGINE_TICK, all).admitted(), "engine tick admitted with VMT pump");
        c.ok(!HookAdmission::decide(0, all).admitted() && !HookAdmission::decide(99, all).admitted(), "unknown families refused");
        c.ok(HookAdmission::deliverable_by_register_hook(MBM_HOOK_GAME_THREAD_TICK) && !HookAdmission::deliverable_by_register_hook(MBM_HOOK_LOAD_MAP),
             "register_hook delivers tick families only");
    }

    std::printf("== table validation ==\n");
    struct Case { const char* what; void (*setup)(); };
    const Case bad[] = {
        {"bad magic refused", [] { g_magic = 0x1234; }},
        {"bad abi version refused", [] { g_version = 2; }},
        {"short table refused", [] { g_size = 16; }},
        {"null destroy refused", [] { g_null_destroy = true; }},
        {"entry failure refused", [] { g_entry_fails = true; }},
    };
    for (auto& k : bad)
    {
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        k.setup();
        uint64_t tok{};
        std::string why;
        auto s = h.attach("bad", &entry, &tok, &why);
        c.ok(s != MBM_OK && tok == 0 && !why.empty(), k.what);
        c.ok(h.fire(1, Event::Update) == MBM_E_STATE && pr.updates == 0, "refused mod never receives callbacks");
    }
    {
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        g_entry_registers = true;
        g_entry_fails = true;
        uint64_t tok{};
        h.attach("bad", &entry, &tok);
        c.ok(h.registration_count(1) == 0, "registrations made in a failed constructor are revoked");
        c.ok(h.attach("none", nullptr, &tok) == MBM_E_INVALID, "missing entry export refused");
    }

    std::printf("== lifecycle + fault isolation ==\n");
    {
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        pr.host_obj = &h;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        c.ok(h.attach("probe", &entry, &tok) == MBM_OK && tok != 0, "valid mod attaches");
        c.ok(e.count("[NMOD] attached abi=1 name='probe'") == 1, "attach logged once");
        h.fire(tok, Event::ProgramStart);
        h.fire(tok, Event::UnrealInit);
        h.fire(tok, Event::Update);
        h.fire(tok, Event::CppModsLoaded); // null entry: ok, no call
        c.ok(pr.events.size() == 3 && pr.events[0] == "program_start" && pr.events[1] == "unreal_init" && pr.events[2] == "update",
             "lifecycle events delivered in order");
        int ticks = 0;
        uint64_t tick_id{};
        c.ok(pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &ticks, &tick_id) == MBM_OK,
             "game-thread tick registered");
        c.ok(pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_PROCESS_EVENT_GLOBAL, &tick_counter, &ticks, &tick_id) == MBM_E_REFUSED,
             "global ProcessEvent registration refused");
        c.ok(e.count("hook refused family=5 route=code-detour-only") == 1, "refusal logged with route name");
        c.ok(pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_ENGINE_TICK, &tick_counter, &ticks, &tick_id) == MBM_E_REFUSED,
             "engine tick refused when VMT pump not armed");
        h.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        h.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        c.ok(ticks == 2, "tick delivered once per frame");
        // tasks run on the next frame, in order
        int tasks = 0;
        pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &tasks);
        pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &tasks);
        c.ok(tasks == 0, "tasks not run synchronously");
        h.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        c.ok(tasks == 2, "queued tasks run on the next game-thread frame");
        // ufunction hook: refused while thunks not ready, admitted when ready
        uint64_t hid{};
        auto pre = [](void* u, const mbm_hook_call*) -> mbm_status { ++*static_cast<int*>(u); return MBM_OK; };
        int hook_hits = 0;
        c.ok(pr.host->register_ufunction_hook(pr.host->ctx, tok, "/Script/Engine.PlayerController:ClientRestart", pre, nullptr, &hook_hits, &hid) == MBM_E_REFUSED,
             "ufunction hook refused while thunk registry not ready");
        e.f.ufunction_thunks_ready = true;
        c.ok(pr.host->register_ufunction_hook(pr.host->ctx, tok, "/Script/Engine.PlayerController:ClientRestart", pre, nullptr, &hook_hits, &hid) == MBM_OK,
             "ufunction hook admitted when thunks ready");
        e.fire_hook("/Script/Engine.PlayerController:ClientRestart", nullptr, nullptr);
        c.ok(hook_hits == 1, "pre hook fired once per call (post null)");
        e.refuse_hook_install = true;
        uint64_t hid2{};
        c.ok(pr.host->register_ufunction_hook(pr.host->ctx, tok, "/Script/Engine.Missing:Fn", pre, nullptr, &hook_hits, &hid2) == MBM_E_NOT_FOUND,
             "engine install failure propagates and leaves no record");
        e.refuse_hook_install = false;
        // call_function only on the game thread
        e.game_thread = false;
        FakeObject obj{"PC", "PlayerController /Game/PC", "PlayerController", {"PlayerController"}, {}};
        e.objects.push_back(&obj);
        c.ok(pr.host->call_function(pr.host->ctx, tok, &obj, "K2_GetPawn", nullptr, 0, nullptr) == MBM_E_STATE, "call_function refused off the game thread");
        e.game_thread = true;
        c.ok(pr.host->call_function(pr.host->ctx, tok, &obj, "K2_GetPawn", nullptr, 0, nullptr) == MBM_OK, "call_function allowed on the game thread");
        // fault: on_update fails -> faulted, all registrations revoked, no more callbacks
        pr.fail_on_update = MBM_E_MOD_FAULT;
        h.fire(tok, Event::Update);
        c.ok(h.state(tok) == ModState::Faulted, "faulting callback marks the mod faulted");
        c.ok(e.count("[NMOD] fault in on_update status=-10 error=probe failure text") == 1, "fault logged once with the SDK error text");
        int before = pr.updates;
        h.fire(tok, Event::Update);
        h.game_thread_frame(0.016f, PumpSource::GameThreadPump);
        e.fire_hook("/Script/Engine.PlayerController:ClientRestart", nullptr, nullptr);
        c.ok(pr.updates == before && ticks == 3 && hook_hits == 1, "no callback of any kind after the fault");
        c.ok(h.registration_count(tok) == 0, "fault revoked every registration");
        c.ok(std::find(e.removed_hooks.begin(), e.removed_hooks.end(), hid) != e.removed_hooks.end(), "fault removed the engine-side hook");
        c.ok(h.stop(tok) == ModState::Stopped && pr.destroyed, "faulted mod is still destroyed at stop");
        c.ok(h.fire(tok, Event::Update) == MBM_E_STATE, "stopped mod gets nothing");
    }

    std::printf("== revoke -> drain -> destroy (cross-thread) ==\n");
    {
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        h.attach("probe", &entry, &tok);
        static Probe* sp;
        sp = &pr;
        int dummy = 0;
        uint64_t kid{};
        auto slow_key = [](void*) -> mbm_status {
            sp->in_callback = true;
            while (!sp->release) std::this_thread::yield();
            sp->events.push_back(std::string("key_done destroyed=") + (sp->destroyed ? "1" : "0"));
            return MBM_OK;
        };
        pr.host->register_key_down(pr.host->ctx, tok, 0x78, 0, slow_key, &dummy, &kid);
        std::thread t([&] { e.press(0x78); });
        while (!pr.in_callback) std::this_thread::yield();
        std::atomic<bool> stopped{false};
        std::thread s([&] { h.stop(tok); stopped = true; });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        c.ok(!stopped && !pr.destroyed, "stop waits while a callback runs on another thread");
        c.ok(h.state(tok) == ModState::Stopping, "mod is Stopping (revoked) during the drain");
        e.press(0x78); // a new key press while stopping must not enter the mod
        pr.release = true;
        t.join();
        s.join();
        c.ok(pr.destroyed && h.state(tok) == ModState::Stopped, "destroy ran after the callback returned");
        c.ok(std::count(pr.events.begin(), pr.events.end(), std::string("key_done destroyed=0")) == 1, "running callback finished before destroy, and only once");
    }

    std::printf("== deferred stop from inside a callback ==\n");
    {
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        pr.host_obj = &h;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        h.attach("probe", &entry, &tok);
        pr.stop_self_in_update = true;
        h.fire(tok, Event::Update);
        c.ok(std::find(pr.events.begin(), pr.events.end(), "stop_from_inside=stopping") != pr.events.end(), "stop from inside returns Stopping");
        c.ok(std::find(pr.events.begin(), pr.events.end(), "destroyed_inside=0") != pr.events.end(), "mod not destroyed while its own callback runs");
        c.ok(pr.destroyed && h.state(tok) == ModState::Stopped, "deferred destroy ran when the callback returned");
    }

    std::printf("== drain timeout -> retained, never destroyed ==\n");
    {
        FakeEngine e;
        Limits lim;
        lim.drain_timeout = std::chrono::milliseconds(60);
        Host h{e, "t", lim};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        h.attach("probe", &entry, &tok);
        static Probe* sp2;
        sp2 = &pr;
        uint64_t kid{};
        pr.host->register_key_down(pr.host->ctx, tok, 0x79, 0, [](void*) -> mbm_status {
            sp2->in_callback = true;
            while (!sp2->release) std::this_thread::yield();
            return MBM_OK;
        }, nullptr, &kid);
        std::thread t([&] { e.press(0x79); });
        while (!pr.in_callback) std::this_thread::yield();
        auto st = h.stop(tok);
        c.ok(st == ModState::Retained && !pr.destroyed, "drain timeout retains the mod without destroy");
        c.ok(e.count("drain timeout; mod retained, destroy NOT called") == 1, "drain timeout logged");
        pr.release = true;
        t.join();
        c.ok(!pr.destroyed, "retained mod is never destroyed later");
    }

    std::printf("== limits ==\n");
    {
        FakeEngine e;
        Limits lim;
        lim.max_registrations_per_mod = 3;
        lim.max_tasks_per_mod = 2;
        lim.max_mods = 1;
        Host h{e, "t", lim};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{}, tok2{};
        h.attach("probe", &entry, &tok);
        c.ok(h.attach("second", &entry, &tok2) == MBM_E_CAPACITY, "mod limit enforced");
        int n = 0;
        uint64_t id{};
        for (int i = 0; i < 3; ++i) pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &n, &id);
        c.ok(pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &n, &id) == MBM_E_CAPACITY, "per-mod registration limit");
        c.ok(pr.host->unregister(pr.host->ctx, tok, id) == MBM_OK, "unregister ok");
        c.ok(pr.host->unregister(pr.host->ctx, tok + 7, id) == MBM_E_NOT_FOUND, "unregister checks the owner token");
        c.ok(pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &n, &id) == MBM_OK, "slot reusable after unregister");
        pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &n);
        pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &n);
        c.ok(pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &n) == MBM_E_CAPACITY, "task queue bounded");
        h.game_thread_frame(0.f, PumpSource::GameThreadPump);
        c.ok(pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &n) == MBM_OK, "queue drains and accepts again");
        c.ok(pr.host->log != nullptr && pr.host->struct_size == sizeof(mbm_host_v1) && pr.host->magic == MBM_ABI_MAGIC, "host table header");
        c.ok(pr.host->find_object(pr.host->ctx, 999, "x", nullptr) == MBM_E_INVALID, "unknown token / null out rejected");
        h.shutdown();
        c.ok(pr.destroyed, "shutdown stops every mod");
    }

    std::printf("== ENGINE_TICK only from the VMT pump ==\n");
    {
        FakeEngine e;
        e.f.vmt_engine_tick_armed = true;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{}, id{};
        h.attach("probe", &entry, &tok);
        int et = 0, gt = 0;
        pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_ENGINE_TICK, &tick_counter, &et, &id);
        pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &gt, &id);
        h.game_thread_frame(0.f, PumpSource::GameThreadPump);
        h.game_thread_frame(0.f, PumpSource::VmtEngineTick);
        c.ok(et == 1 && gt == 2, "engine tick only from VMT drain; game-thread tick from both");
        mbm_admission a{};
        pr.host->hook_admission(pr.host->ctx, MBM_HOOK_ENGINE_TICK, &a);
        c.ok(a.route == MBM_ROUTE_DATA_ONLY && a.live == 1 && std::string(a.route_name) == "vmt-engine-tick", "admission query reports route and live");
    }
    std::printf("== [B2] per-thread callback stack: trivially destructible, bounded ==\n");
    {
        static_assert(std::is_trivially_destructible_v<RC::Platform::NativeMods::detail::ActiveTokens>, "TLS stack has no destructor");
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        h.attach("probe", &entry, &tok);
        const auto refusals_before = h.counters().refusals;
        std::vector<std::unique_ptr<Host::CallScope>> scopes;
        for (uint32_t i = 0; i < RC::Platform::NativeMods::detail::k_max_callback_depth; ++i) scopes.push_back(std::make_unique<Host::CallScope>(h, tok));
        bool all_in = true;
        for (auto& sc : scopes) all_in = all_in && sc->entered();
        c.ok(all_in, "32 nested callback scopes on one thread are admitted");
        {
            Host::CallScope deep{h, tok};
            c.ok(!deep.entered() && deep.depth_refused() && h.counters().refusals == refusals_before + 1, "the 33rd nested scope is refused (counted), not grown");
        }
        scopes.clear();
        Host::CallScope again{h, tok};
        c.ok(again.entered() && RC::Platform::NativeMods::detail::t_active.depth == 1, "stack unwinds fully; next scope admitted");
        std::atomic<uint32_t> other_depth{99};
        std::thread([&] { other_depth = RC::Platform::NativeMods::detail::t_active.depth; }).join();
        c.ok(other_depth == 0, "the stack is per thread");
    }

    std::printf("== [F4] revoked / unregistered registrations are erased (bounded per-pump walk) ==\n");
    {
        FakeEngine e;
        e.f.ufunction_thunks_ready = true;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        h.attach("probe", &entry, &tok);
        int n = 0;
        // a mod that re-registers a tick on every "map load": 1000 cycles
        for (int i = 0; i < 1000; ++i)
        {
            uint64_t id{};
            pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &n, &id);
            pr.host->unregister(pr.host->ctx, tok, id);
        }
        c.ok(h.callback_table_size() == 0, "1000 register/unregister cycles leave an empty table (got " + std::to_string(h.callback_table_size()) + ")");
        auto pre = [](void*, const mbm_hook_call*) -> mbm_status { return MBM_OK; };
        uint64_t hid{};
        pr.host->register_ufunction_hook(pr.host->ctx, tok, "/Script/Engine.PlayerController:ClientRestart", pre, nullptr, nullptr, &hid);
        const auto removed_before = e.removed_hooks.size();
        c.ok(pr.host->unregister(pr.host->ctx, tok, hid) == MBM_OK && e.removed_hooks.size() == removed_before + 1 && h.callback_table_size() == 0,
             "ufunction unregister: engine hook removed, then the entry erased");
        c.ok(pr.host->unregister(pr.host->ctx, tok, hid) == MBM_E_NOT_FOUND, "second unregister of the same id: not found (no reuse, ids are never recycled)");
        e.fire_hook("/Script/Engine.PlayerController:ClientRestart", nullptr, nullptr); // late engine call with an erased id
        c.ok(h.counters().hook_calls == 0, "a late engine call with an erased record id reaches nothing");
        for (int i = 0; i < 5; ++i)
        {
            uint64_t id{};
            pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &n, &id);
            pr.host->register_key_down(pr.host->ctx, tok, 0x70 + i, 0, [](void*) -> mbm_status { return MBM_OK; }, nullptr, &id);
        }
        pr.host->register_ufunction_hook(pr.host->ctx, tok, "/Script/Engine.PlayerController:ClientRestart", pre, nullptr, nullptr, &hid);
        c.ok(h.callback_table_size() == 11, "11 live registrations in the table");
        h.fault(tok, "test", MBM_E_MOD_FAULT);
        c.ok(h.callback_table_size() == 0 && std::find(e.removed_hooks.begin(), e.removed_hooks.end(), hid) != e.removed_hooks.end(),
             "fault: engine hook removed, then every entry of the mod erased");
        e.press(0x70);
        c.ok(h.counters().key_calls == 0, "a key bind the input handler still holds resolves to nothing after the erase");
        h.stop(tok);
    }

    std::printf("== [F6] host services refuse a mod that is not running ==\n");
    {
        FakeEngine e;
        Host h{e};
        e.host = &h;
        Probe pr;
        g_probe = &pr;
        reset_globals();
        uint64_t tok{};
        h.attach("probe", &entry, &tok);
        FakeObject obj{"P", "Pawn /Game/P", "Pawn", {"Pawn"}, {}};
        obj.props["CustomTimeDilation"] = FakeProp{MBM_VT_F32};
        e.objects = {&obj};
        mbm_value v{};
        v.type = MBM_VT_F32;
        v.v.f32 = 3.0f;
        mbm_object found{};
        c.ok(pr.host->property_set(pr.host->ctx, tok, &obj, "CustomTimeDilation", &v) == MBM_OK && pr.host->find_object(pr.host->ctx, tok, "P", &found) == MBM_OK,
             "running mod: services work");
        h.fault(tok, "test", MBM_E_MOD_FAULT); // e.g. a callback failed
        const auto log_lines = e.lines.size();
        v.v.f32 = 9.0f;
        // what a worker thread the mod started would still call:
        int32_t is_valid{};
        uint64_t id{};
        int n = 0;
        const bool all_state = pr.host->property_set(pr.host->ctx, tok, &obj, "CustomTimeDilation", &v) == MBM_E_STATE &&
                               pr.host->property_get(pr.host->ctx, tok, &obj, "CustomTimeDilation", &v) == MBM_E_STATE &&
                               pr.host->find_object(pr.host->ctx, tok, "P", &found) == MBM_E_STATE &&
                               pr.host->object_is_valid(pr.host->ctx, tok, &obj, &is_valid) == MBM_E_STATE &&
                               pr.host->call_function(pr.host->ctx, tok, &obj, "K2_GetPawn", nullptr, 0, nullptr) == MBM_E_STATE &&
                               pr.host->register_hook(pr.host->ctx, tok, MBM_HOOK_GAME_THREAD_TICK, &tick_counter, &n, &id) == MBM_E_STATE &&
                               pr.host->register_key_down(pr.host->ctx, tok, 0x78, 0, [](void*) -> mbm_status { return MBM_OK; }, nullptr, &id) == MBM_E_STATE &&
                               pr.host->execute_in_game_thread(pr.host->ctx, tok, &task_counter, &n) == MBM_E_STATE;
        c.ok(all_state, "faulted mod: every service returns MBM_E_STATE");
        c.ok(obj.props["CustomTimeDilation"].f32 == 3.0f, "faulted mod's property write did not reach the object");
        pr.host->log(pr.host->ctx, tok, MBM_LOG_NORMAL, "worker thread still logging");
        c.ok(e.lines.size() == log_lines && e.count("worker thread still logging") == 0, "faulted mod's log calls are dropped");
        c.ok(pr.host->find_object(pr.host->ctx, 999, "P", &found) == MBM_E_INVALID, "unknown token stays MBM_E_INVALID");
        h.stop(tok);
        c.ok(pr.host->property_get(pr.host->ctx, tok, &obj, "CustomTimeDilation", &v) == MBM_E_STATE || pr.host->property_get(pr.host->ctx, tok, &obj, "CustomTimeDilation", &v) == MBM_E_INVALID,
             "stopped mod: refused");
    }
    {
        // log stays open while destroy() runs (Stopping): a destructor can say goodbye
        FakeEngine e;
        Host h{e};
        e.host = &h;
        static const mbm_host_v1* s_host;
        static uint64_t s_tok;
        mbm_mod_v1 t{};
        t.magic = MBM_ABI_MAGIC;
        t.abi_version = MBM_ABI_VERSION;
        t.struct_size = sizeof(mbm_mod_v1);
        t.name = "bye";
        t.destroy = [](void*) -> mbm_status {
            s_host->log(s_host->ctx, s_tok, MBM_LOG_NORMAL, "goodbye from destroy");
            return MBM_OK;
        };
        static mbm_mod_v1* s_t;
        s_t = &t;
        uint64_t tok{};
        h.attach("bye", [](const mbm_host_v1* host, uint64_t token, const mbm_mod_v1** out) -> mbm_status {
            s_host = host;
            s_tok = token;
            *out = s_t;
            return MBM_OK;
        }, &tok);
        h.stop(tok);
        c.ok(e.count("goodbye from destroy") == 1, "log allowed inside destroy (Stopping)");
    }

    std::printf("== [F3] stop/attach cycles never exhaust max_mods ==\n");
    {
        FakeEngine e;
        Limits lim;
        lim.max_mods = 2;
        Host h{e, "t", lim};
        e.host = &h;
        bool all_ok = true;
        for (int i = 0; i < 40; ++i)
        {
            Probe pr;
            g_probe = &pr;
            reset_globals();
            uint64_t tok{};
            all_ok = all_ok && h.attach("probe", &entry, &tok) == MBM_OK;
            all_ok = all_ok && h.stop(tok) == ModState::Stopped && pr.destroyed;
        }
        c.ok(all_ok, "40 attach/stop cycles with max_mods=2 all succeed");
        c.ok(h.mod_record_count() <= 2, "stopped records are pruned (records now " + std::to_string(h.mod_record_count()) + ")");
        Probe a, b, x;
        uint64_t ta{}, tb{}, tx{};
        g_probe = &a;
        h.attach("a", &entry, &ta);
        g_probe = &b;
        h.attach("b", &entry, &tb);
        g_probe = &x;
        c.ok(h.attach("x", &entry, &tx) == MBM_E_CAPACITY, "the limit still counts mods that are not stopped");
        h.shutdown();
    }

    std::printf("== [F5] FrameGate: at most one GUI frame per engine frame ==\n");
    {
        using namespace std::chrono;
        const auto t0 = steady_clock::time_point{} + hours(1);
        {
            FrameGate g;
            int due = 0;
            float max_dt = 0;
            for (int i = 0; i < 2000; ++i) // marshal pump only, 2000 calls in one second
            {
                float dt{};
                if (g.due(t0 + microseconds(500 * i), PumpSource::GameThreadPump, &dt)) { ++due; max_dt = std::max(max_dt, dt); }
            }
            c.ok(due >= 120 && due <= 126, "2000 marshal calls/s -> " + std::to_string(due) + " frames (8 ms floor, <= 126)");
            c.ok(max_dt > 0.0079f && max_dt < 0.0086f, "dt = time since the previous frame that ran");
        }
        {
            FrameGate g;
            int due = 0, due_vmt = 0;
            for (int i = 0; i < 2000; ++i)
            {
                const auto now = t0 + microseconds(500 * i);
                if (i % 33 == 0 && g.due(now, PumpSource::VmtEngineTick, nullptr)) { ++due; ++due_vmt; } // ~60 Hz engine tick
                if (g.due(now, PumpSource::GameThreadPump, nullptr)) ++due;
            }
            c.ok(due == due_vmt && due_vmt == 61, "both pumps live: only the VMT Tick pump drives the frame (" + std::to_string(due) + " frames, 61 ticks)");
        }
        {
            FrameGate g;
            g.due(t0, PumpSource::VmtEngineTick, nullptr);
            c.ok(!g.due(t0 + milliseconds(100), PumpSource::GameThreadPump, nullptr), "marshal pump yields while the VMT pump was seen recently");
            c.ok(g.due(t0 + milliseconds(300), PumpSource::GameThreadPump, nullptr), "marshal pump takes over 250 ms after the VMT pump went quiet");
            c.ok(!g.due(t0 + milliseconds(302), PumpSource::VmtEngineTick, nullptr), "VMT pump keeps a 4 ms floor");
            c.ok(g.stats().calls == 4 && g.stats().due == 2, "gate counts calls and frames");
        }
        {
            FrameGate g;
            int due = 0;
            for (int i = 0; i < 60; ++i) // the R11 cadence: about one marshal call per frame
                if (g.due(t0 + microseconds(16667 * i), PumpSource::GameThreadPump, nullptr)) ++due;
            c.ok(due == 60, "one pump call per 60 Hz frame: every frame drawn (no frame lost)");
        }
    }
    return c.finish("host_core");
}
