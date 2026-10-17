// HookAdmission.hpp -- global hook backend admission for native C++ mods (#9).
//
// Engine-free, header-only, host-testable. One table decides, for every hook
// family a PC UE4SS C++ mod can request, whether a DATA-ONLY route exists on
// this platform (vtable / function-pointer / engine-listener swaps) or whether
// the family is refused because its only PC route is a code detour. No game,
// mod, address or offset appears here: the only inputs are runtime facts that
// the existing runtime already publishes (is the per-UFunction thunk registry
// up, is the game-thread pump live, ...).
//
// Hard rules encoded here (CLAUDE.md section 3):
//   * no patching / detours of game code: every family whose route needs a
//     code patch is MBM_ROUTE_REFUSED, permanently, whatever the facts say;
//   * values that ship need two independent sources: a data-only route that
//     needs an engine address without two sources is MBM_ROUTE_PENDING_VALUE
//     and is refused exactly like REFUSED until the profile supplies it;
//   * the admission is decided BEFORE anything is installed; a refusal never
//     reaches a backend (unlike the PC path, where the request travels to
//     PLH::x64Detour and is refused deep inside the owned-memory layer).
#pragma once

#include <Mod/NativeModAbi.h>

#include <array>
#include <cstdint>

namespace RC::Platform::HookAdmission
{
    // Facts the engine binding fills from live runtime state. All default to
    // "not available": a missing fact fails closed.
    struct Facts
    {
        bool ufunction_thunks_ready{};       // ScriptHookThunks registry accepting registrations
        bool game_thread_pump_live{};        // R3 marshal carrier or VMT Tick pump has fired on the game thread
        bool vmt_engine_tick_armed{};        // copied-vtable UEngine::Tick pump armed
        bool delete_listener_admitted{};     // engine AddUObjectDeleteListener route (variant C) admitted
        bool create_listener_two_sources{};  // engine AddUObjectCreateListener address with two sources
        bool world_event_route_live{};       // [WORLD-EV] object-index route live
        bool viewport_tick_slot_two_sources{}; // UGameViewportClient::Tick vtable slot with two sources
        bool gnatives_two_sources{};         // GNatives table address with two sources
    };

    struct Rule
    {
        uint32_t family;
        uint32_t route; // route when every required fact holds
        const char* route_name;
        const char* reason;
    };

    // The table. Order = family number (checked by the host test).
    inline constexpr std::array<Rule, MBM_HOOK_FAMILY_COUNT> kRules{{
        {0, MBM_ROUTE_REFUSED, "none", "family 0 is not a hook family"},
        {MBM_HOOK_UFUNCTION, MBM_ROUTE_DATA_ONLY_PARTIAL, "ufunction-func-swap",
         "per-UFunction UFunction::Func pointer swap (ScriptHookThunks, the route Lua RegisterHook and the R3 carriers use); "
         "native UFunctions: every call; Blueprint UFunctions: only calls that enter through UFunction::Func "
         "(ProcessEvent), Blueprint-to-Blueprint calls bypass Func"},
        {MBM_HOOK_GAME_THREAD_TICK, MBM_ROUTE_DATA_ONLY, "game-thread-pump",
         "once per frame on the published game thread, from the runtime's data-only pump (R3 FuncPtr-marshal carrier or "
         "copied-vtable Tick pump); delivery starts when the pump goes live (in-world)"},
        {MBM_HOOK_ENGINE_TICK, MBM_ROUTE_DATA_ONLY, "vmt-engine-tick",
         "copied-vtable UEngine::Tick slot swap; only when that pump is armed in this process, otherwise refused: use "
         "MBM_HOOK_GAME_THREAD_TICK (same frame cadence, not inside UEngine::Tick)"},
        {MBM_HOOK_GAME_VIEWPORT_TICK, MBM_ROUTE_PENDING_VALUE, "vmt-viewport-tick",
         "a copied-vtable swap on the GameViewportClient instance is the data-only route; the Tick slot index has no "
         "two-source profile value yet"},
        {MBM_HOOK_PROCESS_EVENT_GLOBAL, MBM_ROUTE_REFUSED, "code-detour-only",
         "global UObject::ProcessEvent pre/post needs a code detour (PC: an instruction patch on the function body); vtables "
         "are shared read-only data, so no data-only global route exists. Use MBM_HOOK_UFUNCTION on the functions you need"},
        {MBM_HOOK_PROCESS_INTERNAL_GLOBAL, MBM_ROUTE_REFUSED, "code-detour-only",
         "global UObject::ProcessInternal needs a code detour; use MBM_HOOK_UFUNCTION per function"},
        {MBM_HOOK_PROCESS_LOCAL_SCRIPT_GLOBAL, MBM_ROUTE_REFUSED, "code-detour-only",
         "global ProcessLocalScriptFunction needs a code detour; use MBM_HOOK_UFUNCTION per function"},
        {MBM_HOOK_STATIC_CONSTRUCT_OBJECT, MBM_ROUTE_PENDING_VALUE, "engine-create-listener",
         "PC route is a code detour (refused); the data-only route is the engine's own AddUObjectCreateListener "
         "(variant C), whose address has no two-source value yet"},
        {MBM_HOOK_OBJECT_DELETE, MBM_ROUTE_DATA_ONLY, "engine-delete-listener",
         "engine's own FUObjectArray delete listener (variant C, admission-verified at startup)"},
        {MBM_HOOK_LOAD_MAP, MBM_ROUTE_DATA_ONLY_PARTIAL, "world-event-index",
         "data-only object-index route ([WORLD-EV]); delivered after the load, not before (PC: before)"},
        {MBM_HOOK_BEGIN_PLAY, MBM_ROUTE_DATA_ONLY_PARTIAL, "world-event-index",
         "data-only object-index route ([WORLD-EV]); after the event; streamed-level actors not reported"},
        {MBM_HOOK_INIT_GAME_STATE, MBM_ROUTE_DATA_ONLY_PARTIAL, "world-event-index",
         "data-only object-index route ([WORLD-EV]); after InitGameState ran"},
        {MBM_HOOK_CALL_FUNCTION_BY_NAME, MBM_ROUTE_REFUSED, "code-detour-only",
         "CallFunctionByNameWithArguments needs a code detour"},
        {MBM_HOOK_PROCESS_CONSOLE_EXEC, MBM_ROUTE_REFUSED, "code-detour-only", "ProcessConsoleExec needs a code detour"},
        {MBM_HOOK_LOCAL_PLAYER_EXEC, MBM_ROUTE_REFUSED, "code-detour-only", "ULocalPlayer::Exec needs a code detour"},
        {MBM_HOOK_GNATIVES_SLOT, MBM_ROUTE_PENDING_VALUE, "gnatives-slot-swap",
         "GNatives is a writable function-pointer table (data-only in form) but its address has no two-source value, and "
         "replacing every opcode handler changes all bytecode execution on every thread; pausing the game thread from a "
         "handler (Kismet debugger) is refused separately"},
        {MBM_HOOK_CODE_ADDRESS, MBM_ROUTE_REFUSED, "code-detour-only",
         "detours on arbitrary code addresses are never allowed (game code is execute-only; no-detour rule)"},
    }};

