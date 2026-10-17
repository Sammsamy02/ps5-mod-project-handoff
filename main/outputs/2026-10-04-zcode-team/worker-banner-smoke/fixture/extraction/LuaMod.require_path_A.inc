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
using modbridge::normalize_path_for_lua;
