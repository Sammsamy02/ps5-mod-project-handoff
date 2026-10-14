// Host-link glue ONLY (worker-settings-v3-package): satisfies the reference to
// RC::File::PosixFile::read_all() pulled in by the Ini.cpp file-handle parse
// overload under RC_FILE_USE_POSIX_BACKEND, which settings_parse_test_v3.cpp
// never calls (it uses the StringType& overload). Same role as
// worker-engineversion-ini's linuxfile_stub.cpp (their build used the Linux
// backend, hence LinuxFile; this build uses the POSIX backend, hence
// PosixFile). Never executed; no engine behavior.
#include <File/File.hpp>
namespace RC::File {
auto PosixFile::read_all() const -> StringType { throw std::logic_error("stub: not used by host parse oracle"); }
}
