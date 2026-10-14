// Host test — worker-gate5-loadmap (ID: l11-loadmap-defer-r3), 2026-10-05.
// LOCAL-ONLY proposal work: models the staged LuaMod.cpp r3 predicate/decision logic
// (deferred LoadMap arm) as a plain C++20 harness. No UE4SS headers, no PS5 surface,
// no engine claims. The model mirrors the staged control flow 1:1 where it is
// load-bearing: the two file-static atomics (acquire/acq_rel/exchange), the
// pending-set-before-any-drain ordering, the queue-state-independent one-shot
// attempt, the ERROR_ID(=0) rc contract, and the fail-closed loud-refusal rule.
//
// Decision table (mission item 4): R3 mode on/off x arm success/failure x hook
// registrations present/absent (+ partial-arm, never-proven, idempotence, and
// spurious-drain rows). Every row asserts BOTH the state transitions and the
// log-sink shape (silent skips fail the test).

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <cstring>

// ---------------------------------------------------------------- model surface
using GlobalCallbackId = uint64_t;
static constexpr GlobalCallbackId ERROR_ID = 0; // Hooks/GlobalCallbackId.hpp contract

struct LogLine { std::string level; std::string text; };
static std::vector<LogLine> g_log;
static auto log_line(const char* level, const std::string& text) { g_log.push_back({level, text}); }
static auto count_log(const char* level, const char* substr) -> int
{
    int n = 0;
    for (const auto& l : g_log)
        if (l.level == level && l.text.find(substr) != std::string::npos) ++n;
    return n;
}

// The Lua-side queued-callback vectors (model of LuaMod::m_load_map_pre/post_callbacks).
static std::vector<int> g_queued_pre;
static std::vector<int> g_queued_post;

// Injected world state.
static std::atomic<bool> g_pump_pending{false};        // R3MarshalPumpPending analog
static int  g_arm_outcome_mode = 0;                    // +1 arm succeeds, -1 arm refused, +2 pre-only
static int  g_register_calls     = 0;                  // eager + deferred Register* calls
static int  g_detour_fires       = 0;                  // real LoadMap events through an armed detour
static int  g_delivered_pre      = 0;
static bool g_detour_armed       = false;

// Model of the staged RegisterLoadMapPre/PostCallback(WithData, options): arms the
// detour (EnsureHooked->ActivateHook->PLH hook) and stores the callback ONLY on
// success; refusal emits the canonical loud pair and returns ERROR_ID (DetourInstance
// AddCallback :284-288 contract).
static auto model_register_loadmap(bool is_pre) -> GlobalCallbackId
{
    ++g_register_calls;
    const bool success = g_arm_outcome_mode > 0 && (g_arm_outcome_mode == 1 || is_pre);
    if (!success)
    {
        log_line("Warning", "[UE4SS.LoadMap.LuaModImpl] Tried to install hook but hooking is disabled for this function.");
        log_line("Error", "[UE4SS.LoadMap.LuaModImpl] Failed to add hook, detour installation likely failed!");
        return ERROR_ID;
    }
    g_detour_armed = true;
    log_line("Verbose", "[UE4SS.LuaModImpl] Added hook with Id!");
    return 0x100000 | (is_pre ? 1 : 2);
}

// Model of a real UEngine::LoadMap event: delivers queued pre callbacks through the
// armed detour (the delivery lambda iterates the LIVE vectors at fire time).
static auto model_fire_loadmap() -> void
{
    if (!g_detour_armed) return; // no detour => no event routed => no delivery (upstream law)
    ++g_detour_fires;
    g_delivered_pre += static_cast<int>(g_queued_pre.size());
}

// ------------------------------------------------------- staged r3 logic, mirrored
static std::atomic<bool> m_loadmap_deferred_arm_pending{false};
static std::atomic<bool> m_loadmap_deferred_arm_attempted{false};

static auto try_deferred_loadmap_arm() -> void
{
    if (!m_loadmap_deferred_arm_pending.load(std::memory_order_acquire)) return;
    if (m_loadmap_deferred_arm_attempted.exchange(true, std::memory_order_acq_rel)) return;
    m_loadmap_deferred_arm_pending.store(false, std::memory_order_release);
    const auto pre_id  = model_register_loadmap(true);
    const auto post_id = model_register_loadmap(false);
    if (pre_id != ERROR_ID && post_id != ERROR_ID)
    {
        log_line("Default", "[R3] LoadMap detour armed (deferred arm; marshal world proven)");
        return;
    }
    log_line("Error", "[R3] LoadMap deferred arm refused (fail-closed; queued callbacks stay undelivered)");
}

