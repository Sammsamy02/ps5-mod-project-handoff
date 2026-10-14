// Host-link glue ONLY (worker-engineversion-ini): satisfies the reference to
// RC::File::LinuxFile::read_all() pulled in by the Ini.cpp file-handle parse
// overload, which settings_parse_test_ev.cpp never calls (it uses the
// StringType& overload). Never executed; no engine behavior.
#include <File/File.hpp>
namespace RC::File {
auto LinuxFile::read_all() const -> StringType { throw std::logic_error("stub: not used by host parse oracle"); }
}
