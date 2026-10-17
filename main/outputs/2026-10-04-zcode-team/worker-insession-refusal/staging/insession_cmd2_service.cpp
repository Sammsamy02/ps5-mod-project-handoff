// insession_cmd2_service.cpp — the refusal-surviving in-session command service.
// worker-insession-refusal, 2026-10-05. STAGED UNIT — composed by a later
// integration step; NEVER dispatched, built for console, or run on console by
// this worker. Host tests drive the same code through POSIX shims (fixtures/).
//
// See insession_cmd2_service.h for the thread law, the safety case and the
// import law. Summary: ONE dedicated lightweight thread (std::thread, the
// same primitive RuntimeOwner and the UE4SS event loop already use in this
// PRX), spawned by module_start BEFORE any seeds work, joined by module_stop.
// It survives a recoverable initialization failure BY CONSTRUCTION: it owns
// no engine state, never touches UE4SS objects, and sleeps on its own clock
// (10 x 10 ms slices -> 100 ms cadence; stop joins in <= ~10 ms + one bounded
// command effect). It is independent of Lua, the game-thread pump, controller
// input and the 120-second seed budget.
//
// v2 EFFECTS (complete list — nothing else exists in v2; grammar v2 is a pure
// superset of worker-insession-diag grammar v1):
//   noop       : ONE UE4SS.log ack line.
//   diag       : ONE diagnostic_event("insession_<tag>", repeat) — the sink
//                is already open, append-only, same PRX; zero new sinks —
//                plus ONE UE4SS.log line.
//   io_probe   : ONE call of the registered batched IO control slot (a..d,
//                worker-io-controls); "controls not wired" reply otherwise.
//   pool_probe : ONE fresh bridge_kernel_write_copyin_probe(registered pool
//                address, registered length) — fresh EVERY time; the
//                once-per-boot gate (gap_probed_this_boot) lives in the
//                INITIALIZER TU only, never in the bridge. Requires a
//                registered target; replies "target not registered" else.
//   log        : ONE sanitized printable-ASCII line appended to UE4SS.log.
//   reject     : ONE loud UE4SS.log line; consumed, never re-read, NOTHING
//                executed.
//
// LOG LAW: every line is a DIRECT bounded append to the UE4SS.log path
// (runtime_module_entry MODBRIDGE_UE4SS_LOG_PATH convention:
// /data/plugins/UE4SS.log) through the proven kernel trio
// sceKernelOpen(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600) /
// sceKernelWrite (bounded partial-write retry) / sceKernelClose, with the
// runtime_module_entry r7 freshness prefix "[t=<monotonic ms>ms pid=<pid>]".
// Deliberately NOT Output::send: after a refusal the UE4SS output devices
// may be torn down (shutdown_native_program closes them), and this service
// must keep working exactly then. Multiple appenders to this file are the
// established pattern (the entry's own failure lines and the runtime's
// device lines already coexist).
//
// SINGLE-WRITER DIAGNOSTIC LAW: dispatch is HELD until settle_startup() —
// i.e. until module_start is about to return — so a dispatching command can
// never race the wiring thread's startup diagnostic_event burst (the
// lock-free sink's startup discipline, diagnostic.c:269-271). After settle
// on a refused boot the initializer is dead and this thread is the ONLY
// diagnostic_event caller (the v1 event-loop hook must NOT be applied
// alongside this service — ADMISSION-NOTE.md).

#include "insession_cmd2_core.h"
#include "insession_cmd2_service.h"

#include <sys/fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <thread>

#ifdef INSESSION_CMD2_HOST_TEST
// Host fixtures: the rename hook exists only so the at-least-once consume
// path can be injected with a deterministic failure; the console path is the
// plain contract rename. read and rename are native on the host.
extern "C" int insession2_test_rename(const char *oldp, const char *newp);
#define INSESSION2_RENAME(a, b) insession2_test_rename(a, b)
#else
#define INSESSION2_RENAME(a, b) rename(a, b)
#endif

