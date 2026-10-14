#include "shims.hpp"
#include <cassert>
// statics the extracted code shares with the rest of LuaMod.cpp (same names/types)
static std::atomic<bool> m_funcptr_marshal_armed{false};
static int32_t m_funcptr_marshal_callback_id{-1};
static Unreal::FName m_funcptr_marshal_carrier_id{};
static auto is_funcptr_marshal_armed() -> bool { return m_funcptr_marshal_armed.load(); }
static int64_t fake_now = 100000;
#define steady_clock_now_override
static void funcptr_marshal_hook(int&, void*) {}
#include "extracted.inc"
static int failures = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); failures += !ok; }
static Unreal::UClass function_class; static Unreal::UClass class_class;
static Unreal::UFunction* fn(const wchar_t* full, Unreal::UStruct* super, bool native, void* func = reinterpret_cast<void*>(0x1000)) {
    auto* f = new Unreal::UFunction(); f->cls = &function_class; f->full = full; f->name = Unreal::FName(full);
    f->super = super; f->flags = native ? 0x400 : 0; f->func = func; Unreal::UObjectGlobals::all.push_back(f); return f;
}
int main() {
    function_class.name = Unreal::FName(std::wstring(L"Function"));
    // engine bases (non-native BlueprintImplementableEvents)
    auto* tick = fn(L"/Script/Engine.Actor:ReceiveTick", nullptr, false);
    auto* anim = fn(L"/Script/Engine.AnimInstance:BlueprintUpdateAnimation", nullptr, false);
    // distractors: a native function, a non-function object, an unrelated BP function
    fn(L"/Script/Engine.Actor:K2_GetActorLocation", nullptr, true);
    auto* other = new Unreal::UObject(); other->cls = &class_class; other->full = L"/Game/X.X_C"; Unreal::UObjectGlobals::all.push_back(other);
    fn(L"/Game/BP_Door.BP_Door_C:OpenDoor", nullptr, false);

    // 1) program start before any BP class loaded: nothing to arm
    check(r3_marshal_arm_event_carriers() == 0 && !is_funcptr_marshal_armed(), "1 no overrides -> nothing armed");
    // 2) level loads: BP overrides appear (incl. a 2-deep BP child override and a native-flagged odd one)
    auto* bp_tick = fn(L"/Game/BP_Lamp.BP_Lamp_C:ReceiveTick", tick, false);
    auto* bp_anim = fn(L"/Game/ABP_Player.ABP_Player_C:BlueprintUpdateAnimation", anim, false);
    auto* bp_child = fn(L"/Game/BP_LampChild.BP_LampChild_C:ReceiveTick", bp_tick, false);
    auto* bp_nullfunc = fn(L"/Game/BP_Broken.BP_Broken_C:ReceiveTick", tick, false, nullptr);
    auto* bp_native = fn(L"/Game/BP_Odd.BP_Odd_C:ReceiveTick", tick, true);
    size_t n = r3_marshal_arm_event_carriers();
    check(n == 3, "2 three overrides armed (direct, anim, 2-deep child)");
    check(is_funcptr_marshal_armed() && m_funcptr_marshal_callback_id >= 0, "2 marshal armed with a callback id");
    check(bp_tick->post_hooks.size() == 1 && bp_anim->post_hooks.size() == 1 && bp_child->post_hooks.size() == 1, "2 post hook registered once each");
    check(bp_nullfunc->post_hooks.empty() && bp_native->post_hooks.empty() && tick->post_hooks.empty(), "2 null-Func, native-flagged and engine base NOT armed");
    // 3) rescan is idempotent
    check(r3_marshal_arm_event_carriers() == 0 && bp_tick->post_hooks.size() == 1, "3 rescan arms nothing twice");
    // 4) a refused RegisterPostHook is skipped, others still arm
    auto* bp_refuse = fn(L"/Game/BP_R.BP_R_C:ReceiveTick", tick, false); bp_refuse->refuse_hook = true;
    auto* bp_late = fn(L"/Game/W_Hud.W_Hud_C:ReceiveTick", tick, false);
    check(r3_marshal_arm_event_carriers() == 1 && bp_late->post_hooks.size() == 1, "4 refused hook skipped, late override armed");
    // 5) routing predicate: pump_pending -> marshal world (true); not pending -> false
    Unreal::pump_pending = false; check(!funcptr_marshal_usable(), "5 W-works world: not usable");
    Unreal::pump_pending = true;  check(funcptr_marshal_usable(), "5 marshal world: usable (armed or awaiting arm)");
    // 6) lazy arm rate limit: two immediate calls -> at most one scan
    m_funcptr_marshal_last_scan_ms.store(0); m_funcptr_marshal_last_fire_ms.store(0);
    size_t before = Output::log.size(); r3_marshal_lazy_arm(); r3_marshal_lazy_arm();
    size_t scans = 0; for (size_t i = before; i < Output::log.size(); ++i) if (Output::log[i].find(L"event carrier scan") != std::wstring::npos) ++scans;
    check(scans == 1, "6 two immediate lazy arms -> exactly one scan");
    // 7) fresh carrier fire suppresses rescans
    m_funcptr_marshal_last_scan_ms.store(0); m_funcptr_marshal_last_fire_ms.store(r3_now_ms());
    before = Output::log.size(); r3_marshal_lazy_arm();
    scans = 0; for (size_t i = before; i < Output::log.size(); ++i) if (Output::log[i].find(L"event carrier scan") != std::wstring::npos) ++scans;
    check(scans == 0, "7 recently fired carriers -> no rescan");
    std::printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL PASS", failures);
    for (auto& l : Output::log) if (l.find(L"armed:") != std::wstring::npos) std::printf("  log: %ls", l.c_str());
    return failures != 0;
}
