// test_insess_integration.cpp — host test for the INTEGRATED in-session
// service (worker-insession-integrate, 2026-10-05).
//
// Proves the integration contract end-to-end on the host, against mocks:
//   - the REAL service TU (insession_cmd2_service.cpp) + REAL grammar core
//     (insession_cmd2_core.c), both from worker-insession-refusal/staging
//     (host 94/94 suite lineage), compiled with INSESSION_CMD2_HOST_TEST;
//   - the REAL bridge hook region, sed-extracted VERBATIM between the
//     INSESSION-INTEGRATION-HOST-TEST markers of the staged bridge
//     (runtime_start_bridge_r14_diag_spanpub_insession.cpp) into hook.inc —
//     the same bytes the console TU ships (marker law, worker-io-controls
//     precedent). The mocks below provide exactly the three symbols the
//     region needs from the bridge TU: bridge_io_controls(), copy_detail()
//     and (indirectly, via the service) the frozen-import family.
//
// What is asserted (beyond the refusal worker's 94 checks — this is the
// INTEGRATION layer):
//   T0  pre-registration dispatch: io_probe -> "controls not wired",
//       pool_probe -> "target not registered"; bridge never called.
//   T1  bridge_insession_hook() registers the pool target
//       (0x400000+0x9365ec0, 0x20000) and the four io slots, then spawns.
//   T2  dispatch HELD before settle: staged command untouched for 450 ms.
//   T3  refusal flag + settle: refused triage line; then the held command
//       dispatches exactly once.
//   T4  io_probe op=c -> exactly ONE bridge_io_controls() call, reply
//       "io_probe op=c ... tags=batch-A-D".
//   T5  pool_probe -> exactly ONE copyin-probe call with the registered
//       (addr,len) verbatim; reply carries addr/len/class/fresh.
//   T6  diag/log/reject superset sanity on the integrated path.
//   T7  second hook call (entry-hook + bridge-hook double-start shape) is
//       idempotent: one thread, single dispatch, registrations re-stored.
//   T8  service_stop joins fast; the dead thread consumes nothing.
//
// Mocked kernel/posix surface (the service's full effect list): the
// sceKernel trio + read + the rename hook forward to the REAL host FS under
// a mkdtemp root (path translation of the /data prefix), diagnostic_event
// and bridge_kernel_write_copyin_probe are recorded, never executed.

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>

#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Mock surface (definitions BEFORE the hook region include)
// ---------------------------------------------------------------------------

static std::string g_tmp; // mkdtemp root; /data/... maps under it

static std::string translate(const char *p)
{
    std::string s(p);
    if (s.rfind("/data/", 0) == 0)
        return g_tmp + s;
    return s;
}

// --- kernel trio (frozen-import family) ---
static std::atomic<long> g_open_calls{0};
static std::atomic<long> g_write_calls{0};
static std::atomic<long> g_close_calls{0};

extern "C" int sceKernelOpen(const char *path, int flags, uint16_t mode)
{
    ++g_open_calls;
    return open(translate(path).c_str(), flags, (mode_t)mode);
}

extern "C" int64_t sceKernelWrite(int fd, const void *buf, size_t n)
{
    ++g_write_calls;
    return (int64_t)write(fd, buf, n);
}

extern "C" int sceKernelClose(int fd)
{
    ++g_close_calls;
    return close(fd);
}

// --- diagnostic channel recorder (diagnostic.c stand-in) ---
struct DiagRec
{
    char tag[64];
    uint32_t value;
};
static DiagRec g_diag[4096];
static std::atomic<size_t> g_diag_n{0};

extern "C" void diagnostic_event(const char *tag, uint32_t value)
{
    const size_t i = g_diag_n.fetch_add(1);
    if (i < sizeof(g_diag) / sizeof(g_diag[0]))
    {
        std::snprintf(g_diag[i].tag, sizeof(g_diag[i].tag), "%s", tag ? tag : "");
        g_diag[i].value = value;
    }
}

