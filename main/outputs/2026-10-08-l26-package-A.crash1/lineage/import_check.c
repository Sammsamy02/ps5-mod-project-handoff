#include "import_table.h"
#include "probe_api.h"
#include "diagnostic.h"

/* Validate only relocations in our own table. No provider memory is read. */
int probe_imports_validate(void) {
    unsigned missing = 0;
    diagnostic_event("import_validation_begin", PROBE_STRONG_IMPORT_COUNT);
    for (unsigned i = 0; i < PROBE_STRONG_IMPORT_COUNT; ++i) {
        if (modbridge_import_table[i] == 0) {
            if (missing == 0) diagnostic_missing(i);
            ++missing;
        }
    }
    diagnostic_event("import_validation_missing_count", missing);
    return missing ? -1 : 0;
}

/* Called only after validation and constructors have succeeded. */
void probe_imports_log_status(void) {
    probe_event("module_start");
    probe_event("imports_all_nonzero");
}
