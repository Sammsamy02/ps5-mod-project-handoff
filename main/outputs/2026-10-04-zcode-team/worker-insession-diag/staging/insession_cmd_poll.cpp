// insession_cmd_poll.cpp — bounded in-session command consumer (v1: diag/noop).
// worker-insession-diag, 2026-10-05. STAGED UNIT — composed by a later
// integration step; NEVER dispatched, built for console, or run on console by
// this worker. Host tests drive the same code through POSIX shims (fixtures/).
//
// Shape (per worker-reload-support/SURVEY.md §3(a)): the proven e2e intake
// pattern (STOR-once -> consumed-rename -> evidence, console_batch.py
// cmd_put/cmd_gone_ok) consumed by a bounded poll on the EXISTING event loop.
//   intake   /data/modbridge/intake/in-session-cmd.json          (fixed path)
//   consumed /data/modbridge/intake/in-session-cmd.json.consumed (rename)
//
// v1 EFFECTS (complete list — nothing else exists in v1):
//   diag : ONE diagnostic_event("insession_<tag>", repeat) call — the sink is
//          already open, append-only, same-PRX; zero new sinks/imports
//          (precedent runtime_start_bridge_r14_diag_spanpub.cpp:119-121,
//          geometry-diag merge :47-52) — plus ONE UE4SS.log line.
//   noop : ONE UE4SS.log ack line only.
//   reject (malformed/unknown/oversize): ONE loud UE4SS.log line, consumed,
//          never re-read. An unknown cmd value (e.g. "reload") is REJECTED,
//          never executed — the reload half stays proposal (SURVEY §3(b)).
//   NO command in v1 writes engine state of any kind.
//
// THREAD LAW: runs ONLY on the event-loop thread (see insession_cmd_poll.h).
// That is the same thread the PROSPERO event-batch executor (:1677-1707) and
// mod fire_update (:1734-1744) run on, under the loop's own serialization;
// in-session it is also the ONLY diagnostic_event caller after module_start
// (all startup emit sites precede the loop), preserving diagnostic.c's
// single-writer assumption without adding a lock.
//
// IMPORT LAW (zero new imports): the file ops below use ONLY symbols already
// in the composed candidate's 196-strong import contract
// (supplier-contract.json, required_strong=196/missing_strong=0, verified
// identical across composition2-1, l10-1, l10-2, l11-2, comp6-4):
//   sceKernelOpen/sceKernelClose — libkernel/libkernel (diagnostic.c's proven
//     live trio; diagnostic_ready() gates the same bindings today)
//   read, rename                 — libkernel/libScePosix wrappers
//   (strcmp/strlen — used by the parser core — are contract members too)
// sceKernelRead/sceKernelRename are NOT in the contract and are NOT used.
//
// FAILURE LAW (diagnostic.c discipline): every failure tolerated, never
// fatal, no exception can escape (noexcept). Consumption is at-least-once:
// execution happens BEFORE the rename, so a failed rename is logged LOUDLY
// and the same command may execute again on a later poll (duplicate
// diagnostics are observable and harmless; a silent loss would not be).
// The intake holds ONE command; the host stages the next only after the
// .consumed evidence appears (the e2e cmd_put stale-guard discipline).

#include "insession_cmd_core.h"
#include "insession_cmd_poll.h"

#include <sys/fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#ifdef INSESSION_CMD_HOST_TEST
// Host fixtures: the emitter and rename are provided by the fixture (the
// rename hook exists only so the at-least-once path can be injected with a
// deterministic failure; the console path is the plain contract rename).
extern "C" void insession_test_log(const char *line);
extern "C" int insession_test_rename(const char *oldp, const char *newp);
#define INSESSION_LOG(line) insession_test_log(line)
#define INSESSION_RENAME(a, b) insession_test_rename(a, b)
#else
#include <DynamicOutput/Output.hpp>
#include <UE4SSProgram.hpp> // StringType — same include ue4ss_backend.cpp uses
#define INSESSION_LOG(line) insession_log_line(line)
#define INSESSION_RENAME(a, b) rename(a, b)
#endif

// Frozen-import family — every symbol already a strong UND member of the
// 196-contract (identity rows quoted in REPORT.md). Declared explicitly, the
// way diagnostic.c declares its three kernel bindings.
extern "C" {
int sceKernelOpen(const char *path, int flags, uint16_t mode);
int sceKernelClose(int fd);
ssize_t read(int fd, void *buf, size_t n);      // libkernel/libScePosix
int rename(const char *oldp, const char *newp); // libkernel/libScePosix
void diagnostic_event(const char *tag, uint32_t value); // diagnostic.c, same PRX
}

#ifndef INSESSION_INTAKE_PATH
#define INSESSION_INTAKE_PATH "/data/modbridge/intake/in-session-cmd.json"
#endif
#ifndef INSESSION_CONSUMED_PATH
#define INSESSION_CONSUMED_PATH INSESSION_INTAKE_PATH ".consumed"
#endif
/* Loop ticks between polls. The loop floor is the 5ms sleep (wpd1:1754), so
   100 ticks is ~0.5-1.5s cadence depending on frame work. Console builds
   never override it; host fixtures may. */
