#include "analysis.h"
#include "archive/intake.h"
#include "archive/groups.h"
#include <archive.h>
#include <array>
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <string_view>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace {
using Status = mb_archive_analysis_status_v1;
constexpr uint64_t second = UINT64_C(1000000000);
constexpr uint64_t millisecond = UINT64_C(1000000);
void wipe(void *p, size_t n) noexcept {
    auto *bytes = static_cast<volatile unsigned char *>(p);
    for(size_t i = 0; i < n; ++i) bytes[i] = 0;
}
struct Range { uintptr_t begin, end; };
bool range(const void *p, size_t n, Range& r) noexcept {
    const auto start = reinterpret_cast<uintptr_t>(p);
    if((!p && n) || n > UINTPTR_MAX - start) return false;
    r = {start, start + n}; return true;
}
bool overlaps(Range a, Range b) noexcept {
    return a.begin != a.end && b.begin != b.end && a.begin < b.end && b.begin < a.end;
}
bool layout(const mb_archive_analysis_params_v1 *p,
            mb_archive_analysis_result_v1 *r) noexcept {
    std::array<Range, 4> spans{};
    if(!p || !r || reinterpret_cast<uintptr_t>(p) % alignof(mb_archive_analysis_params_v1) ||
       reinterpret_cast<uintptr_t>(r) % alignof(mb_archive_analysis_result_v1) ||
       !range(p, sizeof(*p), spans[0]) || !range(r, sizeof(*r), spans[1]) ||
       p->struct_size != sizeof(*p) || p->abi_version != 1 ||
       !p->report_capacity || p->report_capacity > MB_ARCHIVE_ANALYSIS_OUTPUT_LIMIT ||
       !range(p->report, p->report_capacity, spans[2]) ||
       !range(p->cancel_context, p->cancel_context ? 1 : 0, spans[3])) return false;
    for(size_t i = 0; i < spans.size(); ++i)
        for(size_t j = 0; j < i; ++j) if(overlaps(spans[i], spans[j])) return false;
    return true;
}
struct Output {
    char *data; size_t capacity; bool committed = false;
    ~Output() { if(!committed) wipe(data, capacity); }
};
struct Rendered {
    std::array<char, MB_ARCHIVE_ANALYSIS_OUTPUT_LIMIT> bytes{};
    bool cleared = false;
    void clear() noexcept { wipe(bytes.data(), bytes.size()); cleared = true; }
    ~Rendered() { if(!cleared) clear(); }
};
struct Operation {
    const mb_archive_analysis_params_v1& p;
    uint64_t last = 0, deadline = 0;
    bool have_sample = false, started = false;
    Status fault = MB_ARCHIVE_ANALYSIS_OK;
    bool sample() noexcept {
        timespec t{}; errno = 0;
        const int rc = clock_gettime(CLOCK_MONOTONIC, &t), error = errno;
        if(rc || error || t.tv_sec < 0 || t.tv_nsec < 0 ||
           t.tv_nsec >= static_cast<long>(second) ||
           static_cast<uint64_t>(t.tv_sec) > (UINT64_MAX - second + 1) / second) {
            fault = MB_ARCHIVE_ANALYSIS_CLOCK_FAILED; return false;
        }
        const uint64_t now = static_cast<uint64_t>(t.tv_sec) * second + static_cast<uint64_t>(t.tv_nsec);
        if(have_sample && now < last) { fault = MB_ARCHIVE_ANALYSIS_CLOCK_FAILED; return false; }
        last = now; have_sample = true;
        if(started && now >= deadline && fault != MB_ARCHIVE_ANALYSIS_CLOCK_FAILED)
            fault = MB_ARCHIVE_ANALYSIS_TIMEOUT;
        return fault == MB_ARCHIVE_ANALYSIS_OK;
    }
    bool start() noexcept {
        if(!sample() || p.timeout_ms > (UINT64_MAX - last) / millisecond) {
            fault = MB_ARCHIVE_ANALYSIS_CLOCK_FAILED; return false;
        }
        deadline = last + p.timeout_ms * millisecond; started = true; return true;
    }
    bool check() noexcept {
        if(!sample()) return false;
        if(p.cancelled) {
            try { if(p.cancelled(p.cancel_context)) fault = MB_ARCHIVE_ANALYSIS_CANCELLED; }
            catch(...) { fault = MB_ARCHIVE_ANALYSIS_CANCELLED; }
        }
        return sample();
    }
};
int cancelled(void *opaque) noexcept { return static_cast<Operation *>(opaque)->check() ? 0 : 1; }
struct Work {
    std::vector<mb_archive_row_v1> rows{MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT};
    std::vector<char> names = std::vector<char>(MB_ARCHIVE_ANALYSIS_NAME_LIMIT);
    std::vector<uint32_t> sort = std::vector<uint32_t>(MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT);
    std::vector<uint32_t> group_sort = std::vector<uint32_t>(MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT);
    std::vector<mb_archive_group_v1> groups{MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT};
    std::vector<mb_archive_group_member_v1> members{MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT};
    std::vector<char> keys = std::vector<char>(MB_ARCHIVE_ANALYSIS_NAME_LIMIT);
    std::array<uint8_t, MB_ARCHIVE_CHUNK_BYTES> chunk{};
    std::array<uint8_t, 65557> zip_tail{};
    std::array<uint8_t, 46> zip_header{};
    ~Work() {
        wipe(rows.data(), rows.size() * sizeof(rows[0])); wipe(names.data(), names.size());
        wipe(sort.data(), sort.size() * sizeof(sort[0])); wipe(group_sort.data(), group_sort.size() * sizeof(group_sort[0]));
        wipe(groups.data(), groups.size() * sizeof(groups[0])); wipe(members.data(), members.size() * sizeof(members[0]));
        wipe(keys.data(), keys.size()); wipe(chunk.data(), chunk.size());
        wipe(zip_tail.data(), zip_tail.size()); wipe(zip_header.data(), zip_header.size());
    }
};
bool source(const mb_archive_analysis_params_v1& p, struct stat& s) noexcept {
    errno = 0; const int flags = fcntl(p.fd, F_GETFL); const int flag_error = errno;
    errno = 0; const int rc = fstat(p.fd, &s); const int stat_error = errno;
    return flags >= 0 && !flag_error && !rc && !stat_error && (flags & O_ACCMODE) == O_RDONLY &&
           S_ISREG(s.st_mode) && s.st_size >= 0 && static_cast<uint64_t>(s.st_dev) == p.expected_device &&
           static_cast<uint64_t>(s.st_ino) == p.expected_inode && static_cast<uint64_t>(s.st_size) == p.expected_bytes;
}
bool unchanged(const struct stat& a, const struct stat& b) noexcept {
#ifdef __APPLE__
    return a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec &&
           a.st_ctimespec.tv_sec == b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec == b.st_ctimespec.tv_nsec;
#else
    return a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
           a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
#endif
}
uint16_t le16(const uint8_t *p) noexcept {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}
uint32_t le32(const uint8_t *p) noexcept {
    return static_cast<uint32_t>(le16(p)) | (static_cast<uint32_t>(le16(p + 2)) << 16);
}
int read_exact(Operation& op, uint64_t offset, void *buffer, size_t length) noexcept {
    if(!op.check()) return MB_ARCHIVE_CANCELLED;
    if(offset > op.p.expected_bytes || length > op.p.expected_bytes - offset)
        return MB_ARCHIVE_READER_REJECTED;
    errno = 0;
    const ssize_t n = pread(op.p.fd, buffer, length, static_cast<off_t>(offset));
    const int error = errno;
    if(!op.check()) return MB_ARCHIVE_CANCELLED;
    return n < 0 || error ? MB_ARCHIVE_IO_ERROR :
           static_cast<size_t>(n) != length ? MB_ARCHIVE_SOURCE_CHANGED : MB_ARCHIVE_OK;
}
int ordinary_zip_extra(Operation& op, Work& work, uint64_t offset, uint16_t length,
                       uint64_t& examined) noexcept {
    if(!length) return MB_ARCHIVE_OK;
    constexpr uint64_t limit = UINT64_C(67108864);
    if(length > limit - examined) return MB_ARCHIVE_LIMIT;
    examined += length;
    int code = read_exact(op, offset, work.chunk.data(), length); if(code) return code;
    size_t at = 0; uint32_t fields = 0;
    while(at < length) {
        if(length - at < 4) return MB_ARCHIVE_READER_REJECTED;
        const auto id = le16(work.chunk.data() + at), size = le16(work.chunk.data() + at + 2);
        if(id == 1) return MB_ARCHIVE_UNSUPPORTED_ENTRY; // APPNOTE 4.5.3: ZIP64 extended information.
        at += 4;
        if(size > length - at) return MB_ARCHIVE_READER_REJECTED;
        at += size;
        if((++fields & 63u) == 0 && !op.check()) return MB_ARCHIVE_CANCELLED;
    }
    return MB_ARCHIVE_OK;
}
int zip_complete(Operation& op, const mb_archive_inventory_v1& scan, Work& work) noexcept {
    if((scan.format_code & ARCHIVE_FORMAT_BASE_MASK) != ARCHIVE_FORMAT_ZIP) return MB_ARCHIVE_OK;
    if(scan.filter_count != 1) return MB_ARCHIVE_UNSUPPORTED_ENTRY;
    // PKWARE APPNOTE 4.3.12/4.3.16: one complete ordinary central directory
    // and an EOCD whose comment ends at EOF. A streaming ZIP fallback alone
    // also accepts complete local entries after the actual footer was lost.
    const size_t tail_size = static_cast<size_t>(op.p.expected_bytes < work.zip_tail.size() ?
                                               op.p.expected_bytes : work.zip_tail.size());
    if(tail_size < 22) return MB_ARCHIVE_READER_REJECTED;
    const uint64_t tail_offset = op.p.expected_bytes - tail_size;
    int code = read_exact(op, tail_offset, work.zip_tail.data(), tail_size);
    if(code) return code;
    size_t eocd = tail_size; bool unsupported = false;
    uint64_t directory_offset = 0, directory_size = 0;
    for(size_t at = tail_size - 22 + 1; at; ) {
        --at; const auto *p = work.zip_tail.data() + at;
        if(le32(p) != UINT32_C(0x06054b50) || le16(p + 20) != tail_size - at - 22) continue;
        const uint64_t absolute = tail_offset + at;
        if(absolute >= 20) {
            const uint8_t *locator = nullptr;
            if(at >= 20) locator = work.zip_tail.data() + at - 20;
            else {
                code = read_exact(op, absolute - 20, work.zip_header.data(), 4);
                if(code) return code;
                locator = work.zip_header.data();
            }
            if(le32(locator) == UINT32_C(0x07064b50)) { unsupported = true; continue; }
        }
        const auto count = le16(p + 10);
        const auto size = le32(p + 12), offset = le32(p + 16);
        if(le16(p + 4) || le16(p + 6) || count == UINT16_MAX ||
           size == UINT32_MAX || offset == UINT32_MAX) { unsupported = true; continue; }
        if(le16(p + 8) != count || count != scan.row_count ||
           offset > absolute || size != absolute - offset) continue;
        eocd = at; directory_offset = offset; directory_size = size; break;
    }
    if(eocd == tail_size) return unsupported ? MB_ARCHIVE_UNSUPPORTED_ENTRY : MB_ARCHIVE_READER_REJECTED;
    const uint64_t directory_end = directory_offset + directory_size;
    uint64_t position = directory_offset, extra_bytes = 0;
    for(uint32_t i = 0; i < scan.row_count; ++i) {
        if(position > directory_end || directory_end - position < 46) return MB_ARCHIVE_READER_REJECTED;
        code = read_exact(op, position, work.chunk.data(), 46); if(code) return code;
        const auto *central = work.chunk.data();
        if(le32(central) != UINT32_C(0x02014b50)) return MB_ARCHIVE_READER_REJECTED;
        const uint32_t compressed = le32(central + 20), expanded = le32(central + 24), local = le32(central + 42);
        const auto flags = le16(central + 8), method = le16(central + 10);
        const auto name_bytes = le16(central + 28), extra = le16(central + 30);
        if(le16(central + 34) || compressed == UINT32_MAX ||
           expanded == UINT32_MAX || local == UINT32_MAX) return MB_ARCHIVE_UNSUPPORTED_ENTRY;
        const uint64_t record_size = UINT64_C(46) + name_bytes + extra + le16(central + 32);
        if(!name_bytes || record_size > directory_end - position ||
           local > directory_offset || directory_offset - local < 30) return MB_ARCHIVE_READER_REJECTED;
        code = ordinary_zip_extra(op, work, position + 46 + name_bytes, extra, extra_bytes); if(code) return code;
        code = read_exact(op, local, work.zip_header.data(), 30); if(code) return code;
        const auto *header = work.zip_header.data();
        if(le32(header) != UINT32_C(0x04034b50)) return MB_ARCHIVE_READER_REJECTED;
        if(le32(header + 18) == UINT32_MAX ||
           le32(header + 22) == UINT32_MAX) return MB_ARCHIVE_UNSUPPORTED_ENTRY;
        const uint64_t local_size = UINT64_C(30) + le16(header + 26) + le16(header + 28);
        if(le16(header + 6) != flags || le16(header + 8) != method ||
           !le16(header + 26) || local_size > directory_offset - local ||
           compressed > directory_offset - local - local_size) return MB_ARCHIVE_READER_REJECTED;
        code = ordinary_zip_extra(op, work, local + 30 + le16(header + 26), le16(header + 28), extra_bytes);
        if(code) return code;
        position += record_size;
    }
    return position == directory_end ? MB_ARCHIVE_OK : MB_ARCHIVE_READER_REJECTED;
}
bool suffix(std::string_view name, std::string_view extension) noexcept {
    if(name.size() < extension.size()) return false;
    name.remove_prefix(name.size() - extension.size());
    for(size_t i = 0; i < name.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if(c >= 'A' && c <= 'Z') c = static_cast<unsigned char>(c + ('a' - 'A'));
        if(c != static_cast<unsigned char>(extension[i])) return false;
    } return true;
}
struct Counts { uint32_t regular = 0, directory = 0, pak = 0, iostore = 0, ini = 0, lua = 0, dll = 0, unresolved = 0; };
size_t render(const mb_archive_analysis_params_v1& p, const mb_archive_inventory_v1& scan,
              const mb_archive_groups_result_v1& grouping, const Counts& c, Rendered& out) noexcept {
    char sha[65]{}; constexpr char hex[] = "0123456789abcdef";
    for(size_t i = 0; i < 32; ++i) { sha[i * 2] = hex[p.expected_sha256[i] >> 4]; sha[i * 2 + 1] = hex[p.expected_sha256[i] & 15]; }
    const int n = std::snprintf(out.bytes.data(), out.bytes.size(),
        "{\"schema_version\":1,\"operation\":\"archive_inventory\",\"execution_context\":\"%s\","
        "\"operation_state\":\"supported\",\"compatibility_state\":\"%s\",\"reason_codes\":[%s"
        "\"missing_requirements\",\"platform_acceptance_missing\",\"unknown_target\"%s],"
        "\"install_authority\":{\"state\":\"unsupported\",\"reason\":\"install_operation_unavailable\",\"scope\":[]},"
        "\"runtime_supported\":false,\"installation_supported\":false,\"source_bytes\":%" PRIu64 ",\"source_sha256\":\"%s\","
        "\"member_count\":%u,\"regular_count\":%u,\"directory_count\":%u,\"expanded_bytes\":%" PRIu64 ","
        "\"group_count\":%u,\"unresolved_groups\":%u,\"candidate_detection\":\"filename_only\","
        "\"candidate_types\":{\"pak\":%u,\"iostore\":%u,\"ini\":%u,\"lua\":%u,\"dll\":%u}}",
        p.execution_context == MB_ARCHIVE_ANALYSIS_PS5_NATIVE ? "ps5_native" : "desktop_preparation",
        c.dll ? "unsupported" : "unverified", c.unresolved ? "\"inventory_unresolved\"," : "",
        c.dll ? ",\"windows_runtime_unavailable\"" : "", p.expected_bytes, sha,
        scan.row_count, c.regular, c.directory, scan.expanded_bytes, grouping.group_count, c.unresolved,
        c.pak, c.iostore, c.ini, c.lua, c.dll);
    wipe(sha, sizeof(sha)); return n > 0 && static_cast<size_t>(n) < out.bytes.size() ? static_cast<size_t>(n) : 0;
}
} // namespace

