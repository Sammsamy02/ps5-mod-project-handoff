/* ARGV_ECHO — G1 argv-dispatch probe (worker-argv-probe, team 2026-10-04).
 *
 * Standalone SDK ELF for the pldmgr `GET /loadpayload:<path>` route — the
 * same payload class as the console-proven NEXT_DLL_THREADS.elf and
 * INITONCE_DLL_TEST.elf diagnostics and the SDK's own hello_argv sample.
 *
 * Contract (single bounded JSONL record per dispatch, appended):
 *   {"marker":"modbridge.argvprobe","schema":1,"build_id":"argv-<seed>",
 *    "argc":N,"argv":[...],"argv_capped":bool,"str_trunc":bool}
 *
 * Discipline inherited from worker-ingame-probe-v2:
 *   - the record is the FIRST and ONLY action of the payload (entry marker
 *     first: no scans, no extra imports, no libc chatter before it);
 *   - bounded: <=64 argv entries, each string escaped and capped at 64
 *     bytes (UTF-8 bytes; caps flagged, never silent);
 *   - argc is recorded EXACTLY as the loader provided it — the probe never
 *     synthesizes, normalizes or drops entries, so absence of forwarding is
 *     observable as argc==0 or argc==1-with-path-only;
 *   - no timestamps or randomness in the record: two dispatches of the same
 *     build produce byte-identical lines (two-run determinism check);
 *   - exits immediately after fclose (return 0; process quiesces).
 *
 * Imports: only what the standard SDK hosted link already provides (crt +
 * libc + libkernel), i.e. the exact link class proven to load and run on
 * console through this route three times. No extra libraries, no dlopen
 * family, no module-query identities.
 */
#include <stdio.h>

#ifndef ARGV_BUILD_ID
#define ARGV_BUILD_ID "argv-unknown"
#endif
#ifndef ARGV_LOG_PATH
#define ARGV_LOG_PATH "/data/modbridge-argv-probe.jsonl"
#endif

#define ARGV_MARKER   "modbridge.argvprobe"
#define ARGV_MAX_ARGS 64
#define ARGV_MAX_STR  64
#define ARGV_SCAN_CAP 4096

/* Bounded strlen; argv[i] is expected NUL-terminated but never trusted
 * beyond ARGV_SCAN_CAP bytes. */
static unsigned arg_buflen(const char *s) {
    unsigned n = 0;
    if (!s) return 0;
    while (n < ARGV_SCAN_CAP && s[n] != '\0') n++;
    return n;
}

/* Emit one JSON string, capped at ARGV_MAX_STR bytes of source text.
 * Returns 1 if truncation occurred. */
static int json_string(FILE *out, const char *s) {
    unsigned len = arg_buflen(s);
    unsigned n = (len > ARGV_MAX_STR) ? ARGV_MAX_STR : len;
    unsigned i;
    fputc('"', out);
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out);  break;
        case '\r': fputs("\\r", out);  break;
        case '\t': fputs("\\t", out);  break;
        default:
            if (c < 0x20) fprintf(out, "\\u%04x", c);
            else fputc((int)c, out);
        }
    }
    fputc('"', out);
    return (len > ARGV_MAX_STR) ? 1 : 0;
}

int main(int argc, char *argv[]) {
    FILE *out;
    int counted, capped, trunc, i;

    out = fopen(ARGV_LOG_PATH, "a");
    if (!out) return 2; /* unwritable sink: exit distinctly, log nothing */

    counted = (argc < 0) ? 0 : argc;
    capped  = (counted > ARGV_MAX_ARGS) ? 1 : 0;
    if (capped) counted = ARGV_MAX_ARGS;

    fprintf(out, "{\"marker\":\"" ARGV_MARKER "\",\"schema\":1,"
                 "\"build_id\":\"" ARGV_BUILD_ID "\",\"argc\":%d,\"argv\":[",
            argc);

    trunc = 0;
    for (i = 0; i < counted; i++) {
        if (i) fputc(',', out);
        trunc |= json_string(out, argv[i] ? argv[i] : "");
    }

    fprintf(out, "],\"argv_capped\":%s,\"str_trunc\":%s}\n",
            capped ? "true" : "false", trunc ? "true" : "false");

    fclose(out);
    return 0;
}