// Frozen-import family — every symbol below is a strong UND member of the
// composed candidate's 196-import contract (provider identity verified in
// l10-1/l10-2/l11-2 supplier-contract.json; see ADMISSION-NOTE.md).
// diagnostic_event is the in-link C symbol (diagnostic.c, same PRX);
// bridge_kernel_write_copyin_probe is the bridge TU's in-link definition
// (same PRX, no import) — a missing definition is a LOUD link stop, never a
// silent fallback.
extern "C" {
int sceKernelOpen(const char *path, int flags, uint16_t mode);
int64_t sceKernelWrite(int fd, const void *buf, size_t n);
int sceKernelClose(int fd);
ssize_t read(int fd, void *buf, size_t n);       // libkernel/libScePosix
int rename(const char *oldp, const char *newp);  // libkernel/libScePosix
void diagnostic_event(const char *tag, uint32_t value);
int bridge_kernel_write_copyin_probe(uint64_t addr, uint64_t len);
}

#ifndef INSESSION2_CONSUMED_PATH
#define INSESSION2_CONSUMED_PATH INSESSION2_INTAKE_PATH ".consumed"
#endif
#ifndef INSESSION2_SLICE_MS
#define INSESSION2_SLICE_MS 10u
#endif
#ifndef INSESSION2_SLICES
#define INSESSION2_SLICES 10u /* 10 x 10 ms = the 100 ms poll cadence */
#endif
#define INSESSION2_READ_ATTEMPTS 8u /* bounded short-read loop (v1 law) */
#define INSESSION2_LINE_MAX 256u    /* entry-line family cap */
#define INSESSION2_FULL_MAX 320u    /* prefix + body */
#define INSESSION2_IO_TAGS_CAP 96u  /* bounded control result-tag buffer */

