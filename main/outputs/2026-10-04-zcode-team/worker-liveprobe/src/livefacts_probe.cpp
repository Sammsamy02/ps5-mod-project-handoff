// livefacts_probe.cpp
//
// LIVEFACTS_PROBE — standalone native diagnostic payload (genuine SDK ELF
// ET_DYN with crt1 _start, NOT a PRX). Answers loader-fact questions in its
// own process by recording what the OS ACTUALLY returns; never fabricates.
//
// Output: one JSONL line per experiment appended to LIVEFACTS_LOG_PATH
// (/data/modbridge-livefacts-probe.jsonl). On log open failure the probe
// writes nothing and exits with code 90. Write/close failure -> exit 92.
// Success (all experiments + summary recorded) -> exit 0.
//
// Experiments (probe brief order; each guarded so a failure does not abort
// the rest — every experiment always emits exactly one record):
//   E1 e1_dtdebug_map   walk own _DYNAMIC -> DT_DEBUG -> r_debug.r_map
//   E2 e2_proc_pathname sysctl KERN_PROC_PATHNAME
//   E3 e3_exec_handle   sceKernelGetExecutableModuleHandle + 352B module info
//   E4 e4_module_list_unit sceKernelGetModuleList size-unit + spelling probe
//   E5 e5_proc_vmmap    sysctl KERN_PROC_VMMAP bounded growth (<= 4 MiB)
//   E6 e6_self_retention dlopen(self, RTLD_NOLOAD|RTLD_NOW) vs E3 handle
//   E7 e7_fw_version    kernel_get_fw_version() (crt1.o-provided, pinned
//                       header target/include/ps5/kernel.h)
//
// Bounds (probe brief): record <= 2KB; link-map entries <= 64; module-list
// handles <= 128; VMMAP buffer growth stops at 4 MiB. Single main thread.
#include "probe_abi.hpp"
#include "probe_record.hpp"

#include <dlfcn.h>
#include <errno.h>
#include <ps5/kernel.h>
#include <stdint.h>
#include <string.h>
#include <sys/elf64.h>
#include <sys/elf_common.h>
#include <sys/link_elf.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

// Linker-provided dynamic array of THIS image (standard ELF symbol; the
// final ET_DYN link defines it in the dynamic section; crt1.o references it).
extern "C" Elf64_Dyn _DYNAMIC[];

#ifndef LIVEFACTS_BUILD_ID
#define LIVEFACTS_BUILD_ID "unbuilt"
#endif
#ifndef LIVEFACTS_LOG_PATH
#define LIVEFACTS_LOG_PATH "/data/modbridge-livefacts-probe.jsonl"
#endif

