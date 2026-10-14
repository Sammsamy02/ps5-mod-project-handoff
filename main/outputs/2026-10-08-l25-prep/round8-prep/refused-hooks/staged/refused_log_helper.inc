    // =====================================================================================
    // [REFUSED-LOG] (round8 refused-hooks track; generic, no mod/game names) ONE uniform line
    // for every hook/registration this port refuses (the Lua call throws) or accepts but can
    // never deliver (the call returns normally, the callback is never invoked):
    //
    //   [REFUSED] kind=<Lua API> target='<UFunction path|class|event|command|->' mod='<mod|*>' reason=<code> effect=<throws|accepted-undelivered|queued-pending|never-delivered>
    //
    // Why: the B4 launch had no log line at all for the minimap's 3 refused RegisterHook calls
    // and for 18 self-test registrations that returned "registered"/ret=nil but never fire.
    // Formatting uses narrow literals + one-char widening (the B6 emit_line technique), so it
    // does not depend on ensure_str / the short-wchar codec. The helper never throws and holds
    // no lock; it is safe to call immediately before lua.throw_error (it returns first, so no
    // C++ object of the caller is skipped by a longjmp that it did not already skip).
    // =====================================================================================
    namespace
    {
    namespace refused_log
    {
        // Gate state. Written at startup / by the one-shot LoadMap arm; read at registration time.
        inline std::atomic<bool> eager_detour_block_skipped{false}; // on_program_start: R3MarshalPumpPending() branch taken
        inline std::atomic<int> loadmap_state{0};                   // 0 eager block ran, 1 deferred arm pending, 2 armed, 3 arm refused
        inline std::atomic<uint32_t> total{0};                      // every refusal/undelivered registration, even past the line cap
        constexpr uint32_t kMaxLines = 256;

        enum class Family { EagerDetour, LoadMap, CustomEvent };

        inline auto ascii_from(const StringType& s, char* out, size_t n) -> void
        {
            size_t i = 0;
            for (; i + 1 < n && i < s.size(); ++i)
            {
                const auto c = static_cast<unsigned long>(s[i]);
                out[i] = (c >= 0x20 && c < 0x7f && c != '\'') ? static_cast<char>(c) : '?';
            }
            out[i] = '\0';
        }

        inline auto emit(const char* kind, const StringType& target, const StringType& mod, const char* reason, const char* effect) -> void
        {
            const auto n = total.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n > kMaxLines + 1) { return; }
            char t[300]; char m[100]; char line[640];
            ascii_from(target, t, sizeof(t));
            ascii_from(mod, m, sizeof(m));
            if (n == kMaxLines + 1)
            {
                std::snprintf(line, sizeof(line), "[REFUSED] kind=log target='-' mod='*' reason=line-cap-%u-reached effect=suppressed", kMaxLines);
            }
            else
            {
                std::snprintf(line, sizeof(line), "[REFUSED] kind=%s target='%s' mod='%s' reason=%s effect=%s", kind, t, m, reason, effect);
            }
            StringType wide{};
            for (const char* p = line; *p != '\0'; ++p)
            {
                wide.push_back(static_cast<StringType::value_type>(static_cast<unsigned char>(*p)));
            }
            L22Diagnostics::observe([&]() { Output::send(STR("{}\n"), wide); });
        }

        // Mod name of the calling Lua state, never throws, leaves the Lua stack as it found it.
        inline auto mod_name_of(const LuaMadeSimple::Lua& lua) -> StringType
        {
            StringType out{STR("?")};
            lua_State* L = lua.get_lua_state();
            const int top = lua_gettop(L);
            if (lua_getglobal(L, "ModRef") != LUA_TNIL)
            {
                auto& ref = lua.get_userdata<LuaType::LuaModRef>(-1);
                if (auto* m = ref.get_remote_cpp_object()) { out = StringType{m->get_name()}; }
            }
            lua_settop(L, top);
            return out;
        }

        // nullptr when the family can be delivered right now, else the reason code.
        inline auto undelivered_reason(Family f) -> const char*
        {
            switch (f)
            {
            case Family::EagerDetour:
                return eager_detour_block_skipped.load(std::memory_order_acquire) ? "detour-block-skipped-r3-marshal" : nullptr;
            case Family::LoadMap:
                switch (loadmap_state.load(std::memory_order_acquire))
                {
                case 1: return "loadmap-deferred-arm-pending";
                case 3: return "loadmap-detour-arm-refused";
                default: return nullptr;
                }
            case Family::CustomEvent:
            {
                const bool plsf = Unreal::UObject::ProcessLocalScriptFunctionInternal.is_ready() && Unreal::Version::IsAtLeast(4, 22);
                const bool pi = Unreal::UObject::ProcessInternalInternal.is_ready() && Unreal::Version::IsBelow(4, 22);
                return (plsf || pi) ? nullptr : "process-local-script-function-unavailable";
            }
            }
            return nullptr;
        }

        // The Lua call is about to throw. Call this BEFORE lua.throw_error.
        inline auto throws(const LuaMadeSimple::Lua& lua, const char* kind, const StringType& target, const char* reason) -> void
        {
            emit(kind, target, mod_name_of(lua), reason, "throws");
        }

        // The Lua call is about to return normally. Logs only when the callback can never (or not yet) fire.
        inline auto accepted(const LuaMadeSimple::Lua& lua, const char* kind, const StringType& target, Family f) -> void
        {
            if (const char* why = undelivered_reason(f))
            {
                emit(kind, target, mod_name_of(lua), why, f == Family::LoadMap && loadmap_state.load() == 1 ? "queued-pending" : "accepted-undelivered");
            }
        }
    }
    }

