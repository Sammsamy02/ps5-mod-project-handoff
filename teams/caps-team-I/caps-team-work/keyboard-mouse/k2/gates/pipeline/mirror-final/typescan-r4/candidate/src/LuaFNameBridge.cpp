// [LFB-M] agent-FD (2026-10-10, Claude Q1 items 2-4): out-of-line
// FString/FText marshaling families for the Lua FName Bridge. This TU is the
// ONLY place the bridge touches the real FString/FText types; the header
// forward-declares them so the Lua marshaling route never picks up
// Helpers/String.hpp conversions. Everything below uses the bridge's OWN
// hand UTF-8<->UTF-16 loops plus, on the engine side ONLY:
//   - FString::AppendChars (header-inline: TArray::AddUninitialized +
//     std::memcpy + terminator; the engine allocator, no Strlen)
//   - FString::Len/GetCharArray (header-inline array arithmetic, no wide libc)
//   - FText(const FString&&) -> construct_with_string ->
//     UKismetTextLibrary::Conv_StringToText (engine call)
//   - FText::GetTextData / FText::ToFString ->
//     UKismetTextLibrary::Conv_TextToString (engine call; deliberately NOT
//     FText::ToString, whose StringType(const CharType*) return resolves to
//     the prebuilt 4-byte-wchar libc++ ctor on the short-wchar target)
// No wcslen/wmemcmp/swprintf/mbrtowc, no std::wstring conversion, no fmt
// wide API is reachable from any path in this file.
//
// [LFB-M] agent-FL reverse-routing round (2026-10-10): refusal logging in
// this TU is upgraded to FIRST-10 per family with a call-site tag (low 12
// bits of __builtin_return_address(0) captured at each exported API's entry,
// i.e. the Lua binding that called the bridge) and the first 8 offending
// units in hex (UTF-16 units reverse / raw input bytes forward). The shared
// family pusher push_wide_units_to_lua (new) carries the 'fname-rev' family
// for push_fname_to_lua/push_wide_to_lua (header inline) so FName userdata
// :ToString, the new FName/FText __tostring metamethods, and
// GetFullName-shaped reads are all observable; widen_narrow_to_stringtype
// (new) is the narrow -> StringType reverse filler for LuaLibrary print
// payloads ('print-widen' family; resize + indexed writes only, the EV
// discipline). Still no wide libc anywhere.
//
// [LFB-M] agent-FW cleanup round (2026-10-10, Claude cleanup directive b+c):
// every marker/refusal line this TU emits now comes from ONE CONTIGUOUS
// narrow literal per family (family_marker_template / family_refusal_
// template in the header — no "[LFB-M] " + family_name runtime assembly),
// so the harvest gate is literal-strict again. RELEASE MODE: compile with
// -DMODBRIDGE_RELEASE_DIAG_OFF to silence the PER-CALL diagnostics (the
// [LUA-FN] first-20 logger and every '[LFB-M] <family> in=...' marker —
// their literals leave the TU) while KEEPING the refusal/seam lines
// ('[FN] seam refused:' bridge-forward first-10 and '[LFB-M] <family>
// refused ...' — safety evidence) and the once-per-process composition
// stamp. Default remains VERBOSE. New API: fstring_to_narrow_string
// (engine FString -> narrow std::string; the FString userdata surface's
// get_string_from_object reverse read, formerly the to_utf8_string Helpers
// seam).

#include <cstring>

#include <LuaFNameBridge.hpp>

#include <Unreal/FString.hpp>
#include <Unreal/FText.hpp>

namespace RC::LuaType::LuaFNameBridge
{
    // FString's unit type must be exactly the bridge's CharType: on the
    // Prospero target both are the 2-byte -fshort-wchar wchar_t (UTF-16), so
    // AppendChars receives the hand-converted units at their native stride.
    static_assert(std::is_same_v<Unreal::TCHAR, CharType>, "FString TCHAR must equal the bridge CharType (short-wchar UTF-16 on Prospero, 4-byte wchar_t on the host)");

    // Shared forward fill: hand UTF-16 units into the FString's own CharType
    // array. No marker here; the family wrappers log.
    static auto fill_fstring(const CharType* units, size_t unit_count) -> Unreal::FString
    {
        Unreal::FString out;
        if (unit_count > 0)
        {
            out.AppendChars(units, static_cast<Unreal::int32>(unit_count));
        }
        return out;
    }

    // =================================================================================
    // Forward: Lua string -> FString (Claude Q1 item 2).
    // =================================================================================
    auto Bridge::to_fstring() const -> Unreal::FString
    {
        const uintptr_t site = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
        if (!ok_)
        {
            notify_family_refused(Family::FStringFwd, refusal_reason_name(reason_), input_len_, refused_at_, site, utf8_, utf8_len_, 1);
            log_family_marker(Family::FStringFwd, utf8_, input_len_, false);
            return Unreal::FString{};
        }
        Unreal::FString out = fill_fstring(units_, unit_count_);
        log_family_marker(Family::FStringFwd, utf8_, input_len_, true);
        return out;
    }

