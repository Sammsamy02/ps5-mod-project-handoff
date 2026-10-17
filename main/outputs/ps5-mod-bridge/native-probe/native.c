/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "payload_args.h"
#include "report.h"

/* Candidate loader ABI: this routine does not establish pointer validity. */
int probe_entry(payload_args_t *args) {
    struct probe_facts facts;
    probe_writer writer = 0;
    char report[4096];
    unsigned long length;
    int result;
    if (!args) return 2;
    result = probe_collect(args->sys_dynlib_dlsym, &facts, &writer);
    if (result == 0) {
#if defined(__PROSPERO__) && defined(__x86_64__)
        const char *origin = "native_entry_unverified";
#else
        const char *origin = "desktop_fixture";
#endif
        length = probe_format(&facts, origin, report, sizeof(report));
        result = length ? probe_emit(writer, report, length) : 4;
    }
    if (args->payloadout) *args->payloadout = result;
    return result;
}
