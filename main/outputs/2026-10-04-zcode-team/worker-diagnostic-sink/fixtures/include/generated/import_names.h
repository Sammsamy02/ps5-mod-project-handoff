/* host_import_names.h — minimal host stand-in for the lineage
   generated/import_names.h. Only what diagnostic.c's diagnostic_missing()
   needs; never compiled into the console PRX. */
#pragma once
#define PROBE_IMPORT_NAME_COUNT 4
static const char *const probe_import_names[PROBE_IMPORT_NAME_COUNT] = {
    "sceKernelOpen", "sceKernelWrite", "sceKernelClose", "sceKernelDlsym"};
