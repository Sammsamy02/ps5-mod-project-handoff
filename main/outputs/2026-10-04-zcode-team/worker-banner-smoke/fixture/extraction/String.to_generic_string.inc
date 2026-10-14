    auto inline to_generic_string(const auto& input) -> StringType
    {
        if constexpr (std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<std::remove_cvref_t<decltype(input)>>>, StringViewType>)
        {
            return StringType{input};
        }
        else if constexpr (std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<std::remove_cvref_t<decltype(input)>>>, StringType> ||
                           std::is_same_v<std::remove_cvref_t<std::remove_pointer_t<std::remove_cvref_t<decltype(input)>>>, CharType>)
        {
            return input;
        }
        else
        {
#if RC_IS_ANSI == 1
            return to_string(input);
#elif defined(__PROSPERO__)
            // PS5 codecvt bug (proven live, pids 466/492/700): every
            // wstring_convert-based route garbles (NUL-interleave on
            // narrow->wide). Narrow inputs reaching this branch are Lua
            // strings and ASCII log text (the print pipeline — without this,
            // the "[Lua] [mod] Loaded" banner itself garbles NUL-interleaved
            // on a fully successful load; worker-lifecycle-conversion P3).
            // Byte-widen instead, same discipline as SettingsManager's ASCII
            // loop and widen_ascii_native_key in the program unit.
            StringType out;
            if constexpr (std::is_pointer_v<std::remove_cvref_t<decltype(input)>>)
            {
                if (input == nullptr)
                {
                    return out;
                }
            }
            for (const char c : std::string_view{input})
            {
                out.push_back(static_cast<CharType>(static_cast<unsigned char>(c)));
            }
            return out;
#else
            return ensure_str(input);
#endif
        }
    }
