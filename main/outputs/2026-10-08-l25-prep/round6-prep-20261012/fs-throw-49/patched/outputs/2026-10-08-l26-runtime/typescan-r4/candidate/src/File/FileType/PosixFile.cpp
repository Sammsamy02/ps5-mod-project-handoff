#if defined(__PROSPERO__) || defined(RC_FILE_USE_POSIX_BACKEND)
#include <File/File.hpp>
#include <File/FileType/PosixFile.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace RC::File
{
    namespace
    {
        constexpr size_t cache_header_bytes = 88;
        constexpr size_t cache_max_bytes = 0x500;
        constexpr std::array<uint8_t, 8> cache_magic{'R','C','P','S','5','F','C','1'};
        constexpr uint32_t cache_version = 1;
        constexpr uint32_t cache_platform = 0x00355350; // Fixed PS5-format tag, not an OS probe.
        std::atomic<uint64_t> temporary_sequence{};

        [[noreturn]] auto invalid(const char* message) -> void
        {
            Internal::StaticStorage::internal_error = true;
            throw std::runtime_error{message};
        }
        [[noreturn]] auto io_error(const char* operation) -> void
        {
            const int error = errno;
            Internal::StaticStorage::internal_error = true;
            throw std::system_error{error, std::generic_category(), operation};
        }
        // [FS-THROW-49 P1] PC parity for parent-directory creation. On PC,
        // WinFile::create_all_directories catches the filesystem error INSIDE
        // the file layer, sets internal_error and rethrows a File error
        // (std::runtime_error). The throwing std::filesystem overload used here
        // before let the error out without internal_error. Same result on
        // success; on error: internal_error = true + a std::system_error (a
        // std::runtime_error), exactly like io_error above.
        auto create_parent_directories(const std::filesystem::path& parent, const char* operation) -> void
        {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
            if (ec)
            {
                Internal::StaticStorage::internal_error = true;
                throw std::system_error{ec, std::string{operation} + " '" + parent.string() + "'"};
            }
        }
        auto validate_path(const std::filesystem::path& path) -> void
        {
            if (path.empty() || path.native().find('\0') != std::string::npos)
                invalid("File path is empty or contains a NUL");
        }
        auto stat_file(int fd) -> struct stat
        {
            struct stat value{};
            if (::fstat(fd, &value) != 0) io_error("PosixFile fstat");
            if (!S_ISREG(value.st_mode) || value.st_size < 0) invalid("File is not a regular file");
            return value;
        }
        auto same_inode(const struct stat& a, const struct stat& b) -> bool
        {
            return a.st_dev == b.st_dev && a.st_ino == b.st_ino;
        }
        auto read_exact(int fd, uint8_t* bytes, size_t count, off_t offset = 0) -> void
        {
            size_t done{};
            while (done < count)
            {
                const size_t request = std::min(count - done, static_cast<size_t>(SSIZE_MAX));
                const ssize_t actual = ::pread(fd, bytes + done, request, offset + static_cast<off_t>(done));
                if (actual < 0) { if (errno == EINTR) continue; io_error("PosixFile pread"); }
                if (actual == 0) invalid("File ended before its declared byte length");
                done += static_cast<size_t>(actual);
            }
        }
        auto write_all(int fd, const uint8_t* bytes, size_t count) -> void
        {
            size_t done{};
            while (done < count)
            {
                const ssize_t actual = ::write(fd, bytes + done, std::min(count - done, static_cast<size_t>(SSIZE_MAX)));
                if (actual < 0) { if (errno == EINTR) continue; io_error("PosixFile write"); }
                if (actual == 0) invalid("File write made no progress");
                done += static_cast<size_t>(actual);
            }
        }
        auto put_le(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t length) -> void
        {
            for (size_t i = 0; i < length; ++i) bytes.at(offset + i) = static_cast<uint8_t>(value >> (8 * i));
        }
        auto get_le(const std::vector<uint8_t>& bytes, size_t offset, size_t length) -> uint64_t
        {
            uint64_t value{};
            for (size_t i = 0; i < length; ++i) value |= static_cast<uint64_t>(bytes.at(offset + i)) << (8 * i);
            return value;
        }
        struct TemporaryFile
        {
            int fd{-1};
            std::filesystem::path path;
            bool owns_path{};
            ~TemporaryFile()
            {
                const int saved = errno;
                if (fd >= 0) ::close(fd);
                if (owns_path) ::unlink(path.c_str());
                errno = saved;
            }
        };
        auto reject_source_alias(int source, const std::filesystem::path& sidecar) -> void
        {
            TemporaryFile existing;
            existing.fd = ::open(sidecar.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
            if (existing.fd < 0)
            {
                if (errno == ENOENT) return;
                io_error("PosixFile open existing sidecar");
            }
            if (same_inode(stat_file(source), stat_file(existing.fd)))
                invalid("Serialization file aliases the opened source file");
        }
        auto append_utf8(std::vector<uint8_t>& out, uint32_t point) -> void
        {
            if (point <= 0x7f) out.push_back(static_cast<uint8_t>(point));
            else if (point <= 0x7ff)
            {
                out.push_back(static_cast<uint8_t>(0xc0 | (point >> 6)));
                out.push_back(static_cast<uint8_t>(0x80 | (point & 0x3f)));
            }
            else if (point <= 0xffff)
            {
                out.push_back(static_cast<uint8_t>(0xe0 | (point >> 12)));
                out.push_back(static_cast<uint8_t>(0x80 | ((point >> 6) & 0x3f)));
                out.push_back(static_cast<uint8_t>(0x80 | (point & 0x3f)));
            }
            else
            {
                out.push_back(static_cast<uint8_t>(0xf0 | (point >> 18)));
                out.push_back(static_cast<uint8_t>(0x80 | ((point >> 12) & 0x3f)));
                out.push_back(static_cast<uint8_t>(0x80 | ((point >> 6) & 0x3f)));
                out.push_back(static_cast<uint8_t>(0x80 | (point & 0x3f)));
            }
        }
        // Logwriter fix F3 (2026-10-08, CE FIX-PROPOSAL.md): appending form of
        // the whole-message UTF-16 -> UTF-8 encoder so the complete byte image
        // is staged before the first byte reaches the descriptor; the caller
        // flushes it with write_all while the output device holds its write
        // lock, which keeps a message atomic even across short ::write returns.
        auto encode_utf8_into(StringViewType text, std::vector<uint8_t>& result) -> void
        {
            result.reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i)
            {
                uint32_t point = static_cast<uint16_t>(text[i]);
                if (point >= 0xd800 && point <= 0xdbff)
                {
                    if (i + 1 == text.size()) invalid("Text has a truncated UTF-16 surrogate pair");
                    const uint32_t low = static_cast<uint16_t>(text[++i]);
                    if (low < 0xdc00 || low > 0xdfff) invalid("Text has an invalid UTF-16 surrogate pair");
                    point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00;
                }
                else if (point >= 0xdc00 && point <= 0xdfff) invalid("Text has an unpaired UTF-16 surrogate");
                append_utf8(result, point);
            }
        }
        auto decode_utf8(const std::vector<uint8_t>& bytes) -> StringType
        {
            StringType result;
            size_t at = bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf ? 3 : 0;
            while (at < bytes.size())
            {
                const uint8_t first = bytes[at++];
                uint32_t point{};
                size_t extra{};
                uint32_t minimum{};
                if (first <= 0x7f) point = first;
                else if (first >= 0xc2 && first <= 0xdf) { point = first & 0x1f; extra = 1; minimum = 0x80; }
                else if (first >= 0xe0 && first <= 0xef) { point = first & 0x0f; extra = 2; minimum = 0x800; }
                else if (first >= 0xf0 && first <= 0xf4) { point = first & 7; extra = 3; minimum = 0x10000; }
                else invalid("File contains malformed UTF-8");
                if (extra > bytes.size() - at) invalid("File contains truncated UTF-8");
                for (size_t i = 0; i < extra; ++i)
                {
                    const uint8_t next = bytes[at++];
                    if ((next & 0xc0) != 0x80) invalid("File contains an invalid UTF-8 continuation");
                    point = (point << 6) | (next & 0x3f);
                }
                if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
                    invalid("File contains an invalid UTF-8 code point");
                if (point <= 0xffff) result.push_back(static_cast<CharType>(point));
                else
                {
                    point -= 0x10000;
                    result.push_back(static_cast<CharType>(0xd800 | (point >> 10)));
                    result.push_back(static_cast<CharType>(0xdc00 | (point & 0x3ff)));
                }
            }
            return result;
        }
    }

    PosixFile::PosixFile(PosixFile&& other) noexcept { *this = std::move(other); }
    auto PosixFile::operator=(PosixFile&& other) noexcept -> PosixFile&
    {
        if (this == &other) return *this;
        close_noexcept();
        m_fd = std::exchange(other.m_fd, -1);
        m_mapping = std::exchange(other.m_mapping, nullptr);
        m_mapping_bytes = std::exchange(other.m_mapping_bytes, 0);
        m_mapping_active = std::exchange(other.m_mapping_active, false);
        m_properties = other.m_properties;
        m_path = std::move(other.m_path);
        m_serialization_path = std::move(other.m_serialization_path);
        m_serialization_config_failed = std::exchange(other.m_serialization_config_failed, false);
        m_cache_loaded = std::exchange(other.m_cache_loaded, false);
        m_identity_loaded = std::exchange(other.m_identity_loaded, false);
        m_cached_identity = other.m_cached_identity;
        m_cache = std::move(other.m_cache);
        m_next_item = std::exchange(other.m_next_item, 0);
        m_item = other.m_item;
        return *this;
    }
    PosixFile::~PosixFile() noexcept { close_noexcept(); }
    auto PosixFile::is_valid() noexcept -> bool { return m_fd >= 0; }
    auto PosixFile::is_file_open() const noexcept -> bool { return m_fd >= 0; }
    auto PosixFile::get_file_descriptor() const noexcept -> int { return m_fd; }
    auto PosixFile::get_raw_handle() noexcept -> void* { return m_fd >= 0 ? &m_fd : nullptr; }
    auto PosixFile::get_file_path() const -> const std::filesystem::path& { return m_path; }
    auto PosixFile::invalidate_file() noexcept -> void
    {
        // Deliberate ownership release for the original transfer contract.
        m_fd = -1; m_mapping = nullptr; m_mapping_bytes = 0; m_mapping_active = false;
    }
    auto PosixFile::close_noexcept() noexcept -> void
    {
        const int saved = errno;
        if (std::exchange(m_mapping_active, false)) ::munmap(m_mapping, m_mapping_bytes);
        m_mapping = nullptr; m_mapping_bytes = 0;
        const int fd = std::exchange(m_fd, -1);
        if (fd >= 0) ::close(fd); // Never retry close: the descriptor may already be released.
        errno = saved;
    }
    auto PosixFile::close_current_file() -> void
    {
        int first_error{};
        if (std::exchange(m_mapping_active, false) && ::munmap(m_mapping, m_mapping_bytes) != 0) first_error = errno;
        m_mapping = nullptr; m_mapping_bytes = 0;
        const int fd = std::exchange(m_fd, -1);
        if (fd >= 0 && ::close(fd) != 0 && !first_error) first_error = errno;
        if (first_error) { errno = first_error; io_error("PosixFile close/unmap"); }
    }
    auto PosixFile::require_open() const -> void { if (m_fd < 0) invalid("File is closed"); }
    auto PosixFile::open_file(const std::filesystem::path& path, const OpenProperties& properties) -> PosixFile
    {
        validate_path(path);
        int flags = O_CLOEXEC;
        switch (properties.open_for)
        {
        case OpenFor::Reading: flags |= O_RDONLY; break;
        case OpenFor::Writing: flags |= O_WRONLY; break;
        case OpenFor::Appending: flags |= O_WRONLY | O_APPEND; break;
        case OpenFor::ReadWrite: flags |= O_RDWR; break;
        default: invalid("Invalid File open mode");
        }
        if (properties.overwrite_existing_file != OverwriteExistingFile::Yes && properties.overwrite_existing_file != OverwriteExistingFile::No)
            invalid("Invalid File overwrite mode");
        if (properties.create_if_non_existent != CreateIfNonExistent::Yes && properties.create_if_non_existent != CreateIfNonExistent::No)
            invalid("Invalid File create mode");
        if (properties.overwrite_existing_file == OverwriteExistingFile::Yes) flags |= O_CREAT | O_TRUNC;
        else if (properties.create_if_non_existent == CreateIfNonExistent::Yes) flags |= O_CREAT;
        PosixFile result;
        result.m_path = path; result.m_properties = properties;
        if ((flags & O_CREAT) && !path.parent_path().empty()) create_parent_directories(path.parent_path(), "PosixFile create_directories");
        result.m_fd = ::open(path.c_str(), flags, 0600);
        if (result.m_fd < 0)
        {
            if (errno == ENOENT) throw FileNotFoundException{"PosixFile open: file not found"};
            io_error("PosixFile open");
        }
        (void)stat_file(result.m_fd);
        return result;
    }
    auto PosixFile::delete_file(const std::filesystem::path& path) -> void
    {
        validate_path(path);
        if (::unlink(path.c_str()) != 0) io_error("PosixFile unlink");
    }
    auto PosixFile::delete_file() -> void { close_current_file(); delete_file(m_path); }
    auto PosixFile::read_all() const -> StringType
    {
        require_open();
        if (m_properties.open_for != OpenFor::Reading && m_properties.open_for != OpenFor::ReadWrite)
            invalid("File was not opened for reading");
        const auto before = live_identity();
        if (before.bytes > SIZE_MAX || before.bytes > static_cast<uint64_t>(std::numeric_limits<off_t>::max()))
            invalid("File byte length cannot be represented");
        std::vector<uint8_t> bytes(static_cast<size_t>(before.bytes));
        read_exact(m_fd, bytes.data(), bytes.size());
        if (!(live_identity() == before)) invalid("File changed during read");
        return decode_utf8(bytes);
    }
    auto PosixFile::write_string_to_file(StringViewType text) -> void
    {
        require_open();
        if (m_properties.open_for == OpenFor::Reading) invalid("File was not opened for writing");
        // Logwriter fix F3: stage the COMPLETE encoded message, then flush it
        // with write_all while the calling output device holds its write lock
        // (message-atomic under concurrency). Stack-local staging (NOT
        // thread_local): a thread_local with a destructor registers
        // __cxa_thread_atexit_impl, breaking the frozen import contract; a
        // local vector frees at scope exit and keeps atomicity.
        std::vector<uint8_t> staged;
        encode_utf8_into(text, staged);
        write_all(m_fd, staged.data(), staged.size());
    }
    auto PosixFile::memory_map() -> std::span<uint8_t>
    {
        require_open();
        const auto info = stat_file(m_fd);
        if (static_cast<uint64_t>(info.st_size) > SIZE_MAX) invalid("File mapping length cannot be represented");
        const size_t size = static_cast<size_t>(info.st_size);
        if (m_mapping_active)
        {
            if (size != m_mapping_bytes) invalid("Mapped file size changed; close before remapping");
            return {m_mapping, m_mapping_bytes};
        }
        if (!size) return {};
        const int protection = m_properties.open_for == OpenFor::Reading ? PROT_READ : PROT_READ | PROT_WRITE;
        const int sharing = m_properties.open_for == OpenFor::Reading ? MAP_PRIVATE : MAP_SHARED;
        void* map = ::mmap(nullptr, size, protection, sharing, m_fd, 0);
        if (map == MAP_FAILED) io_error("PosixFile mmap");
        m_mapping = static_cast<uint8_t*>(map); m_mapping_bytes = size; m_mapping_active = true;
        return {m_mapping, m_mapping_bytes};
    }
    auto PosixFile::live_identity() const -> Identity
    {
        require_open();
        const auto info = stat_file(m_fd);
#ifdef __APPLE__
        const auto modification = info.st_mtimespec, change = info.st_ctimespec;
#else
        const auto modification = info.st_mtim, change = info.st_ctim;
#endif
        if (modification.tv_nsec < 0 || modification.tv_nsec >= 1000000000 || change.tv_nsec < 0 || change.tv_nsec >= 1000000000)
            invalid("File timestamps have invalid nanoseconds");
        return {static_cast<uint64_t>(info.st_dev), static_cast<uint64_t>(info.st_ino), static_cast<uint64_t>(info.st_size),
            static_cast<int64_t>(modification.tv_sec), static_cast<int64_t>(change.tv_sec),
            static_cast<uint32_t>(modification.tv_nsec), static_cast<uint32_t>(change.tv_nsec)};
    }
    auto PosixFile::is_same_as(PosixFile& other) -> bool { return live_identity() == other.live_identity(); }
    auto PosixFile::reset_cache() noexcept -> void
    {
        m_cache.clear(); m_cache_loaded = false; m_identity_loaded = false; m_next_item = 0; m_item.fill(0);
    }
    auto PosixFile::set_serialization_output_file(const std::filesystem::path& path) noexcept -> void
    {
        reset_cache();
        try { m_serialization_path = path; m_serialization_config_failed = false; }
        catch (...) { m_serialization_path.clear(); m_serialization_config_failed = true; Internal::StaticStorage::internal_error = true; }
    }
    auto PosixFile::require_serialization_path() const -> void
    {
        require_open();
        if (m_serialization_config_failed) invalid("Serialization path could not be retained");
        validate_path(m_serialization_path);
    }
    auto PosixFile::serialization_file_exists() -> bool
    {
        require_serialization_path();
        struct stat info{};
        if (::lstat(m_serialization_path.c_str(), &info) == 0)
        {
            if (!S_ISREG(info.st_mode)) invalid("Serialization file is not a regular file");
            return true;
        }
        if (errno == ENOENT) return false;
        io_error("PosixFile sidecar lstat");
    }
    auto PosixFile::build_identity_cache() const -> std::vector<uint8_t>
    {
        const auto identity = live_identity();
        std::vector<uint8_t> bytes(cache_header_bytes);
        std::copy(cache_magic.begin(), cache_magic.end(), bytes.begin());
        put_le(bytes, 8, cache_version, 4); put_le(bytes, 12, cache_header_bytes, 4);
        put_le(bytes, 16, bytes.size(), 8); put_le(bytes, 24, cache_platform, 4);
        put_le(bytes, 28, 16, 2); put_le(bytes, 30, sizeof(long), 2);
        put_le(bytes, 32, identity.device, 8); put_le(bytes, 40, identity.inode, 8); put_le(bytes, 48, identity.bytes, 8);
        put_le(bytes, 56, std::bit_cast<uint64_t>(identity.modification_seconds), 8); put_le(bytes, 64, identity.modification_nanoseconds, 4);
        put_le(bytes, 72, std::bit_cast<uint64_t>(identity.change_seconds), 8); put_le(bytes, 80, identity.change_nanoseconds, 4);
        return bytes;
    }
    auto PosixFile::persist_cache(const std::vector<uint8_t>& bytes) -> void
    {
        require_serialization_path();
        reject_source_alias(m_fd, m_serialization_path);
        if (!m_serialization_path.parent_path().empty()) create_parent_directories(m_serialization_path.parent_path(), "PosixFile cache create_directories");
        TemporaryFile temporary;
        for (unsigned attempt = 0; attempt < 32; ++attempt)
        {
            temporary.path = m_serialization_path;
            temporary.path += ".ue4ss-native-tmp-" + std::to_string(::getpid()) + "-" + std::to_string(temporary_sequence.fetch_add(1));
            temporary.fd = ::open(temporary.path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
            if (temporary.fd >= 0) { temporary.owns_path = true; break; }
            if (errno != EEXIST) io_error("PosixFile create cache temporary");
        }
        if (temporary.fd < 0) invalid("Could not allocate a private cache temporary name");
        write_all(temporary.fd, bytes.data(), bytes.size());
        const int fd = std::exchange(temporary.fd, -1);
        if (::close(fd) != 0) io_error("PosixFile close cache temporary");
        if (::rename(temporary.path.c_str(), m_serialization_path.c_str()) != 0) io_error("PosixFile commit cache");
        temporary.owns_path = false;
        reset_cache();
    }
    auto PosixFile::load_cache() -> void
    {
        require_serialization_path();
        if (m_cache_loaded) return;
        TemporaryFile file;
        file.fd = ::open(m_serialization_path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
        if (file.fd < 0) io_error("PosixFile open cache");
        const auto info = stat_file(file.fd);
        if (info.st_size < static_cast<off_t>(cache_header_bytes) || info.st_size > static_cast<off_t>(cache_max_bytes))
            invalid("Native cache byte length is invalid");
        std::vector<uint8_t> bytes(static_cast<size_t>(info.st_size));
        read_exact(file.fd, bytes.data(), bytes.size());
        const auto after = stat_file(file.fd);
        if (info.st_size != after.st_size || !same_inode(info, after)) invalid("Cache changed during read");
        if (!std::equal(cache_magic.begin(), cache_magic.end(), bytes.begin()) ||
            get_le(bytes, 8, 4) != cache_version || get_le(bytes, 12, 4) != cache_header_bytes ||
            get_le(bytes, 16, 8) != bytes.size() || get_le(bytes, 24, 4) != cache_platform ||
            get_le(bytes, 28, 2) != 16 || get_le(bytes, 30, 2) != sizeof(long) ||
            get_le(bytes, 68, 4) || get_le(bytes, 84, 4) || get_le(bytes, 64, 4) >= 1000000000 || get_le(bytes, 80, 4) >= 1000000000)
            invalid("Cache format/platform/version/length is invalid");
        size_t offset = cache_header_bytes;
        while (offset < bytes.size())
        {
            if (bytes.size() - offset < 8) invalid("Cache item header is truncated");
            const size_t item_bytes = static_cast<size_t>(get_le(bytes, offset, 4));
            if ((item_bytes != 4 && item_bytes != 8) || get_le(bytes, offset + 4, 4) || item_bytes > bytes.size() - offset - 8)
                invalid("Cache item has invalid width or length");
            offset += 8 + item_bytes;
        }
        m_cached_identity = {get_le(bytes,32,8),get_le(bytes,40,8),get_le(bytes,48,8),
            std::bit_cast<int64_t>(get_le(bytes,56,8)),std::bit_cast<int64_t>(get_le(bytes,72,8)),
            static_cast<uint32_t>(get_le(bytes,64,4)),static_cast<uint32_t>(get_le(bytes,80,4))};
        m_cache = std::move(bytes); m_cache_loaded = true; m_identity_loaded = true; m_next_item = cache_header_bytes;
    }
    auto PosixFile::serialize_identifying_properties() -> void { require_serialization_path(); persist_cache(build_identity_cache()); }
    auto PosixFile::deserialize_identifying_properties() -> void { reset_cache(); load_cache(); }
    auto PosixFile::is_deserialized_and_live_equal() -> bool
    {
        require_serialization_path();
        if (!m_identity_loaded)
        {
            if (!serialization_file_exists()) return false;
            load_cache();
        }
        return m_cached_identity == live_identity();
    }
    auto PosixFile::invalidate_serialization() -> void
    {
        require_serialization_path(); reject_source_alias(m_fd, m_serialization_path); reset_cache();
        if (::unlink(m_serialization_path.c_str()) != 0 && errno != ENOENT) io_error("PosixFile invalidate cache");
    }
    auto PosixFile::serialize_integral_value(uint64_t bits, uint32_t count) -> void
    {
        require_serialization_path();
        if (count != 4 && count != 8) invalid("Unsupported native serialized integer width");
        if (serialization_file_exists()) load_cache();
        std::vector<uint8_t> bytes = m_cache_loaded && m_cached_identity == live_identity() ? m_cache : build_identity_cache();
        if (8 + count > cache_max_bytes - bytes.size()) invalid("Native cache capacity is exhausted");
        const size_t offset = bytes.size(); bytes.resize(offset + 8 + count);
        put_le(bytes, offset, count, 4); put_le(bytes, offset + 8, bits, count); put_le(bytes, 16, bytes.size(), 8);
        persist_cache(bytes);
    }
    auto PosixFile::serialize_item(const GenericItemData& data, bool internal) -> void
    {
        if (internal) invalid("Native identifying properties are serialized as one versioned record");
        switch (data.data_type)
        {
        case GenericDataType::UnsignedLong: serialize_integral_value(data.data_ulong, sizeof(data.data_ulong)); break;
        case GenericDataType::SignedLong: serialize_integral_value(static_cast<unsigned long>(data.data_long), sizeof(data.data_long)); break;
        case GenericDataType::UnsignedLongLong: serialize_integral_value(data.data_ulonglong, sizeof(data.data_ulonglong)); break;
        case GenericDataType::SignedLongLong: serialize_integral_value(static_cast<unsigned long long>(data.data_longlong), sizeof(data.data_longlong)); break;
        default: invalid("Unsupported native serialized item type");
        }
    }
    auto PosixFile::get_serialized_item(size_t count, bool internal) -> void*
    {
        if (internal) invalid("Native identifying properties are decoded as one versioned record");
        load_cache();
        if (!(m_cached_identity == live_identity())) invalid("Cached source identity is stale");
        if (m_next_item >= m_cache.size()) invalid("No serialized item remains");
        const size_t stored = static_cast<size_t>(get_le(m_cache, m_next_item, 4));
        if (count != stored) invalid("Serialized item width differs from requested type");
        const uint64_t bits = get_le(m_cache, m_next_item + 8, count);
        m_item.fill(0);
        if (count == 4) { const uint32_t value = static_cast<uint32_t>(bits); std::memcpy(m_item.data(), &value, sizeof(value)); }
        else std::memcpy(m_item.data(), &bits, sizeof(bits));
        m_next_item += 8 + count;
        return m_item.data();
    }
} // namespace RC::File
#endif