    // =================================================================================
    // Forward: Lua string -> FText (Claude Q1 item 3). The mod menu's
    // SetText(FText) path constructs the FText here from hand-converted
    // units: FString fill, then the header-inline in-port
    // FText(const FString&&) shape whose body is the engine's
    // Conv_StringToText — never the FText(const CharType*) ->
    // FString(TCHAR*) Strlen seam, never ensure_str.
    // =================================================================================
    auto Bridge::to_ftext() const -> Unreal::FText
    {
        const uintptr_t site = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
        if (!ok_)
        {
            notify_family_refused(Family::FTextFwd, refusal_reason_name(reason_), input_len_, refused_at_, site, utf8_, utf8_len_, 1);
            log_family_marker(Family::FTextFwd, utf8_, input_len_, false);
            return Unreal::FText{};
        }
        Unreal::FString filled = fill_fstring(units_, unit_count_);
        Unreal::FText made{std::move(filled)}; // engine Conv_StringToText
        log_family_marker(Family::FTextFwd, utf8_, input_len_, true);
        return made;
    }

    // =================================================================================
    // Reverse: engine -> Lua (Claude Q1 item 4). Hand UTF-16 -> UTF-8 ->
    // lua_pushlstring, bounded by kMaxPushUnits like the FName reader.
    // [LFB-M] agent-FL: this is now the EXPORTED shared family pusher; the
    // refusal path is first-10 with the binding call-site tag and the first
    // 8 offending UTF-16 units in hex.
    // =================================================================================
    auto push_wide_units_to_lua(lua_State* L, const CharType* units, size_t unit_count, Family family) -> void
    {
        const uintptr_t site = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
        if (!L) { return; }
        if (!units || unit_count == 0)
        {
            lua_pushlstring(L, "", 0);
            log_family_marker(family, "", 0, true);
            return;
        }
        if (unit_count > kMaxPushUnits)
        {
            notify_family_refused(family, "overlong", unit_count, 0, site, units, unit_count, sizeof(CharType));
            lua_pushlstring(L, "", 0);
            return;
        }
        char buf[kMaxPushUtf8Bytes + 1];
        const size_t bytes = utf16_to_utf8(units, unit_count, buf, kMaxPushUtf8Bytes);
        if (bytes == static_cast<size_t>(-1))
        {
            notify_family_refused(family, refusal_reason_name(Refusal::None), unit_count, 0, site, units, unit_count, sizeof(CharType));
            lua_pushlstring(L, "", 0);
            return;
        }
        lua_pushlstring(L, buf, bytes);
        log_family_marker(family, buf, bytes, true);
    }

    auto push_fstring_to_lua(lua_State* L, const Unreal::FString& string) -> void
    {
        // Len() is Data.Num()-1 and GetCharArray().GetData() is the raw
        // engine-allocated unit array: both header-inline, no NUL scan, no
        // wide libc.
        push_wide_units_to_lua(L,
                               string.GetCharArray().GetData(),
                               static_cast<size_t>(string.Len() > 0 ? string.Len() : 0),
                               Family::FStringRev);
    }

    auto push_ftext_to_lua(lua_State* L, const Unreal::FText& text) -> void
    {
        if (!L) { return; }
        if (!text.GetTextData())
        {
            // No text data: an empty FText reads as an empty Lua string (the
            // upstream ToFString path returns an empty FString for this too).
            lua_pushlstring(L, "", 0);
            log_family_marker(Family::FTextRev, "", 0, true);
            return;
        }
        // Engine call (Conv_TextToString). Deliberately NOT FText::ToString():
        // its StringType(const CharType*) construction resolves against the
        // prebuilt 4-byte-wchar libc++ on the short-wchar target.
        const Unreal::FString as_string = text.ToFString();
        push_wide_units_to_lua(L,
                               as_string.GetCharArray().GetData(),
                               static_cast<size_t>(as_string.Len() > 0 ? as_string.Len() : 0),
                               Family::FTextRev);
    }

    // =================================================================================
    // Reverse, std::string variant: engine FString -> narrow UTF-8
    // std::string (agent-FW cleanup round; FL inventory §2, the FString
    // userdata get_string_from_object helper — was to_utf8_string, the
    // Helpers seam). Same hand UTF-16 -> UTF-8 core, same kMaxPushUnits
    // bound, logs the 'fstring-rev' family; refusal yields the empty string
    // (first-10 notice with site tag + hex units). Never Helpers/to_string.
    // =================================================================================
    auto fstring_to_narrow_string(const Unreal::FString& string) -> std::string
    {
        const uintptr_t site = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
        const CharType* units = string.GetCharArray().GetData();
        const size_t unit_count = static_cast<size_t>(string.Len() > 0 ? string.Len() : 0);
        if (!units || unit_count == 0)
        {
            log_family_marker(Family::FStringRev, "", 0, true);
            return std::string{};
        }
        if (unit_count > kMaxPushUnits)
        {
            notify_family_refused(Family::FStringRev, "overlong", unit_count, 0, site, units, unit_count, sizeof(CharType));
            return std::string{};
        }
        char buf[kMaxPushUtf8Bytes + 1];
        const size_t bytes = utf16_to_utf8(units, unit_count, buf, kMaxPushUtf8Bytes);
        if (bytes == static_cast<size_t>(-1))
        {
            notify_family_refused(Family::FStringRev, refusal_reason_name(Refusal::None), unit_count, 0, site, units, unit_count, sizeof(CharType));
            return std::string{};
        }
        log_family_marker(Family::FStringRev, buf, bytes, true);
        return std::string{buf, bytes};
    }