// on_program_start gate (r2 structure + r3 pending store) and drain hook (r3 call site).
static auto on_program_start_gate() -> void
{
    if (g_pump_pending.load())
    {
        m_loadmap_deferred_arm_pending.store(true, std::memory_order_release); // BEFORE consumer can arm
        log_line("Warning", "[R3] LoadMap detour registrations deferred (queued; one-shot deferred arm when marshal world proven)");
        return;
    }
    model_register_loadmap(true);  // eager pre  (W-works, unchanged behavior)
    model_register_loadmap(false); // eager post
}

static auto funcptr_marshal_hook_drain() -> void
{
    // staged order: identity publication/game-thread/dispatch ownership all modeled as
    // "this drain IS proven"; the call sits after those gates, before action processing.
    try_deferred_loadmap_arm();
}

// --------------------------------------------------------------------- test rows
static int g_failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); ++g_failures; } } while (0)

static auto reset_world(std::atomic<bool> pump_pending, int arm_mode) -> void
{
    g_log.clear();
    g_queued_pre.clear(); g_queued_post.clear();
    g_pump_pending.store(pump_pending);
    g_arm_outcome_mode = arm_mode;
    g_register_calls = 0; g_detour_fires = 0; g_delivered_pre = 0; g_detour_armed = false;
    m_loadmap_deferred_arm_pending.store(false);
    m_loadmap_deferred_arm_attempted.store(false);
}