#ifndef INSESSION_CMD_POLL_EVERY
#define INSESSION_CMD_POLL_EVERY 100u
#endif
#define INSESSION_READ_ATTEMPTS 8u /* bounded short-read loop */

#ifdef INSESSION_CMD_HOST_TEST
namespace
{
#else
// Narrow ASCII -> StringType byte-by-byte: the PS5 codecvt discipline (proven
// live, wpd1 UE4SSProgram.cpp:1807-1822) — never a wstring_convert route.
// Every byte emitted here is generated by this TU; the tag has already passed
// the pure-ASCII charset check in the parser core.
static void insession_log_line(const char *line)
{
    StringType wide;
    for (const char *p = line; *p; ++p)
        wide.push_back(static_cast<StringType::value_type>(
            static_cast<unsigned char>(*p)));
    Output::send(STR("{}\n"), wide);
}

namespace
{
#endif

struct insession_file_body
{
    char bytes[INSESSION_CMD_MAX]; /* fixed capacity; no allocation */
    size_t used;
    int truncated; /* 1 = file reached the buffer cap: reject without parsing */
};

/* Attempt ONE bounded read of the intake file.
   Returns 1 when the file was present (content in body), 0 when absent or
   unreadable — the idle poll cost is exactly one failed open. */
int insession_read_intake(struct insession_file_body *body)
{
    int fd = sceKernelOpen(INSESSION_INTAKE_PATH, O_RDONLY | O_NOFOLLOW, 0);
    body->used = 0;
    body->truncated = 0;
    if (fd < 0)
        return 0;
    for (unsigned attempt = 0; attempt < INSESSION_READ_ATTEMPTS; ++attempt)
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

/* Bounded literal-append helpers (diagnostic.c append discipline). */
void insession_append(char *buf, size_t *used, size_t cap, const char *text)
{
    while (*text && *used < cap)
        buf[(*used)++] = *text++;
}

void insession_append_u32(char *buf, size_t *used, size_t cap, uint32_t v)
{
    char digits[10];
    unsigned n = 0;
    if (v == 0)
    {
        insession_append(buf, used, cap, "0");
        return;
    }
    while (v > 0 && n < sizeof(digits))
    {
        digits[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0)
        insession_append(buf, used, cap, &digits[--n]);
}

/* Consume the intake file. A failure here is LOUD and leaves the file in
   place: the next poll may re-execute the same command (at-least-once, see
   FAILURE LAW above). */
void insession_consume(void)
{
    if (INSESSION_RENAME(INSESSION_INTAKE_PATH, INSESSION_CONSUMED_PATH) == 0)
        return;
    INSESSION_LOG("INSESSION-CMD CONSUME-RENAME FAILED; command may repeat "
                  "until host reconciles the intake");
}

} // namespace

extern "C" void insession_cmd_poll(void) noexcept
{
    static unsigned s_tick; /* loop-thread-only state; no atomics by law */
    ++s_tick;
    if ((s_tick % (unsigned)INSESSION_CMD_POLL_EVERY) != 0)
        return; /* idle tick: one increment, nothing else */

    struct insession_file_body body;
    if (!insession_read_intake(&body))
        return; /* no command pending */

    if (body.truncated)
    {
        INSESSION_LOG("INSESSION-CMD REJECTED oversize (>=512B); consumed, "
                      "not executed");
        insession_consume();
        return;
    }

    struct insession_cmd cmd;
    insession_parse_v1(body.bytes, body.used, &cmd);

    if (cmd.verdict == INSESSION_REJECT)
    {
        char line[128];
        size_t used = 0;
        insession_append(line, &used, sizeof(line) - 1,
                         "INSESSION-CMD REJECTED (");
        insession_append(line, &used, sizeof(line) - 1,
                         cmd.reason ? cmd.reason : "malformed");
        insession_append(line, &used, sizeof(line) - 1,
                         "); consumed, not executed");
        line[used] = '\0';
        INSESSION_LOG(line);
        insession_consume();
        return;
    }

    if (cmd.verdict == INSESSION_NOOP)
    {
        INSESSION_LOG("insession-cmd: noop ack (consumed)");
        insession_consume();
        return;
    }

    /* INSESSION_DIAG — the only effectful command in v1. */
    char tag[INSESSION_TAG_MAX + 12]; /* "insession_" + tag + NUL */
    size_t used = 0;
    insession_append(tag, &used, sizeof(tag) - 1, "insession_");
    insession_append(tag, &used, sizeof(tag) - 1, cmd.tag);
    tag[used] = '\0';
    diagnostic_event(tag, cmd.repeat);

    char line[128];
    used = 0;
    insession_append(line, &used, sizeof(line) - 1,
                     "insession-cmd: diag tag=");
    insession_append(line, &used, sizeof(line) - 1, cmd.tag);
    insession_append(line, &used, sizeof(line) - 1, " repeat=");
    insession_append_u32(line, &used, sizeof(line) - 1, cmd.repeat);
    insession_append(line, &used, sizeof(line) - 1, " emitted (consumed)");
    line[used] = '\0';
    INSESSION_LOG(line); /* the ONE UE4SS.log line */
    insession_consume();
}
