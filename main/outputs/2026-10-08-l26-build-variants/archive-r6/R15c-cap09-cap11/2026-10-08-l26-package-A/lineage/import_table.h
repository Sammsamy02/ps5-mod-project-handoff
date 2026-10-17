#pragma once
#include <stdint.h>
#define PROBE_STRONG_IMPORT_COUNT 187u
#define PROBE_TOTAL_REFERENCE_COUNT 193u
extern const uintptr_t modbridge_import_table[PROBE_TOTAL_REFERENCE_COUNT];
int probe_imports_validate(void);
void probe_imports_log_status(void);