namespace {

livefacts::LogSink g_sink;
int g_seq = 0;

// Cross-experiment facts (recorded honestly; E6 depends on E2/E3 outcomes).
char g_exe_path[1024];
int g_exe_path_ok = 0;
int64_t g_exec_handle_raw = 0;
int g_exec_handle_attempted = 0;

constexpr size_t kLinkMapCap = 64;      // brief cap for E1 entries
constexpr size_t kModuleListCap = 128;  // brief cap for E4 handles
constexpr size_t kVmmapMax = 4u << 20;  // brief cap for E5 buffer growth

// Appends a JSON string member "key":"0x<hex>" without a leading comma.
void kv_hex_str(livefacts::Buf &b, const char *key, uint64_t v) {
    livefacts::kv_key(b, key);
    livefacts::buf_ch(b, '"');
    livefacts::buf_raw(b, "0x");
    livefacts::buf_u64_hex(b, v);
    livefacts::buf_ch(b, '"');
}

void emit(livefacts::Buf &b) {
    livefacts::record_flush(b);
    if (livefacts::log_write_record(g_sink, b) != 0) {
        g_sink.failed = 1;
    }
}

// ---------------------------------------------------------------------------
// E1: own _DYNAMIC -> DT_DEBUG -> r_debug -> link-map walk.
// rc: 0 walk ok; -1 no DT_DEBUG entry; -2 entry present but value zero
// (rtld did not fill it); -3 r_debug.r_map null. errno recorded verbatim.
void e1_dtdebug_map() {
    const Elf64_Dyn *debug_dyn = nullptr;
    size_t steps = 0;
    int64_t rc = 0;
    for (const Elf64_Dyn *d = _DYNAMIC; steps < 256; ++d, ++steps) {
        if (d->d_tag == DT_NULL) {
            break;  // end of the dynamic array
        }
        if (d->d_tag == DT_DEBUG) {
            debug_dyn = d;
            break;
        }
    }
    const uint64_t dt_debug_value =
        debug_dyn != nullptr ? (uint64_t)debug_dyn->d_un.d_ptr : 0;
    if (debug_dyn == nullptr) {
        rc = -1;
    } else if (dt_debug_value == 0) {
        rc = -2;
    }
    struct link_map *lm = nullptr;
    if (rc == 0) {
        struct r_debug *rd = reinterpret_cast<struct r_debug *>(dt_debug_value);
        lm = rd->r_map;
        if (lm == nullptr) {
            rc = -3;
        }
    }
    const int64_t eno = errno;  // no OS call in this experiment; recorded
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e1_dtdebug_map", rc, eno,
                            LIVEFACTS_BUILD_ID);
    livefacts::kv_int(b, "dt_debug_present", debug_dyn != nullptr ? 1 : 0);
    kv_hex_str(b, "dt_debug_value", dt_debug_value);
    size_t emitted = 0;
    bool list_truncated = false;
    livefacts::buf_raw(b, ",\"entries\":[");
    for (struct link_map *it = lm; it != nullptr; it = it->l_next) {
        if (emitted >= kLinkMapCap) {
            list_truncated = true;
            break;
        }
        // Worst-case next element must fit or the record is marked truncated.
        if (!livefacts::buf_room(b, 8 + 18 + 2 + 6 * 64 + 8)) {
            list_truncated = true;
            break;
        }
        if (emitted != 0) {
            livefacts::buf_ch(b, ',');
        }
        livefacts::buf_ch(b, '"');
        livefacts::buf_raw(b, "0x");
        livefacts::buf_u64_hex(b, (uint64_t)(uintptr_t)it->l_addr);
        livefacts::buf_ch(b, '"');
        livefacts::buf_ch(b, ',');
        livefacts::buf_str_escaped(
            b, it->l_name != nullptr ? it->l_name : "", 64);
        ++emitted;
    }
    livefacts::buf_ch(b, ']');
    livefacts::kv_int(b, "entries_emitted", (int64_t)emitted);
    livefacts::kv_int(b, "entries_truncated", list_truncated ? 1 : 0);
    emit(b);
}

// ---------------------------------------------------------------------------
// E2: sysctl KERN_PROC_PATHNAME -> own executable path.
// rc: sysctl return; errno captured after the call.
void e2_proc_pathname() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    memset(g_exe_path, 0, sizeof g_exe_path);
    size_t len = sizeof g_exe_path - 1;
    const int rc = sysctl(mib, 4, g_exe_path, &len, nullptr, 0);
    const int64_t eno = (rc != 0) ? errno : 0;
    g_exe_path_ok = (rc == 0 && len > 0 && g_exe_path[0] != '\0');
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e2_proc_pathname", (int64_t)rc, eno,
                            LIVEFACTS_BUILD_ID);
    livefacts::kv_str(b, "path", g_exe_path, 256);
    livefacts::kv_int(b, "path_len", (int64_t)(g_exe_path_ok ? len : 0));
    livefacts::kv_int(b, "path_ok", g_exe_path_ok);
    emit(b);
}

// Validation oracle for E4 (pinned 352-byte ABI; success = rc 0 AND the
// kernel echoed size == 352). Writes through/errno are captured by callers.
int validate_handle(uint64_t h, int64_t *size_echo, int64_t *seg_count) {
    livefacts::ModuleInfo info;
    memset(&info, 0, sizeof info);
    info.size = sizeof info;  // 352
    const int r = sceKernelGetModuleInfo((livefacts::ModuleHandle)h, &info);
    *size_echo = (int64_t)info.size;
    *seg_count = (int64_t)info.segmentCount;
    return r;
}

