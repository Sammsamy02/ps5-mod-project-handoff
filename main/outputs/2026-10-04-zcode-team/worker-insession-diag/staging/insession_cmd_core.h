/* insession_cmd_core.h — grammar v1 for the in-session command intake.
   worker-insession-diag, 2026-10-05. STAGED UNIT — proposal/composition-later;
   nothing here has run on console.

   Grammar v1 (STRICT — an unrecognized byte shape must never silently no-op):
     accepted : {"cmd":"noop"}
                {"cmd":"diag","tag":"<1..48 chars of [A-Za-z0-9_.-]>",
                 "repeat":<1..1024>}
     rejected : everything else (malformed JSON, unknown cmd value, unknown
                key, duplicate key, escape sequences, control/non-ASCII bytes,
                out-of-range repeat, trailing bytes, oversize input). Rejected
                commands are consumed-and-logged-LOUDLY by the poll glue, never
                executed, never re-read.

   Key order is flexible (the scanner is a real object scanner, not a literal
   match); only the whitespace inside/around the single JSON object is
   tolerated. Escapes are NOT implemented in v1: a tag that would need an
   escape is a tag the host should not be sending — reject keeps the parser
   allocation-free and injection-proof.

   Pure function: no I/O, no allocation, no global state. Compiles as C in the
   PRX composition (diagnostic.c discipline) and unmodified in host fixtures. */

#ifndef INSESSION_CMD_CORE_H
#define INSESSION_CMD_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Input budget: one FTP STOR of the intake file larger than this is consumed
   as oversize without parsing (bounded read in the poll glue never buffers
   more). */
#define INSESSION_CMD_MAX 512u

/* tag: 1..48 chars of [A-Za-z0-9_.-]. diagnostic.c's supplier sink carries
   tags up to 96 chars, so "insession_" + 48 always fits both sinks. */
#define INSESSION_TAG_MAX 48u

/* repeat: 1..1024. Carried as the diagnostic_event value in ONE call — v1
   never emits the event `repeat` times (the sdiag jsonl is append-only and
   one bounded row per command keeps the sink predictable). */
#define INSESSION_REPEAT_MAX 1024u

enum insession_verdict
{
    INSESSION_DIAG = 0,   /* execute: diagnostic_event("insession_" + tag, repeat) */
    INSESSION_NOOP = 1,   /* ack only: one log line, no sink write */
    INSESSION_REJECT = 2  /* loud log + consumed; nothing executed */
};

struct insession_cmd
{
    int verdict;                      /* enum insession_verdict */
    char tag[INSESSION_TAG_MAX + 1];  /* NUL-terminated; set only for DIAG */
    uint32_t repeat;                  /* 1..INSESSION_REPEAT_MAX; DIAG only */
    const char *reason;               /* REJECT: static literal reason; else NULL */
};

/* Parse ONE in-session command. buf/n need no NUL terminator; the scanner
   never reads past n. Leading/trailing whitespace around the single object is
   tolerated; everything else follows the strict grammar above. */
void insession_parse_v1(const char *buf, size_t n, struct insession_cmd *out);

#ifdef __cplusplus
}
#endif

#endif /* INSESSION_CMD_CORE_H */
