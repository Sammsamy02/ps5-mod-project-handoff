// VERBATIM extraction — modbridge::normalize_path_for_lua (the fixed LuaMod
// conversion path). Source: worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp
// (sha256 c92c92ff66686bf6f44e18aaec7d36b227d32fc75726f33349f4bdc7412ef3c0)
// lines 42-53. Byte-exact; do not edit.
// Local override of normalize_path_for_lua: the shared String.hpp version
// routes through wstring_convert on this target, which has the proven
// codecvt stride bug (4-byte-unit helpers with 2-byte wchar_t). All mod
// paths are pure ASCII; use the filesystem path's narrow string directly.
namespace modbridge {
    auto normalize_path_for_lua(const std::filesystem::path& path) -> std::string
    {
        std::string utf8_path = path.string();
        std::replace(utf8_path.begin(), utf8_path.end(), '\\', '/');
        return utf8_path;
    }
}

// VERBATIM extraction — the fixed widen_ascii lambda (wpd1 widened setup_mods
// loop). Source: worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp
// (sha256 68b8bd187531574df177bcc5266374d12a94158218d75c86df7bc92b4bc42539)
// lines 1825-1833. Byte-exact; do not edit. Requires StringType and
// File::CharType in scope (target parity: 2-byte wchar_t).
            auto widen_ascii = [](const std::string& narrow) -> StringType {
                StringType wide;
                wide.reserve(narrow.size());
                for (const char c : narrow)
                {
                    wide.push_back(static_cast<File::CharType>(static_cast<unsigned char>(c)));
                }
                return wide;
            };
