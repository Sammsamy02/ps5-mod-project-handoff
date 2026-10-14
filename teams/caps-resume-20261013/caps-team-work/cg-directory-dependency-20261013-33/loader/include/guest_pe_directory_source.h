// POSIX directory-backed source for immediate neighboring PE DLLs.
#ifndef CAP13_GUEST_PE_DIRECTORY_SOURCE_H
#define CAP13_GUEST_PE_DIRECTORY_SOURCE_H

#include "guest_pe_batch.h"

#include <string>

namespace cap13 {

// Single-threaded adapter for FetchPeDependency. Keeps a directory handle
// open, rejects ambiguous case-insensitive names and unsafe file types, and
// reads a bounded snapshot. The segment backend outlives attached modules.
class GuestPeDirectorySource
{
  public:
    GuestPeDirectorySource(std::string folder, GuestSegmentSwitch* segment);
    ~GuestPeDirectorySource();
    GuestPeDirectorySource(const GuestPeDirectorySource&) = delete;
    GuestPeDirectorySource& operator=(const GuestPeDirectorySource&) = delete;

    bool ready() const { return directory_fd_ >= 0 && segment_ != nullptr; }
    const std::string& last_error() const { return last_error_; }
    static bool fetch(void* user, const char* import_dll,
                      GuestPeFetchedDependency* out);

  private:
    bool fetch_one(const char* import_dll, GuestPeFetchedDependency* out);
    std::string folder_;
    GuestSegmentSwitch* segment_ = nullptr;
    int directory_fd_ = -1;
    std::string last_error_;
};

} // namespace cap13
#endif