static int diag_find(const char *tag, uint32_t value)
{
    const size_t n = g_diag_n.load();
    const size_t cap = sizeof(g_diag) / sizeof(g_diag[0]);
    const size_t bound = n < cap ? n : cap;
    for (size_t i = 0; i < bound; ++i)
        if (std::strcmp(g_diag[i].tag, tag) == 0 && g_diag[i].value == value)
            return 1;
    return 0;
}

static long diag_count(const char *tag)
{
    long hits = 0;
    const size_t n = g_diag_n.load();
    const size_t cap = sizeof(g_diag) / sizeof(g_diag[0]);
    const size_t bound = n < cap ? n : cap;
    for (size_t i = 0; i < bound; ++i)
        if (std::strcmp(g_diag[i].tag, tag) == 0)
            ++hits;
    return hits;
}

// --- bridge copyin probe recorder (bridge_kernel_write_copyin_probe) ---
static std::atomic<long> g_probe_calls{0};
static std::atomic<uint64_t> g_probe_addr{0};
static std::atomic<uint64_t> g_probe_len{0};

extern "C" int bridge_kernel_write_copyin_probe(uint64_t addr, uint64_t len)
{
    ++g_probe_calls;
    g_probe_addr.store(addr);
    g_probe_len.store(len);
    return 1; // readable class
}

// --- batched io controls recorder (the frozen bridge region stand-in) ---
static std::atomic<long> g_ioctrl_calls{0};

extern "C" void bridge_io_controls(void)
{
    ++g_ioctrl_calls;
}

// --- bounded printable-ASCII detail copy (bridge copy_detail semantics) ---
static void copy_detail(char *detail, unsigned detail_capacity, const char *text)
{
    if (detail == nullptr || detail_capacity == 0u)
        return;
    unsigned used = 0u;
    if (text != nullptr)
    {
        while (*text != '\0' && used + 1u < detail_capacity)
        {
            const char c = *text++;
            detail[used++] = (c >= 0x20 && c <= 0x7e) ? c : '?';
        }
    }
    detail[used] = '\0';
}

// --- the service's host rename hook (consume path) ---
static std::atomic<long> g_rename_calls{0};

extern "C" int insession2_test_rename(const char *oldp, const char *newp)
{
    ++g_rename_calls;
    return rename(translate(oldp).c_str(), translate(newp).c_str());
}

// ---------------------------------------------------------------------------
// The VERBATIM integrated hook region (registrations + adapter + spawn)
// ---------------------------------------------------------------------------

#include "hook.inc" // sed-extracted INSESSION-INTEGRATION-HOST-TEST region

// ---------------------------------------------------------------------------
// Test plumbing
// ---------------------------------------------------------------------------

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond, msg)                                                       \
    do                                                                         \
    {                                                                          \
        ++g_checks;                                                            \
        if (cond)                                                              \
        {                                                                      \
            std::printf("PASS %s\n", msg);                                     \
        }                                                                      \
        else                                                                   \
        {                                                                      \
            ++g_fails;                                                         \
            std::printf("FAIL %s\n", msg);                                     \
        }                                                                      \
    } while (0)

static const char *INTAKE = "/data/modbridge/intake/in-session-cmd.json";
static const char *CONSUMED = "/data/modbridge/intake/in-session-cmd.json.consumed";
static const char *LOG = "/data/plugins/UE4SS.log";

