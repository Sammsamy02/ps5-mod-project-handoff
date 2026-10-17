/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MODBRIDGE_NATIVE_REPORT_H
#define MODBRIDGE_NATIVE_REPORT_H
enum probe_state { PROBE_UNMEASURED, PROBE_UNAVAILABLE, PROBE_OBSERVED };
struct probe_observation { enum probe_state state; long value; int resolve_code, has_value; };
struct probe_facts { struct probe_observation observation[4]; int output_resolve_code; };
typedef int (*probe_resolver)(int, const char *, void *);
typedef long (*probe_writer)(int, const void *, unsigned long);
int probe_collect(probe_resolver resolver, struct probe_facts *facts, probe_writer *writer);
unsigned long probe_format(const struct probe_facts *facts, const char *origin, char *out, unsigned long capacity);
int probe_emit(probe_writer writer, const char *bytes, unsigned long length);
#endif
