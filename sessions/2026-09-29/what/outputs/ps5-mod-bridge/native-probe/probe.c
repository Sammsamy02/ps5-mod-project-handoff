/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "report.h"

static void *resolve(probe_resolver resolver, const char *name, int *code) {
    void *address = 0;
    *code = resolver(1, name, &address);
    if (*code == 0 && address) return address;
    address = 0;
    *code = resolver(0x2001, name, &address);
    return *code == 0 ? address : 0;
}

int probe_collect(probe_resolver resolver, struct probe_facts *facts, probe_writer *writer) {
    if (!facts || !writer) return 2;
    *writer = 0;
    facts->output_resolve_code = 0;
    for (int i = 0; i < 4; ++i) {
        facts->observation[i].state = PROBE_UNMEASURED;
        facts->observation[i].value = 0;
        facts->observation[i].resolve_code = 0;
        facts->observation[i].has_value = 0;
    }
    if (!resolver) return 2;
    for (int i = 0; i < 4; ++i) {
        const char *name = i == 0 ? "getpid" : i == 1 ? "getuid" : i == 2 ? "geteuid" : "getpagesize";
        struct probe_observation *observation = &facts->observation[i];
        void *address = resolve(resolver, name, &observation->resolve_code);
        observation->state = PROBE_UNAVAILABLE;
        if (!address) continue;
        observation->has_value = 1;
        if (i == 1 || i == 2) observation->value = ((unsigned int (*)(void))address)();
        else observation->value = ((int (*)(void))address)();
        if ((i == 0 || i == 3) && observation->value <= 0) continue;
        observation->state = PROBE_OBSERVED;
    }
    *writer = (probe_writer)resolve(resolver, "write", &facts->output_resolve_code);
    return *writer ? 0 : 3;
}

int probe_emit(probe_writer writer, const char *bytes, unsigned long length) {
    unsigned long sent = 0;
    if (!writer || !bytes || !length || length > 4096) return 3;
    for (int attempt = 0; attempt < 8 && sent < length; ++attempt) {
        unsigned long remaining = length - sent;
        long count = writer(1, bytes + sent, remaining);
        if (count <= 0 || (unsigned long)count > remaining) return 3;
        sent += (unsigned long)count;
    }
    return sent == length ? 0 : 3;
}
