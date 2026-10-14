/* insession_cmd2_core.h — grammar v2 for the in-session command intake.
   worker-insession-refusal, 2026-10-05. STAGED UNIT — proposal/composition-
   later; nothing here has run on console. Extends worker-insession-diag
   grammar v1 (pure-superset; v1 inputs parse identically) for the dedicated
   refusal-surviving service thread.

   Grammar v2 (STRICT — an unrecognized byte shape must never silently no-op):
     accepted : {"cmd":"noop"}
                {"cmd":"diag","tag":"<1..48 chars of [A-Za-z0-9_.-]>",
                 "repeat":<1..1024>}
                {"cmd":"io_probe","op":"a"|"b"|"c"|"d"}
                {"cmd":"pool_probe"}
                {"cmd":"log","text":"<1..96 printable ASCII 0x20..0x7e>"}
     rejected : everything else (malformed JSON, unknown cmd value, unknown
                key, duplicate key, escape sequences, control/non-ASCII bytes,
                out-of-range repeat, bad op, empty/oversize text, trailing
                bytes, oversize input). Rejected commands are consumed-and-
                logged-LOUDLY by the service, never executed, never re-read.

   Key order is flexible (the scanner is a real object scanner, not a literal
   match); only whitespace inside/around the single JSON object is tolerated.
   Escapes are NOT implemented: a field that would need an escape is a field
   the host should not be sending — reject keeps the parser allocation-free
   and injection-proof.

   Per-command key exclusivity is strict: each cmd accepts ONLY its own keys
   (diag: tag+repeat; io_probe: op; pool_probe: none; log: text; noop: none).

   Pure function: no I/O, no allocation, no global state. Compiles as C in the
   PRX composition (diagnostic.c discipline) and unmodified in host fixtures.

   IMPORT LAW (composition): the only libc the scanner needs is
   strcmp/strlen — both strong members of the composed candidate's 196-import
   contract (verified present in l10-1/l10-2/l11-2 supplier-contract.json:
   libSceLibcInternal/libSceLibcInternal). Zero new imports. */

#ifndef INSESSION_CMD2_CORE_H
#define INSESSION_CMD2_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Input budget: one FTP STOR of the intake file larger than this is consumed
   as oversize without parsing (bounded read in the service never buffers
   more). Same 512 B cap as grammar v1. */
#define INSESSION2_CMD_MAX 512u

/* tag: 1..48 chars of [A-Za-z0-9_.-]. diagnostic.c's supplier sink carries
   tags up to 96 chars, so "insession_" + 48 always fits. */
#define INSESSION2_TAG_MAX 48u

/* repeat: 1..1024. Carried as the diagnostic_event value in ONE call — v2
   never emits the event `repeat` times (one bounded sink row per command). */
#define INSESSION2_REPEAT_MAX 1024u

/* log text: 1..96 bytes of printable ASCII (0x20..0x7e). The scanner's
   string rule already rejects escapes/control/high bytes; the length cap
   keeps the composed UE4SS.log line inside the 256-byte entry-line family
   (runtime_module_entry RUNTIME_LOG_LINE_CAPACITY). */
#define INSESSION2_TEXT_MAX 96u

/* io_probe op slots: exactly "a"|"b"|"c"|"d" -> slots 0..3. */
#define INSESSION2_IO_SLOTS 4u

enum insession2_verdict
{
    INSESSION2_DIAG = 0,      /* diagnostic_event("insession_" + tag, repeat) */
    INSESSION2_NOOP = 1,      /* one UE4SS.log ack line only */
    INSESSION2_IO_PROBE = 2,  /* call registered IO control slot 0..3 (a..d) */
    INSESSION2_POOL_PROBE = 3,/* fresh bridge_kernel_write_copyin_probe */
    INSESSION2_LOG = 4,       /* append sanitized text line to UE4SS.log */
    INSESSION2_REJECT = 5     /* loud log + consumed; nothing executed */
};

struct insession2_cmd
{
    int verdict;                      /* enum insession2_verdict */
    char tag[INSESSION2_TAG_MAX + 1]; /* NUL-terminated; DIAG only */
    char text[INSESSION2_TEXT_MAX + 1]; /* NUL-terminated; LOG only */
    uint32_t repeat;                  /* 1..INSESSION2_REPEAT_MAX; DIAG only */
    int io_slot;                      /* 0..3; IO_PROBE only ('a'->0) */
    const char *reason;               /* REJECT: static literal reason; else NULL */
};

/* Parse ONE in-session command. buf/n need no NUL terminator; the scanner
   never reads past n. Leading/trailing whitespace around the single object is
   tolerated; everything else follows the strict grammar above. */
void insession2_parse(const char *buf, size_t n, struct insession2_cmd *out);

#ifdef __cplusplus
}
#endif

#endif /* INSESSION_CMD2_CORE_H */
