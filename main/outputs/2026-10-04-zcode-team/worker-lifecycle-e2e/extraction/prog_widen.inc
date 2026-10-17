    // PS5 codecvt discipline (proven live, pids 466/492/700): every
    // wstring_convert-based wide<->narrow route garbles (NUL-interleave or
    // [::2]). Native mod keys/names are pure ASCII UTF-8; widen byte-by-byte
    // at every native-source boundary. Hoisted from setup_mods so the whole
    // lifecycle (setup, start, reinstall, uninstall-by-name) shares one
    // conversion (worker-lifecycle-conversion P0/P1).
    [[nodiscard]] static auto widen_ascii_native_key(const std::string& narrow) -> StringType
    {
        StringType wide;
        wide.reserve(narrow.size());
        for (const char c : narrow)
        {
            wide.push_back(static_cast<File::CharType>(static_cast<unsigned char>(c)));
        }
        return wide;
    }