// ---------------------------------------------------------------------------
// E3: UNPINNED sceKernelGetExecutableModuleHandle, raw int64 + errno, then
// the pinned module-info oracle on the returned value.
void e3_exec_handle() {
    errno = 0;
    const int64_t raw = sceKernelGetExecutableModuleHandle();
    const int64_t eno = errno;
    g_exec_handle_raw = raw;
    g_exec_handle_attempted = 1;
    const int plausible = (raw > 0 && raw <= 0xffffffffLL) ? 1 : 0;
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e3_exec_handle",
                            plausible ? 0 : -1, eno, LIVEFACTS_BUILD_ID);
    livefacts::kv_int(b, "raw", raw);
    kv_hex_str(b, "raw_hex", (uint64_t)raw);
    livefacts::kv_int(b, "plausible_handle", plausible);
    if (plausible) {
        livefacts::ModuleInfo info;
        memset(&info, 0, sizeof info);
        info.size = sizeof info;  // 352, pinned ABI
        errno = 0;
        const int mr = sceKernelGetModuleInfo((livefacts::ModuleHandle)raw,
                                              &info);
        const int64_t meno = errno;
        livefacts::kv_int(b, "info_rc", mr);
        livefacts::kv_int(b, "info_errno", meno);
        livefacts::kv_int(b, "size_echo", (int64_t)info.size);
        livefacts::kv_int(b, "size_echo_ok", info.size == 352 ? 1 : 0);
        livefacts::kv_int(b, "segment_count", (int64_t)info.segmentCount);
        // First 4 segment {perm,size} values, hex (brief E3): flat keys
        // seg0_perm seg0_size ... seg3_perm seg3_size (8 members).
        for (size_t i = 0; i < 8; ++i) {
            char key[16];
            key[0] = 's'; key[1] = 'e'; key[2] = 'g';
            key[3] = (char)('0' + i / 2);
            key[4] = '_';
            strcpy(key + 5, (i % 2) == 0 ? "perm" : "size");
            const uint64_t value =
                (i % 2) == 0
                    ? (uint64_t)(uint32_t)info.segmentInfo[i / 2].prot
                    : (uint64_t)info.segmentInfo[i / 2].size;
            kv_hex_str(b, key, value);
        }
    }
    emit(b);
}