extern "C" int mb_archive_analyze_v1(const mb_archive_analysis_params_v1 *p,
                                    mb_archive_analysis_result_v1 *r) noexcept {
    if(!layout(p, r)) return MB_ARCHIVE_ANALYSIS_BAD_ARGUMENT;
    Output output{p->report, p->report_capacity};
    mb_archive_analysis_result_v1 out{}; out.struct_size = sizeof(out); out.abi_version = 1;
    Status status = MB_ARCHIVE_ANALYSIS_BAD_ARGUMENT; size_t length = 0;
    if(p->fd < 0 || p->reserved || p->execution_context < MB_ARCHIVE_ANALYSIS_DESKTOP_PREPARATION ||
       p->execution_context > MB_ARCHIVE_ANALYSIS_PS5_NATIVE || !p->expected_bytes ||
       p->expected_bytes > MB_ARCHIVE_ANALYSIS_SOURCE_LIMIT || !p->timeout_ms ||
       p->timeout_ms > MB_ARCHIVE_ANALYSIS_TIMEOUT_LIMIT_MS) { out.status = status; *r = out; return status; }
    Operation op{*p};
    if(!op.start()) status = op.fault;
    else {
        wipe(p->report, p->report_capacity);
        struct stat before{};
        if(!op.check()) status = op.fault;
        else if(!source(*p, before)) status = MB_ARCHIVE_ANALYSIS_SOURCE_REFUSED;
        else {
            try {
                auto work = std::make_unique<Work>();
                mb_archive_source_v1 input{}; input.struct_size = sizeof(input); input.abi_version = 1;
                input.fd = p->fd; input.size = p->expected_bytes; std::memcpy(input.sha256, p->expected_sha256, 32);
                mb_archive_limits_v1 limits{MB_ARCHIVE_ANALYSIS_SOURCE_LIMIT, MB_ARCHIVE_ANALYSIS_SOURCE_LIMIT,
                    MB_ARCHIVE_ANALYSIS_SOURCE_LIMIT * 2, MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT, MB_ARCHIVE_ANALYSIS_NAME_LIMIT};
                mb_archive_budget_v1 budget{}; budget.io_limit = MB_ARCHIVE_ANALYSIS_SOURCE_LIMIT * 16;
                budget.operation_limit = UINT64_C(1048576); budget.path_work_limit = UINT64_C(67108864);
                budget.deadline_ms = op.deadline / millisecond; budget.cancelled = cancelled; budget.context = &op;
                mb_archive_buffers_v1 buffers{work->rows.data(), MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT,
                    work->names.data(), MB_ARCHIVE_ANALYSIS_NAME_LIMIT, work->sort.data(), MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT,
                    work->chunk.data(), work->chunk.size()};
                mb_archive_inventory_v1 scan{}; out.scan_status = static_cast<uint32_t>(mb_archive_scan_v1(&input, &limits, &budget, &buffers, &scan));
                if(out.scan_status || scan.complete != 1 || scan.status || scan.installation_supported)
                    status = MB_ARCHIVE_ANALYSIS_SCAN_FAILED;
                else if((out.scan_status = static_cast<uint32_t>(zip_complete(op, scan, *work))) != 0)
                    status = MB_ARCHIVE_ANALYSIS_SCAN_FAILED;
                else if(!op.check()) status = op.fault;
                else {
                    mb_archive_groups_result_v1 grouping{};
                    out.group_status = static_cast<uint32_t>(mb_archive_groups_v1(&scan, work->rows.data(), MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT,
                        work->names.data(), MB_ARCHIVE_ANALYSIS_NAME_LIMIT, work->group_sort.data(), MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT,
                        work->groups.data(), MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT, work->members.data(), MB_ARCHIVE_ANALYSIS_ENTRY_LIMIT,
                        work->keys.data(), MB_ARCHIVE_ANALYSIS_NAME_LIMIT, &grouping));
                    if(out.group_status || grouping.complete != 1 || grouping.status || grouping.installation_supported || grouping.runtime_supported)
                        status = MB_ARCHIVE_ANALYSIS_GROUP_FAILED;
                    else {
                        Counts counts{};
                        for(uint32_t i = 0; i < scan.row_count; ++i) {
                            const auto& row = work->rows[i];
                            if(row.kind == MB_ARCHIVE_DIRECTORY) { ++counts.directory; continue; }
                            ++counts.regular; const std::string_view name(work->names.data() + row.path_offset, row.path_length);
                            counts.pak += suffix(name, ".pak"); counts.iostore += suffix(name, ".utoc") || suffix(name, ".ucas");
                            counts.ini += suffix(name, ".ini"); counts.lua += suffix(name, ".lua"); counts.dll += suffix(name, ".dll");
                        }
                        for(uint32_t i = 0; i < grouping.group_count; ++i)
                            counts.unresolved += work->groups[i].status == MB_ARCHIVE_GROUP_STATUS_UNRESOLVED;
                        Rendered rendered; length = render(*p, scan, grouping, counts, rendered);
                        if(!length) status = MB_ARCHIVE_ANALYSIS_INTERNAL_ERROR;
                        else if(length >= p->report_capacity) status = MB_ARCHIVE_ANALYSIS_OUTPUT_TOO_SMALL;
                        else { std::memcpy(p->report, rendered.bytes.data(), length + 1); status = MB_ARCHIVE_ANALYSIS_OK; }
                        rendered.clear();
                    }
                }
                work.reset();
            } catch(...) { status = MB_ARCHIVE_ANALYSIS_INTERNAL_ERROR; }
            // Large private erasure precedes callbacks and the last source/clock gates.
            if(!op.check()) status = op.fault;
            else if(status == MB_ARCHIVE_ANALYSIS_OK) {
                struct stat after{};
                if(!source(*p, after) || !unchanged(before, after)) status = MB_ARCHIVE_ANALYSIS_SOURCE_REFUSED;
                else if(!op.sample()) status = op.fault;
            }
        }
    }
    out.status = status;
    if(status == MB_ARCHIVE_ANALYSIS_OK) { output.committed = true; out.complete = 1; out.report_bytes = length; }
    *r = out; return status;
}