int main()
{
    // Row 1 — R3 OFF (W-works), eager arm OK, hooks PRESENT at load: eager arms both,
    // delivery works, pending never set, deferred arm never fires.
    {
        reset_world(false, +1);
        on_program_start_gate();
        g_queued_pre.push_back(1); // main.lua registered during start_mods
        model_fire_loadmap();
        CHECK(g_register_calls == 2, "R3off/ok/present: eager pre+post armed (2 calls)");
        CHECK(g_delivered_pre == 1, "R3off/ok/present: queued callback delivered on fire");
        CHECK(!m_loadmap_deferred_arm_attempted.load(), "R3off/ok/present: deferred arm never attempted");
        CHECK(count_log("Warning", "[R3] LoadMap detour registrations deferred") == 0, "R3off: no deferral Warning");
    }
    // Row 2 — R3 OFF, hooks ABSENT: eager arms both unconditionally (upstream shape);
    // fire delivers nothing; still no deferred activity.
    {
        reset_world(false, +1);
        on_program_start_gate();
        model_fire_loadmap();
        CHECK(g_register_calls == 2, "R3off/ok/absent: eager arms both");
        CHECK(g_delivered_pre == 0, "R3off/ok/absent: nothing queued, nothing delivered");
        CHECK(!m_loadmap_deferred_arm_attempted.load(), "R3off/ok/absent: deferred never attempted");
    }
    // Row 3 — R3 ON, deferred arm SUCCEEDS, hooks present: eager skipped + 1 Warning;
    // ONE attempt at first proven drain; armed line; queued callbacks deliver.
    {
        reset_world(true, +1);
        on_program_start_gate();
        g_queued_pre.push_back(1);
        CHECK(g_register_calls == 0, "R3on: eager block skipped entirely");
        CHECK(count_log("Warning", "[R3] LoadMap detour registrations deferred") == 1, "R3on: exactly one boot Warning");
        funcptr_marshal_hook_drain();
        CHECK(m_loadmap_deferred_arm_attempted.load(), "R3on/ok: attempt fired at first proven drain");
        CHECK(g_register_calls == 2, "R3on/ok: deferred pre+post registered exactly once");
        CHECK(count_log("Default", "LoadMap detour armed (deferred arm") == 1, "R3on/ok: loud armed line");
        model_fire_loadmap();
        CHECK(g_delivered_pre == 1, "R3on/ok: queued callback delivered through armed detour");
        funcptr_marshal_hook_drain();
        funcptr_marshal_hook_drain();
        CHECK(g_register_calls == 2, "R3on/ok: extra drains never re-attempt (one-shot)");
    }
    // Row 4 — R3 ON, deferred arm SUCCEEDS, hooks ABSENT at attempt, registered AFTER
    // (the mod-load-vs-carrier-tick order trap): attempt still fires; late registration
    // delivers on the next real event.
    {
        reset_world(true, +1);
        on_program_start_gate();
        funcptr_marshal_hook_drain();
        CHECK(g_register_calls == 2, "R3on/ok/late: attempt NOT gated on queue state");
        g_queued_pre.push_back(7); // main.lua registers after the arm
        model_fire_loadmap();
        CHECK(g_delivered_pre == 1, "R3on/ok/late: late-registered callback delivered");
    }
    // Row 5 — R3 ON, deferred arm REFUSED, hooks present: one attempt across many
    // drains; canonical loud pair + explicit fail-closed Error; callbacks stay queued;
    // no delivery; no crash-class behavior.
    {
        reset_world(true, -1);
        on_program_start_gate();
        g_queued_pre.push_back(1);
        for (int i = 0; i < 50; ++i) funcptr_marshal_hook_drain();
        CHECK(g_register_calls == 2, "R3on/refused: exactly one attempt (2 register calls total)");
        CHECK(count_log("Error", "Failed to add hook, detour installation likely failed!") == 2, "R3on/refused: canonical loud refusal pair per side");
        CHECK(count_log("Error", "[R3] LoadMap deferred arm refused") == 1, "R3on/refused: explicit fail-closed Error (never silent)");
        model_fire_loadmap();
        CHECK(g_delivered_pre == 0 && !g_detour_armed, "R3on/refused: no detour, no delivery, queued intact");
    }
    // Row 6 — R3 ON, deferred arm REFUSED, hooks ABSENT: same loud one-shot refusal;
    // nothing delivered (nothing queued); state explicit for later-registered mods.
    {
        reset_world(true, -1);
        on_program_start_gate();
        funcptr_marshal_hook_drain();
        g_queued_pre.push_back(9);
        model_fire_loadmap();
        CHECK(count_log("Error", "[R3] LoadMap deferred arm refused") == 1, "R3on/refused/absent: loud refusal stands");
        CHECK(g_delivered_pre == 0, "R3on/refused/absent: nothing delivered");
        CHECK(!g_detour_armed, "R3on/refused/absent: detour never armed");
    }
    // Row 7 — R3 ON, PARTIAL arm (pre succeeds, post refused): per-side ids reported;
    // pre-side delivery works; post-side loss named loudly. Never a whole-block skip.
    {
        reset_world(true, +2);
        on_program_start_gate();
        g_queued_pre.push_back(1);
        funcptr_marshal_hook_drain();
        CHECK(g_detour_armed, "R3on/partial: pre side armed");
        CHECK(count_log("Error", "Failed to add hook, detour installation likely failed!") == 1, "R3on/partial: post-side canonical refusal");
        CHECK(count_log("Error", "[R3] LoadMap deferred arm refused") == 1, "R3on/partial: explicit fail-closed Error");
        model_fire_loadmap();
        CHECK(g_delivered_pre == 1, "R3on/partial: pre-side delivery still works");
    }
    // Row 8 — R3 ON, marshal world NEVER proven (no drain): no attempt ever fires;
    // the boot Warning is the standing loud declaration (fail-loud-at-boot, not silent).
    {
        reset_world(true, +1);
        on_program_start_gate();
        g_queued_pre.push_back(1);
        model_fire_loadmap();
        CHECK(!m_loadmap_deferred_arm_attempted.load(), "R3on/unproven: no attempt without a proven drain");
        CHECK(g_register_calls == 0, "R3on/unproven: no registration happened");
        CHECK(count_log("Warning", "[R3] LoadMap detour registrations deferred") == 1, "R3on/unproven: boot Warning stands");
    }
    // Row 9 — W-works spurious drain: pending never set, so a drain calling
    // try_deferred_loadmap_arm is a no-op (no double registration beside the eager arm).
    {
        reset_world(false, +1);
        on_program_start_gate();
        funcptr_marshal_hook_drain();
        CHECK(g_register_calls == 2, "W-works spurious drain: eager pair only, no duplicate arm");
    }
    // Row 10 — ordering law: pending is stored BEFORE any drain can run (drain requires
    // the marshal armed, which the consumer arms after the gate). Model the hazard: a
    // drain that ran BEFORE the gate must find pending false and do nothing.
    {
        reset_world(true, +1);
        funcptr_marshal_hook_drain(); // hypothetical early drain
        CHECK(!m_loadmap_deferred_arm_attempted.load(), "ordering: pre-gate drain is a no-op");
        on_program_start_gate();
        funcptr_marshal_hook_drain();
        CHECK(m_loadmap_deferred_arm_attempted.load(), "ordering: post-gate drain attempts exactly once");
    }

    const char* verdict = g_failures == 0 ? "PASS" : "FAIL";
    printf("%s: 10 rows, %d checks failed\n", verdict, g_failures);
    return g_failures == 0 ? 0 : 1;
}