// ---- state (atomics; written by module_start/module_stop context, read by
// ---- the poll thread; thread creation/join provides the happens-before) --
namespace
{

std::atomic<unsigned> s_stop{0};
std::atomic<unsigned> s_started{0};
std::atomic<unsigned> s_have_thread{0};
std::atomic<unsigned> s_settled{0};
std::atomic<unsigned> s_refused{0};
std::atomic<uint64_t> s_pool_addr{0};
std::atomic<uint32_t> s_pool_len{0};
std::atomic<unsigned> s_pool_registered{0};
// Function pointers stored bit-cast: keeps registration atomic and legal at
// any time without dataloader-ABI questions.
std::atomic<uint64_t> s_io_controls[INSESSION2_IO_SLOTS] = {{0}, {0}, {0}, {0}};

// Uninitialized thread storage: constructed only inside service_start's
// try-block (a spawn failure never runs a constructor cascade at static-init
// time), destroyed conceptually by the join in service_stop. The process
// never destroys it dynamically; module_stop joins first (CALL-ORDER LAW).
alignas(std::thread) static unsigned char s_thread_storage[sizeof(std::thread)];
std::thread &thread_ref(void)
{
    return *reinterpret_cast<std::thread *>(&s_thread_storage[0]);
}

struct insession_file_body
{
    char bytes[INSESSION2_CMD_MAX]; /* fixed capacity; no allocation */
    size_t used;
    int truncated; /* 1 = file reached the buffer cap: reject without parsing */
};

/* Attempt ONE bounded read of the intake file.
   Returns 1 when the file was present (content in body), 0 when absent or
   unreadable — the idle poll cost is exactly one failed open. */
int insession2_read_intake(struct insession_file_body *body)
{
    int fd = sceKernelOpen(INSESSION2_INTAKE_PATH, O_RDONLY | O_NOFOLLOW, 0);
    body->used = 0;
    body->truncated = 0;
    if (fd < 0)
        return 0;
    for (unsigned attempt = 0; attempt < INSESSION2_READ_ATTEMPTS; ++attempt)
    {
        ssize_t got = read(fd, body->bytes + body->used,
                           sizeof(body->bytes) - body->used);
        if (got <= 0)
            break; /* EOF or read error: parse what we have (bounded) */
        body->used += (size_t)got;
        if (body->used >= sizeof(body->bytes))
        {
            body->truncated = 1;
            break; /* oversize: never buffer or read past the cap */
        }
    }
    sceKernelClose(fd);
    return 1;
}

/* Bounded literal-append helpers (diagnostic.c discipline). */
void insession2_append(char *buf, size_t *used, size_t cap, const char *text)
{
    while (*text && *used < cap)
        buf[(*used)++] = *text++;
}

void insession2_append_u32(char *buf, size_t *used, size_t cap, uint32_t v)
{
    char digits[10];
    unsigned n = 0;
    if (v == 0)
    {
        insession2_append(buf, used, cap, "0");
        return;
    }
    while (v > 0 && n < sizeof(digits))
    {
        digits[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    /* Direct char copies — NEVER insession2_append(&digits[--n]): that
       helper expects a NUL terminator and a decimal buffer has none (the
       host test caught exactly that stack-byte leak). */
    while (n > 0 && *used < cap)
        buf[(*used)++] = digits[--n];
}

void insession2_append_hex64(char *buf, size_t *used, size_t cap, uint64_t v)
{
    static const char digits[] = "0123456789abcdef";
    insession2_append(buf, used, cap, "0x");
    for (int shift = 60; shift >= 0 && *used < cap; shift -= 4)
        buf[(*used)++] = digits[(v >> shift) & 15];
}

/* Printable-ASCII sanitize (runtime_module_entry log_append_sanitized law):
   control/non-ASCII bytes become '?'. Defense in depth — the parser already
   restricts every command-supplied byte. */
void insession2_append_sanitized(char *buf, size_t *used, size_t cap,
                                 const char *text)
{
    if (text == 0)
        return;
    while (*text && *used < cap)
    {
        unsigned char c = (unsigned char)*text++;
        buf[(*used)++] = (char)((c >= 0x20 && c <= 0x7e) ? c : '?');
    }
}

/* ONE bounded append to UE4SS.log with the entry freshness prefix. Every
   failure tolerated, never fatal. */
void insession2_log_line(const char *body)
{
    char full[INSESSION2_FULL_MAX];
    size_t used = 0;
    uint64_t now_ms = 0;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        now_ms = (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
    insession2_append(full, &used, sizeof(full) - 1, "[t=");
    insession2_append_u32(full, &used, sizeof(full) - 1,
                          (uint32_t)(now_ms & 0xffffffffu));
    insession2_append(full, &used, sizeof(full) - 1, "ms pid=");
    insession2_append_u32(full, &used, sizeof(full) - 1,
                          (uint32_t)((uint64_t)getpid() & 0xffffffffu));
    insession2_append(full, &used, sizeof(full) - 1, "] ");
    insession2_append_sanitized(full, &used, sizeof(full) - 1, body);
    if (used < sizeof(full) - 1)
        full[used++] = '\n';
    int fd = sceKernelOpen(INSESSION2_UE4SS_LOG_PATH,
                           O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0)
        return; /* tolerated: the diagnostic channel still carries events */
    size_t offset = 0;
    for (unsigned attempts = 0; offset < used && attempts < 8u; ++attempts)
    {
        int64_t written = sceKernelWrite(fd, full + offset, used - offset);
        if (written <= 0 || (uint64_t)written > used - offset)
            break;
        offset += (size_t)written;
    }
    (void)sceKernelClose(fd);
}

/* Consume the intake file. A failure here is LOUD and leaves the file in
   place: the next poll may re-execute the same command (at-least-once, the
   v1 law). */
void insession2_consume(void)
{
    if (INSESSION2_RENAME(INSESSION2_INTAKE_PATH, INSESSION2_CONSUMED_PATH) == 0)
        return;
    insession2_log_line("INSESSION-CMD CONSUME-RENAME FAILED; command may "
                        "repeat until host reconciles the intake");
}

// ---- effects (each: effect, then ONE log line) ----------------------------

void insession2_effect_diag(const struct insession2_cmd *cmd)
{
    char tag[INSESSION2_TAG_MAX + 12]; /* "insession_" + tag + NUL */
    char line[INSESSION2_LINE_MAX];
    size_t used = 0;
    insession2_append(tag, &used, sizeof(tag) - 1, "insession_");
    insession2_append(tag, &used, sizeof(tag) - 1, cmd->tag);
    tag[used] = '\0';
    diagnostic_event(tag, cmd->repeat);

    used = 0;
    insession2_append(line, &used, sizeof(line) - 1,
                      "insession-cmd: diag tag=");
    insession2_append(line, &used, sizeof(line) - 1, cmd->tag);
    insession2_append(line, &used, sizeof(line) - 1, " repeat=");
    insession2_append_u32(line, &used, sizeof(line) - 1, cmd->repeat);
    insession2_append(line, &used, sizeof(line) - 1, " emitted (consumed)");
    line[used] = '\0';
    insession2_log_line(line);
}

void insession2_effect_io_probe(const struct insession2_cmd *cmd)
{
    const unsigned slot = (unsigned)cmd->io_slot; /* parser guarantees 0..3 */
    const uint64_t raw = s_io_controls[slot].load(std::memory_order_acquire);
    char line[INSESSION2_LINE_MAX];
    size_t used = 0;
    insession2_append(line, &used, sizeof(line) - 1,
                      "insession-cmd: io_probe op=");
    {
        const char opname[2] = {(char)('a' + slot), '\0'};
        insession2_append(line, &used, sizeof(line) - 1, opname);
    }
    if (raw == 0)
    {
        insession2_append(line, &used, sizeof(line) - 1,
                          ": controls not wired (consumed)");
        line[used] = '\0';
        insession2_log_line(line);
        return;
    }
    char tags[INSESSION2_IO_TAGS_CAP];
    tags[0] = '\0';
    int rc = -1;
    {
        /* Only the registered control runs; never a guessed address. */
        insession2_io_control_fn fn =
            reinterpret_cast<insession2_io_control_fn>(raw);
        rc = fn(tags, (unsigned)sizeof(tags));
    }
    insession2_append(line, &used, sizeof(line) - 1, " rc=");
    insession2_append_u32(line, &used, sizeof(line) - 1, (uint32_t)rc);
    insession2_append(line, &used, sizeof(line) - 1, " tags=");
    insession2_append_sanitized(line, &used, sizeof(line) - 1,
                                tags[0] != '\0' ? tags : "-");
    insession2_append(line, &used, sizeof(line) - 1, " (consumed)");
    line[used] = '\0';
    insession2_log_line(line);
}

void insession2_effect_pool_probe(void)
{
    char line[INSESSION2_LINE_MAX];
    size_t used = 0;
    insession2_append(line, &used, sizeof(line) - 1,
                      "insession-cmd: pool_probe");
    if (!s_pool_registered.load(std::memory_order_acquire))
    {
        insession2_append(line, &used, sizeof(line) - 1,
                          ": target not registered (consumed)");
        line[used] = '\0';
        insession2_log_line(line);
        return;
    }
    /* FRESH attempt every time: the once-per-boot gap_probed_this_boot gate
       lives in the initializer TU only (UnrealInitializer.cpp), never in the
       bridge. The bridge emits its own pool_copyin_probe_fresh/rc/errno/sink
       diagnostics; the class codes: 1 readable / 0 refused (EFAULT class,
       never "unmapped") / -1 unavailable-or-invalid request. */
    const uint64_t addr = s_pool_addr.load(std::memory_order_acquire);
    const uint32_t len = s_pool_len.load(std::memory_order_acquire);
    const int rc = bridge_kernel_write_copyin_probe(addr, (uint64_t)len);
    insession2_append(line, &used, sizeof(line) - 1, " addr=");
    insession2_append_hex64(line, &used, sizeof(line) - 1, addr);
    insession2_append(line, &used, sizeof(line) - 1, " len=0x");
    {
        /* 8 fixed hex digits of the 32-bit length (a bounded direct copy —
           never a non-NUL-terminated scratch buffer). */
        static const char hexd[] = "0123456789abcdef";
        int shift;
        for (shift = 28; shift >= 0 && used < sizeof(line) - 1; shift -= 4)
            line[used++] = hexd[(len >> shift) & 15];
    }
    insession2_append(line, &used, sizeof(line) - 1, " class=");
    insession2_append_u32(line, &used, sizeof(line) - 1, (uint32_t)rc);
    insession2_append(line, &used, sizeof(line) - 1, " fresh (consumed)");
    line[used] = '\0';
    insession2_log_line(line);
}

void insession2_effect_log(const struct insession2_cmd *cmd)
{
    char line[INSESSION2_LINE_MAX];
    size_t used = 0;
    insession2_append(line, &used, sizeof(line) - 1, "insession-cmd log: ");
    insession2_append_sanitized(line, &used, sizeof(line) - 1, cmd->text);
    line[used] = '\0';
    insession2_log_line(line);
}

} // namespace

void insession_cmd2_poll_once(void) noexcept
{
    struct insession_file_body body;
    if (!insession2_read_intake(&body))
        return; /* no command pending */

    if (body.truncated)
    {
        insession2_log_line("INSESSION-CMD REJECTED oversize (>=512B); "
                            "consumed, not executed");
        insession2_consume();
        return;
    }

    struct insession2_cmd cmd;
    insession2_parse(body.bytes, body.used, &cmd);

    if (cmd.verdict == INSESSION2_REJECT)
    {
        char line[INSESSION2_LINE_MAX];
        size_t used = 0;
        insession2_append(line, &used, sizeof(line) - 1,
                          "INSESSION-CMD REJECTED (");
        insession2_append(line, &used, sizeof(line) - 1,
                          cmd.reason ? cmd.reason : "malformed");
        insession2_append(line, &used, sizeof(line) - 1,
                          "); consumed, not executed");
        line[used] = '\0';
        insession2_log_line(line);
        insession2_consume();
        return;
    }

    if (cmd.verdict == INSESSION2_NOOP)
    {
        insession2_log_line("insession-cmd: noop ack (consumed)");
        insession2_consume();
        return;
    }
    if (cmd.verdict == INSESSION2_DIAG)
    {
        insession2_effect_diag(&cmd);
        insession2_consume();
        return;
    }
    if (cmd.verdict == INSESSION2_IO_PROBE)
    {
        insession2_effect_io_probe(&cmd);
        insession2_consume();
        return;
    }
    if (cmd.verdict == INSESSION2_POOL_PROBE)
    {
        insession2_effect_pool_probe();
        insession2_consume();
        return;
    }
    /* INSESSION2_LOG */
    insession2_effect_log(&cmd);
    insession2_consume();
}

namespace
{

void insession2_poll_loop(void) noexcept
{
    /* Never fatal: nothing may escape a thread proc. All the effect code
       above is bounded and non-throwing; the try/catch is the final net. */
    try
    {
        while (s_stop.load(std::memory_order_relaxed) == 0)
        {
            /* 10 x 10 ms slices: the 100 ms cadence with a <= 10 ms stop
               latency. */
            for (unsigned slice = 0;
                 slice < (unsigned)INSESSION2_SLICES &&
                 s_stop.load(std::memory_order_relaxed) == 0;
                 ++slice)
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds((int)INSESSION2_SLICE_MS));
            }
            if (s_stop.load(std::memory_order_relaxed) != 0)
                break;
            if (s_settled.load(std::memory_order_acquire) == 0)
                continue; /* wiring window: idle open-tries ONLY */
            insession_cmd2_poll_once();
        }
    }
    catch (...)
    {
        /* tolerated: the service goes quiet, the process is untouched */
    }
}

} // namespace

void insession_cmd2_service_start(void) noexcept
{
    unsigned expected = 0;
    if (!s_started.compare_exchange_strong(expected, 1u,
                                           std::memory_order_relaxed))
        return; /* idempotent: at most one service per process */
    try
    {
        new (&s_thread_storage[0]) std::thread(insession2_poll_loop);
        s_have_thread.store(1u, std::memory_order_release);
    }
    catch (...)
    {
        s_started.store(2u, std::memory_order_relaxed); /* service absent */
    }
}

void insession_cmd2_service_stop(void) noexcept
{
    s_stop.store(1u, std::memory_order_relaxed);
    if (s_have_thread.load(std::memory_order_acquire) != 0 &&
        thread_ref().get_id() != std::this_thread::get_id())
    {
        try
        {
            thread_ref().join();
        }
        catch (...)
        {
        }
        s_have_thread.store(0u, std::memory_order_release);
    }
}

void insession_cmd2_settle_startup(void) noexcept
{
    s_settled.store(1u, std::memory_order_release);
}

void insession_cmd2_note_initializer_refused(void) noexcept
{
    unsigned expected = 0;
    if (!s_refused.compare_exchange_strong(expected, 1u,
                                           std::memory_order_relaxed))
        return; /* already recorded: exactly one triage line per boot */
    insession2_log_line("insession-cmd: initializer refused (recoverable); "
                        "in-session service remains callable");
}

void insession_cmd2_register_pool_probe_target(uint64_t pool_addr,
                                               uint32_t probe_len) noexcept
{
    s_pool_addr.store(pool_addr, std::memory_order_relaxed);
    s_pool_len.store(probe_len, std::memory_order_relaxed);
    s_pool_registered.store(1u, std::memory_order_release);
}

void insession_cmd2_register_io_control(unsigned slot,
                                        insession2_io_control_fn fn) noexcept
{
    if (slot >= (unsigned)INSESSION2_IO_SLOTS)
        return;
    s_io_controls[slot].store(reinterpret_cast<uint64_t>(fn),
                              std::memory_order_release);
}