    // =================================================================================
    // Reverse, narrow variant: engine FName -> UTF-8 bytes in a caller buffer
    // (Claude Q1 item 5: GetName-family reads that become Lua table keys via
    // APIs needing a const char*). Pool decoder (FName::ToString, agent ER)
    // -> hand UTF-16 -> UTF-8. Returns bytes written (terminator excluded);
    // 0 with out[0]=0 on refusal. Never to_string()/Helpers.
    // =================================================================================
    auto fname_to_narrow(const FName& name, char* out, size_t cap) -> size_t
    {
        if (!out || cap == 0) { return 0; }
        out[0] = '\0';
        StringType text = name.ToString(); // pool decoder owns ToString (agent ER)
        return wide_to_narrow(text.data(), text.size(), out, cap);
    }

    auto wide_to_narrow(const CharType* units, size_t unit_count, char* out, size_t cap) -> size_t
    {
        const uintptr_t site = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
        if (!out || cap == 0) { return 0; }
        out[0] = '\0';
        if (!units || unit_count == 0)
        {
            log_family_marker(Family::FNameNarrow, "", 0, true);
            return 0;
        }
        if (unit_count > kMaxPushUnits)
        {
            notify_family_refused(Family::FNameNarrow, "overlong", unit_count, 0, site, units, unit_count, sizeof(CharType));
            log_family_marker(Family::FNameNarrow, "", 0, false);
            return 0;
        }
        char buf[kMaxPushUtf8Bytes + 1];
        const size_t bytes = utf16_to_utf8(units, unit_count, buf, kMaxPushUtf8Bytes);
        if (bytes == static_cast<size_t>(-1))
        {
            notify_family_refused(Family::FNameNarrow, refusal_reason_name(Refusal::None), unit_count, 0, site, units, unit_count, sizeof(CharType));
            log_family_marker(Family::FNameNarrow, "", 0, false);
            return 0;
        }
        const size_t n = bytes < cap - 1 ? bytes : cap - 1;
        if (n > 0)
        {
            std::memcpy(out, buf, n); // byte copy only
        }
        out[n] = '\0';
        log_family_marker(Family::FNameNarrow, out, n, true);
        return n;
    }

    // =================================================================================
    // [LFB-M] agent-FL: narrow -> StringType reverse filler for the LuaLibrary
    // print payloads (global_print's luaL_tolstring output). Hand UTF-8 ->
    // UTF-16, then resize + indexed writes ONLY (the EV discipline: on
    // short-wchar targets even the (ptr,count) StringType ctor can resolve
    // to the prebuilt 4-byte-wchar __init). Logs the 'print-widen' family;
    // refuses first-10 (site tag + first-8 input bytes hex) and yields the
    // empty StringType. Input is bounded to kMaxPushUnits BYTES with a hand
    // walk-back to a codepoint boundary (never mid-sequence); longer payloads
    // convert their bounded prefix — the marker line still reports the FULL
    // original length so truncation stays observable.
    // =================================================================================
    auto widen_narrow_to_stringtype(const char* s, size_t n) -> StringType
    {
        const uintptr_t site = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
        StringType out;
        if (!s || n == 0)
        {
            log_family_marker(Family::PrintWiden, "", 0, true);
            return out;
        }
        size_t bounded = n;
        if (bounded > kMaxPushUnits)
        {
            bounded = kMaxPushUnits;
            while (bounded > 0 && (static_cast<unsigned char>(s[bounded]) & 0xC0) == 0x80)
            {
                --bounded; // back off to a codepoint boundary (hand walk, no libc)
            }
            if (bounded == 0)
            {
                notify_family_refused(Family::PrintWiden, "boundary", n, 0, site, s, 8, 1);
                log_family_marker(Family::PrintWiden, "", 0, false);
                return out;
            }
        }
        CharType units[kMaxPushUnits + 1];
        Refusal refusal = Refusal::None;
        size_t refused_at = 0;
        const size_t used = utf8_to_utf16(s, bounded, units, kMaxPushUnits, &refusal, &refused_at);
        if (used == static_cast<size_t>(-1))
        {
            notify_family_refused(Family::PrintWiden, refusal_reason_name(refusal), n, refused_at, site, s, 8, 1);
            log_family_marker(Family::PrintWiden, "", 0, false);
            return out;
        }
        out.resize(used);
        for (size_t i = 0; i < used; ++i) { out[i] = units[i]; } // container fill, not a conversion
        log_family_marker(Family::PrintWiden, s, n, true);
        return out;
    }
} // namespace RC::LuaType::LuaFNameBridge