    struct Decision
    {
        uint32_t family{};
        uint32_t route{MBM_ROUTE_REFUSED};
        bool live{};
        const char* route_name{"none"};
        const char* reason{"unknown family"};
        // true when the mod may register now (admitted route; live or queued)
        bool admitted() const { return route == MBM_ROUTE_DATA_ONLY || route == MBM_ROUTE_DATA_ONLY_PARTIAL; }
    };

    inline constexpr auto decide(uint32_t family, const Facts& f) -> Decision
    {
        Decision d{};
        d.family = family;
        if (family == 0 || family >= MBM_HOOK_FAMILY_COUNT) return d;
        const Rule& r = kRules[family];
        d.route = r.route;
        d.route_name = r.route_name;
        d.reason = r.reason;
        switch (family)
        {
        case MBM_HOOK_UFUNCTION:
            // Registration is admitted only while the thunk registry accepts;
            // otherwise refused (never queued: the swap must be installed now).
            if (!f.ufunction_thunks_ready) { d.route = MBM_ROUTE_REFUSED; d.reason = "per-UFunction thunk registry not ready"; }
            d.live = f.ufunction_thunks_ready;
            break;
        case MBM_HOOK_GAME_THREAD_TICK:
            d.live = f.game_thread_pump_live; // admitted either way: queued until the pump is live
            break;
        case MBM_HOOK_ENGINE_TICK:
            if (!f.vmt_engine_tick_armed)
            {
                d.route = MBM_ROUTE_REFUSED;
                d.reason = "copied-vtable UEngine::Tick pump is not armed in this process; use MBM_HOOK_GAME_THREAD_TICK";
            }
            d.live = f.vmt_engine_tick_armed;
            break;
        case MBM_HOOK_GAME_VIEWPORT_TICK:
            // Even with the value, the swap itself is not implemented in v1.
            d.live = false;
            if (f.viewport_tick_slot_two_sources) d.reason = "slot value present, but the viewport swap is not implemented in ABI v1";
            break;
        case MBM_HOOK_STATIC_CONSTRUCT_OBJECT:
            d.live = false;
            if (f.create_listener_two_sources) d.reason = "create-listener value present, but delivery to native mods is not implemented in ABI v1";
            break;
        case MBM_HOOK_OBJECT_DELETE:
            if (!f.delete_listener_admitted) { d.route = MBM_ROUTE_REFUSED; d.reason = "engine delete-listener route not admitted in this process"; }
            // ABI v1 exposes no delete-callback signature yet: refuse registration
            // but report the route truthfully.
            d.live = f.delete_listener_admitted;
            break;
        case MBM_HOOK_LOAD_MAP:
        case MBM_HOOK_BEGIN_PLAY:
        case MBM_HOOK_INIT_GAME_STATE:
            d.live = f.world_event_route_live;
            break;
        case MBM_HOOK_GNATIVES_SLOT:
            d.live = false;
            break;
        default:
            d.live = false;
            break;
        }
        return d;
    }

    // Families whose callback shape ABI v1 actually delivers through
    // register_hook(family, mbm_tick_fn). Everything else is answered by
    // hook_admission only (the mod learns why it cannot have it).
    inline constexpr auto deliverable_by_register_hook(uint32_t family) -> bool
    {
        return family == MBM_HOOK_GAME_THREAD_TICK || family == MBM_HOOK_ENGINE_TICK;
    }
} // namespace RC::Platform::HookAdmission