// ---------------------------------------------------------------------------
// E4: sceKernelGetModuleList spelling + size-unit probe. NO SDK header
// declares it. Pinned prosperodev spelling is 3-arg (array,size,available);
// the probe brief supplies 4-arg (flags,size,handles,available). Both are
// called with size = 4*N (bytes reading) and size = N (elements reading).
// All six argument registers hold 0 or pointers into owned zeroed scratch,
// so any reinterpretation writes only inside our buffers or fails in-kernel.
void e4_module_list_unit() {
    static uint64_t region_rdi[4 * kModuleListCap];  // 3-arg array slot
    static uint64_t region_rdx[4 * kModuleListCap];  // 4-arg handles slot
    static uint64_t region_tail[4];                  // rcx/r8/r9 slots
    // Take the real entry address without a direct fn-ptr-to-fn-ptr cast
    // (-Wcast-function-type-mismatch): copy the address representation.
    ModuleList6 fn = nullptr;
    const void *entry_addr =
        reinterpret_cast<const void *>(&sceKernelGetModuleList);
    memcpy(&fn, &entry_addr, sizeof fn);
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e4_module_list_unit", 0, 0,
                            LIVEFACTS_BUILD_ID);
    livefacts::kv_int(b, "capacity", (int64_t)kModuleListCap);
    livefacts::kv_str(
        b, "api_note",
        "no SDK header; 3-arg prosperodev and 4-arg brief spellings both "
        "probed; size tried as 4N and N", 160);
    const char *ids[4] = {"f4_size4N", "f4_sizeN", "f3_size4N", "f3_sizeN"};
    const size_t sizes[4] = {4 * kModuleListCap, kModuleListCap,
                             4 * kModuleListCap, kModuleListCap};
    for (int v = 0; v < 4; ++v) {
        memset(region_rdi, 0, sizeof region_rdi);
        memset(region_rdx, 0, sizeof region_rdx);
        memset(region_tail, 0, sizeof region_tail);
        errno = 0;
        int r;
        if (v < 2) {
            // 4-arg reading: (flags=0, size, handles*, available*)
            r = fn(nullptr, sizes[v], region_rdx, region_tail, region_tail,
                   region_tail);
        } else {
            // 3-arg reading: (array*, size, available*)
            r = fn(region_rdi, sizes[v], region_rdx, region_tail, region_tail,
                   region_tail);
        }
        const int64_t eno = errno;
        // First plausible handle candidate from each output region.
        uint64_t cand_rdx = 0;
        for (size_t i = 0; i < 4 * kModuleListCap; ++i) {
            if (region_rdx[i] != 0 && region_rdx[i] <= 0xffffffffULL) {
                cand_rdx = region_rdx[i];
                break;
            }
        }
        uint64_t cand_rdi = 0;
        for (size_t i = 0; i < 4 * kModuleListCap; ++i) {
            if (region_rdi[i] != 0 && region_rdi[i] <= 0xffffffffULL) {
                cand_rdi = region_rdi[i];
                break;
            }
        }
        int64_t vrdx_rc = 0x7fffffff, vrdx_se = 0, vrdx_eno = 0;
        if (cand_rdx != 0) {
            errno = 0;
            int64_t seg = 0;
            vrdx_rc = validate_handle(cand_rdx, &vrdx_se, &seg);
            vrdx_eno = errno;
        }
        int64_t vrdi_rc = 0x7fffffff, vrdi_se = 0, vrdi_eno = 0;
        if (cand_rdi != 0) {
            errno = 0;
            int64_t seg = 0;
            vrdi_rc = validate_handle(cand_rdi, &vrdi_se, &seg);
            vrdi_eno = errno;
        }
        // Flat per-variant fields: <id>_rc/_eno/_avail/_rdx0/_rdi0 plus the
        // module-info validation outcomes for each region candidate.
        char key[40];
        const size_t plen = strlen(ids[v]);
        memcpy(key, ids[v], plen);
        key[plen] = '_';
        strcpy(key + plen + 1, "rc");
        livefacts::kv_int(b, key, r);
        strcpy(key + plen + 1, "eno");
        livefacts::kv_int(b, key, eno);
        strcpy(key + plen + 1, "avail_tail");
        livefacts::kv_int(b, key, (int64_t)region_tail[0]);
        strcpy(key + plen + 1, "rdx0");
        kv_hex_str(b, key, region_rdx[0]);
        strcpy(key + plen + 1, "rdi0");
        kv_hex_str(b, key, region_rdi[0]);
        strcpy(key + plen + 1, "vrdx_rc");
        livefacts::kv_int(b, key, vrdx_rc);
        strcpy(key + plen + 1, "vrdx_se");
        livefacts::kv_int(b, key, vrdx_se);
        strcpy(key + plen + 1, "vrdx_eno");
        livefacts::kv_int(b, key, vrdx_eno);
        strcpy(key + plen + 1, "vrdi_rc");
        livefacts::kv_int(b, key, vrdi_rc);
        strcpy(key + plen + 1, "vrdi_se");
        livefacts::kv_int(b, key, vrdi_se);
        strcpy(key + plen + 1, "vrdi_eno");
        livefacts::kv_int(b, key, vrdi_eno);
    }
    emit(b);
}

// ---------------------------------------------------------------------------
// E5: sysctl KERN_PROC_VMMAP with bounded retry-growth, cap 4 MiB.
void e5_proc_vmmap() {
    static uint8_t buffer[kVmmapMax] __attribute__((aligned(16)));
    const pid_t pid = getpid();
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, pid};
    size_t len = 65536;
    int rc = -1;
    int64_t eno = 0;
    unsigned attempts = 0;
    for (;;) {
        ++attempts;
        rc = sysctl(mib, 4, buffer, &len, nullptr, 0);
        if (rc == 0) {
            eno = 0;
            break;
        }
        eno = errno;
        if (eno == ENOMEM && len > 0 && len * 2 <= kVmmapMax) {
            len *= 2;
            continue;
        }
        break;  // non-ENOMEM failure or growth cap reached
    }
    int64_t entries = -1;
    int64_t remainder = -1;
    int64_t frc = (int64_t)rc;
    if (rc == 0) {
        remainder = (int64_t)(len % sizeof(struct kinfo_vmentry));
        entries = (int64_t)(len / sizeof(struct kinfo_vmentry));
    } else if (eno == ENOMEM) {
        frc = -3;  // exceeded the 4 MiB growth cap
    }
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e5_proc_vmmap", frc, eno,
                            LIVEFACTS_BUILD_ID);
    livefacts::kv_int(b, "final_buf", (int64_t)len);
    livefacts::kv_int(b, "kve_size", (int64_t)sizeof(struct kinfo_vmentry));
    livefacts::kv_int(b, "entries", entries);
    livefacts::kv_int(b, "remainder", remainder);
    livefacts::kv_int(b, "attempts", (int64_t)attempts);
    emit(b);
}