static int stage_cmd(const char *json)
{
    const std::string p = translate(INTAKE);
    const int fd = open(p.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return 0;
    const size_t n = std::strlen(json);
    const ssize_t w = write(fd, json, n);
    close(fd);
    return w == (ssize_t)n;
}

static int file_exists(const char *p)
{
    struct stat st;
    return stat(translate(p).c_str(), &st) == 0;
}

static int wait_gone(int timeout_ms)
{
    for (int ms = 0; ms < timeout_ms; ms += 20)
    {
        if (!file_exists(INTAKE))
            return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return !file_exists(INTAKE);
}

static std::string read_file(const char *p)
{
    FILE *f = std::fopen(translate(p).c_str(), "rb");
    if (!f)
        return "";
    std::string out;
    char buf[1024];
    size_t got;
    while ((got = std::fread(buf, 1, sizeof buf, f)) > 0)
        out.append(buf, got);
    std::fclose(f);
    return out;
}

static long count_sub(const std::string &hay, const char *needle)
{
    long n = 0;
    size_t pos = 0;
    const size_t len = std::strlen(needle);
    while ((pos = hay.find(needle, pos)) != std::string::npos)
    {
        ++n;
        pos += len;
    }
    return n;
}

static long long now_ms(void)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int main(void)
{
    // mkdtemp root + the /data subtree the service touches.
    char tmpl[] = "/tmp/insess-integrate-XXXXXX";
    const char *d = mkdtemp(tmpl);
    if (!d)
    {
        std::printf("FAIL mkdtemp\n");
        return 2;
    }
    g_tmp = d;
    char cmd[512];
    std::snprintf(cmd, sizeof cmd, "mkdir -p '%s/data/modbridge/intake' '%s/data/plugins'",
                  g_tmp.c_str(), g_tmp.c_str());
    if (std::system(cmd) != 0)
    {
        std::printf("FAIL mkdir\n");
        return 2;
    }

    // ---- T0: pre-registration manual dispatch (no thread, nothing wired) --
    CHECK(stage_cmd("{\"cmd\":\"io_probe\",\"op\":\"a\"}"), "T0 stage io_probe");
    insession_cmd2_poll_once();
    CHECK(!file_exists(INTAKE) && file_exists(CONSUMED), "T0 io_probe consumed (rename)");
    CHECK(g_ioctrl_calls.load() == 0, "T0 bridge_io_controls never called");
    CHECK(count_sub(read_file(LOG), "controls not wired") == 1,
          "T0 reply: controls not wired");

    CHECK(stage_cmd("{\"cmd\":\"pool_probe\"}"), "T0 stage pool_probe");
    insession_cmd2_poll_once();
    CHECK(g_probe_calls.load() == 0, "T0 copyin probe never called");
    CHECK(count_sub(read_file(LOG), "target not registered") == 1,
          "T0 reply: target not registered");

    // ---- T1: the integrated hook (registrations + spawn) ------------------
    bridge_insession_hook();
    CHECK(g_ioctrl_calls.load() == 0, "T1 hook itself runs no controls");
    CHECK(g_probe_calls.load() == 0, "T1 hook itself runs no probe");

    // ---- T2: dispatch HELD before settle ----------------------------------
    remove(translate(CONSUMED).c_str()); // clear T0's consumed evidence
    CHECK(stage_cmd("{\"cmd\":\"noop\"}"), "T2 stage noop");
    std::this_thread::sleep_for(std::chrono::milliseconds(450));
    CHECK(file_exists(INTAKE), "T2 intake untouched before settle");
    CHECK(!file_exists(CONSUMED), "T2 nothing consumed before settle");
    CHECK(count_sub(read_file(LOG), "noop ack") == 0, "T2 no effect before settle");

    // ---- T3: refusal flag + settle ----------------------------------------
    insession_cmd2_note_initializer_refused();
    CHECK(count_sub(read_file(LOG), "initializer refused (recoverable)") == 1,
          "T3 refused triage line (exactly one)");
    insession_cmd2_settle_startup();
    CHECK(wait_gone(3000), "T3 held command dispatched after settle");
    CHECK(count_sub(read_file(LOG), "noop ack") == 1, "T3 exactly one noop ack");

    // ---- T4: io_probe through the wired adapter ---------------------------
    CHECK(stage_cmd("{\"cmd\":\"io_probe\",\"op\":\"c\"}"), "T4 stage io_probe c");
    CHECK(wait_gone(3000), "T4 io_probe consumed");
    CHECK(g_ioctrl_calls.load() == 1, "T4 bridge_io_controls called exactly once");
    {
        const std::string log = read_file(LOG);
        CHECK(count_sub(log, "io_probe op=c") == 1, "T4 reply names op=c");
        CHECK(count_sub(log, "tags=batch-A-D") == 1, "T4 reply carries batch tag");
        CHECK(count_sub(log, "rc=0") >= 1, "T4 reply carries dispatch-ack rc");
    }

    // ---- T5: pool_probe with the registered SeedTable target --------------
    CHECK(stage_cmd("{\"cmd\":\"pool_probe\"}"), "T5 stage pool_probe");
    CHECK(wait_gone(3000), "T5 pool_probe consumed");
    CHECK(g_probe_calls.load() == 1, "T5 copyin probe called exactly once");
    CHECK(g_probe_addr.load() == 0x400000ull + 0x9365ec0ull,
          "T5 probe address = kBaseObserved + kFNamePoolLabel");
    CHECK(g_probe_len.load() == 0x20000ull, "T5 probe length = 0x20000");
    {
        const std::string log = read_file(LOG);
        CHECK(count_sub(log, "addr=0x0000000009765ec0") == 1, "T5 reply addr");
        CHECK(count_sub(log, "len=0x00020000") == 1, "T5 reply len");
        CHECK(count_sub(log, "class=1 fresh (consumed)") == 1, "T5 reply class+fresh");
    }

    // ---- T6: grammar-v2 superset sanity on the integrated path ------------
    CHECK(stage_cmd("{\"cmd\":\"diag\",\"tag\":\"integrate.p1\",\"repeat\":7}"),
          "T6 stage diag");
    CHECK(wait_gone(3000), "T6 diag consumed");
    CHECK(diag_find("insession_integrate.p1", 7), "T6 diagnostic_event emitted");
    CHECK(count_sub(read_file(LOG), "diag tag=integrate.p1 repeat=7") == 1,
          "T6 diag reply line");

    CHECK(stage_cmd("{\"cmd\":\"log\",\"text\":\"integration marker line\"}"),
          "T6 stage log");
    CHECK(wait_gone(3000), "T6 log consumed");
    CHECK(count_sub(read_file(LOG), "insession-cmd log: integration marker line") == 1,
          "T6 log reply line");

    const long io_before = g_ioctrl_calls.load();
    const long probe_before = g_probe_calls.load();
    CHECK(stage_cmd("{\"cmd\":\"frobnicate\"}"), "T6 stage reject");
    CHECK(wait_gone(3000), "T6 reject consumed");
    CHECK(count_sub(read_file(LOG), "INSESSION-CMD REJECTED (unknown cmd)") == 1,
          "T6 loud reject line");
    CHECK(g_ioctrl_calls.load() == io_before && g_probe_calls.load() == probe_before,
          "T6 rejected command executed nothing");

    // ---- T7: idempotent re-hook (entry-hook + bridge-hook double start) ---
    // The real composition calls service_start TWICE (entry at module_start,
    // bridge in the scan wrapper); stop happens only at module_stop. Both
    // starts must yield ONE thread and ONE dispatch per command.
    const long io_before7 = g_ioctrl_calls.load();
    bridge_insession_hook();
    CHECK(g_ioctrl_calls.load() == io_before7, "T7 re-hook runs no controls");
    CHECK(stage_cmd("{\"cmd\":\"diag\",\"tag\":\"twice\",\"repeat\":1}"),
          "T7 stage diag after re-hook");
    CHECK(wait_gone(3000), "T7 command dispatches after re-hook");
    CHECK(diag_count("insession_twice") == 1, "T7 exactly one diagnostic_event");
    CHECK(count_sub(read_file(LOG), "diag tag=twice repeat=1") == 1,
          "T7 exactly one dispatch (single thread)");

    // ---- T8: stop bound + dead thread consumes nothing --------------------
    // (Terminal module_stop shape: after service_stop the composition never
    // restarts the service — service_start-after-stop is not a supported
    // call order, so the dead thread must simply consume nothing.)
    const long long t0 = now_ms();
    insession_cmd2_service_stop();
    const long long dt = now_ms() - t0;
    CHECK(dt < 500, "T8 stop join latency < 500 ms");
    CHECK(stage_cmd("{\"cmd\":\"noop\"}"), "T8 stage post-stop noop");
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    CHECK(file_exists(INTAKE), "T8 dead thread consumed nothing");

    std::printf("\n--- insession integration host test: %d checks, %d failures ---\n",
                g_checks, g_fails);
    return g_fails == 0 ? 0 : 1;
}
