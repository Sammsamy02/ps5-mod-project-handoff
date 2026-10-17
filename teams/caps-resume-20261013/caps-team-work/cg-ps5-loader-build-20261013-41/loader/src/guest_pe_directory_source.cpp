#include "guest_pe_directory_source.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <new>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace cap13 {
namespace {

constexpr size_t kMaxDirectoryEntries = 4096;
constexpr size_t kMaxDependencyBytes = 512u * 1024u * 1024u;

char lower_ascii(char ch)
{
    return ch >= 'A' && ch <= 'Z' ? char(ch - 'A' + 'a') : ch;
}

bool same_basename(const char* a, const char* b)
{
    if (!a || !b) return false;
    for (; *a && *b; ++a, ++b)
        if (lower_ascii(*a) != lower_ascii(*b)) return false;
    return !*a && !*b;
}

bool plain_import_name(const char* name)
{
    if (!name || !*name) return false;
    for (const char* at = name; *at; ++at)
        if (*at == '/' || *at == '\\' || *at == ':' ||
            static_cast<unsigned char>(*at) < 0x20) return false;
    return std::strcmp(name, ".") && std::strcmp(name, "..");
}

} // namespace

GuestPeDirectorySource::GuestPeDirectorySource(std::string folder,
                                               GuestSegmentSwitch* segment)
    : folder_(std::move(folder)), segment_(segment)
{
    if (folder_.empty() || !segment_)
    {
        last_error_ = "missing dependency folder or segment backend";
        return;
    }
    directory_fd_ = open(folder_.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory_fd_ < 0)
        last_error_ = "dependency folder is unavailable or is a symlink";
}

GuestPeDirectorySource::~GuestPeDirectorySource()
{
    if (directory_fd_ >= 0) close(directory_fd_);
}

bool GuestPeDirectorySource::fetch(void* user, const char* import_dll,
                                  GuestPeFetchedDependency* out)
{
    return user && static_cast<GuestPeDirectorySource*>(user)->fetch_one(import_dll, out);
}

bool GuestPeDirectorySource::fetch_one(const char* import_dll,
                                      GuestPeFetchedDependency* out)
{
    last_error_.clear();
    if (!ready() || !out || !plain_import_name(import_dll))
    {
        last_error_ = "invalid dependency source or imported basename";
        return false;
    }
    // dup() shares the underlying directory offset. A new open file
    // description is required so repeated dependency lookups start at entry 0.
    const int scan_fd = openat(directory_fd_, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (scan_fd < 0)
    {
        last_error_ = "could not duplicate dependency directory handle";
        return false;
    }
    DIR* directory = fdopendir(scan_fd);
    if (!directory)
    {
        close(scan_fd);
        last_error_ = "could not enumerate dependency directory";
        return false;
    }
    std::string found;
    size_t count = 0;
    for (;;)
    {
        errno = 0;
        dirent* entry = readdir(directory);
        if (!entry)
        {
            if (errno && last_error_.empty())
                last_error_ = "dependency directory enumeration failed";
            break;
        }
        if (!std::strcmp(entry->d_name, ".") ||
            !std::strcmp(entry->d_name, "..")) continue;
        if (++count > kMaxDirectoryEntries)
        {
            last_error_ = "dependency directory entry cap exceeded";
            break;
        }
        if (!same_basename(entry->d_name, import_dll)) continue;
        if (!found.empty())
        {
            last_error_ = "ambiguous case-insensitive dependency basename";
            break;
        }
        found = entry->d_name;
    }
    closedir(directory);
    if (!last_error_.empty()) return false;
    if (found.empty())
    {
        last_error_ = "dependency basename not present";
        return false;
    }
    const int file_fd = openat(directory_fd_, found.c_str(),
                               O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file_fd < 0)
    {
        last_error_ = "dependency is not a regular file or is a symlink";
        return false;
    }
    struct stat before{};
    if (fstat(file_fd, &before) != 0 || !S_ISREG(before.st_mode) ||
        before.st_size <= 0 || uint64_t(before.st_size) > kMaxDependencyBytes)
    {
        close(file_fd);
        last_error_ = "dependency is not a regular bounded file";
        return false;
    }
    std::vector<uint8_t> bytes;
    try { bytes.resize(static_cast<size_t>(before.st_size)); }
    catch (const std::bad_alloc&)
    {
        close(file_fd);
        last_error_ = "dependency buffer allocation failed";
        return false;
    }
    size_t offset = 0;
    while (offset < bytes.size())
    {
        const ssize_t got = read(file_fd, bytes.data() + offset,
                                 bytes.size() - offset);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) break;
        offset += static_cast<size_t>(got);
    }
    struct stat after{};
    const bool stable = fstat(file_fd, &after) == 0 &&
                        before.st_dev == after.st_dev &&
                        before.st_ino == after.st_ino &&
                        before.st_size == after.st_size &&
                        before.st_mtime == after.st_mtime;
    close(file_fd);
    if (offset != bytes.size() || !stable)
    {
        last_error_ = "dependency changed or could not be read completely";
        return false;
    }
    out->source_path = folder_ + "/" + found;
    out->bytes = std::move(bytes);
    out->segment = segment_;
    return true;
}

} // namespace cap13