// ---------------------------------------------------------------------------
// E6: self retention via dlopen(own E2 path, RTLD_NOLOAD|RTLD_NOW) and
// domain-equality comparison with the E3 raw handle (brief: domain equality
// for SELF). dlclose releases the reference exactly once when retained.
void e6_self_retention() {
    int64_t rc = 0;
    int mode = 0;
#ifdef RTLD_NOLOAD
    mode = RTLD_NOLOAD | RTLD_NOW;
    if (!g_exe_path_ok) {
        rc = -9;  // E2 did not produce a path; skipped honestly
    }
#else
    rc = -8;  // SDK dlfcn has no RTLD_NOLOAD; recorded absent, skipped
#endif
    void *h = nullptr;
    int64_t eno = 0;
    int64_t dlc = 0;
    if (rc == 0) {
        errno = 0;
        h = dlopen(g_exe_path, mode);
        eno = errno;
        if (h != nullptr) {
            errno = 0;
            dlc = dlclose(h);
        }
    }
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e6_self_retention", rc, eno,
                            LIVEFACTS_BUILD_ID);
#ifdef RTLD_NOLOAD
    livefacts::kv_int(b, "noload_defined", 1);
    kv_hex_str(b, "mode_hex", (uint64_t)(unsigned)mode);
#else
    livefacts::kv_int(b, "noload_defined", 0);
#endif
    livefacts::kv_int(b, "handle_nonnull", h != nullptr ? 1 : 0);
    kv_hex_str(b, "handle", (uint64_t)(uintptr_t)h);
    livefacts::kv_int(b, "dlclose_rc", dlc);
    livefacts::kv_int(b, "exec_handle_attempted", g_exec_handle_attempted);
    kv_hex_str(b, "exec_handle_raw_hex", (uint64_t)g_exec_handle_raw);
    livefacts::kv_int(
        b, "self_domain_equal",
        g_exec_handle_attempted
            ? ((uint64_t)(uintptr_t)h == (uint64_t)g_exec_handle_raw ? 1 : 0)
            : -1);
    emit(b);
}

// ---------------------------------------------------------------------------
// E7: firmware version via pinned ps5/kernel.h; kernel_get_fw_version is
// provided by the SDK's own crt1.o (static link, no dynamic import).
void e7_fw_version() {
    errno = 0;
    const uint32_t fw = kernel_get_fw_version();
    const int64_t eno = errno;
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "e7_fw_version", 0, eno,
                            LIVEFACTS_BUILD_ID);
    kv_hex_str(b, "fw_raw_hex", (uint64_t)fw);
    livefacts::kv_int(b, "fw_raw_dec", (int64_t)fw);
    livefacts::kv_str(b, "header", "target/include/ps5/kernel.h", 40);
    emit(b);
}

void summary_record() {
    livefacts::Buf b;
    livefacts::record_begin(b, ++g_seq, "summary", 0, 0, LIVEFACTS_BUILD_ID);
    livefacts::kv_str(b, "status", "COMPLETED", 16);
    livefacts::kv_int(b, "records_written", (int64_t)g_seq);
    livefacts::kv_int(b, "write_failures", g_sink.failed);
    livefacts::kv_str(b, "log_path", LIVEFACTS_LOG_PATH, 64);
    emit(b);
}

}  // namespace

int main(void) {
    if (livefacts::log_open(g_sink, LIVEFACTS_LOG_PATH) != 0) {
        return 90;  // log open failed: write nothing, record in exit code
    }
    e1_dtdebug_map();
    e2_proc_pathname();
    e3_exec_handle();
    e4_module_list_unit();
    e5_proc_vmmap();
    e6_self_retention();
    e7_fw_version();
    summary_record();
    if (livefacts::log_close(g_sink) != 0) {
        g_sink.failed = 1;
    }
    return g_sink.failed ? 92 : 0;
}
