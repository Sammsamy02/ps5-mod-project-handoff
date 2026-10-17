/* optional_imports.h -- reference runtime side of the weak-import fallback.
 *
 * PROPOSAL ONLY: reference pattern for the composed runtime. The packaging
 * side (import_fallback.py) demotes the seven extension imports to STB_WEAK
 * in .dynsym; this header shows the mandatory call-site discipline that makes
 * that safe. On the PS5 loader (FreeBSD rtld lineage, same as the frozen PRX's
 * six existing weak imports) an unresolved weak undefined symbol binds to 0
 * instead of aborting the load -- so every entry point that reaches an
 * optional import MUST test the symbol address for NULL first.
 *
 * Design rules (enforced by review, see FALLBACK-DESIGN.md):
 *  R1. Snapshot addresses once at module_start into local function pointers;
 *      never call an optional import directly from arbitrary code paths.
 *  R2. Every snapshot feeds a capability flag; features degrade explicitly.
 *  R3. qsort has a complete in-module fallback (heapsort) so libc absence is
 *      transparent; the sce* dynamic-module APIs degrade to "feature off"
 *      with a recorded reason code, never a NULL call.
 *  R4. No optional import is referenced from destructors/fini paths, where a
 *      partially-bound module must still unwind cleanly.
 * Compiles as freestanding C99 (-ffreestanding -fno-builtin), no libc assumed.
 */
#ifndef OPTIONAL_IMPORTS_H
#define OPTIONAL_IMPORTS_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ API */
typedef struct optional_import_caps {
    int  dynamic_module_tools;   /* all four sce* resolves non-NULL       */
    int  qsort_available;        /* imported qsort resolved non-NULL      */
    const void *dlsym;           /* sceKernelDlsym   (NULL if unresolved) */
    const void *get_module_info; /* sceKernelGetModuleInfo                */
    const void *load_start;      /* sceKernelLoadStartModule              */
    const void *stop_unload;     /* sceKernelStopUnloadModule             */
    const void *qsort;           /* libc qsort                            */
} optional_import_caps;

/* R1: call once from module_start before any feature init; returns 0 and
 * fills *out always. Safe when every optional import is unresolved. */
static int optional_imports_probe(optional_import_caps *out);

/* R3: qsort entry point that never fails; delegates to the imported libc
 * qsort when present, otherwise the internal heapsort below. */
void optional_qsort(void *base, size_t nmemb, size_t size,
                    int (*compar)(const void *, const void *));

/* R2: reason codes recorded by feature owners when caps are missing. */
enum {
    OPTIMP_OK = 0,
    OPTIMP_NO_DLSYM = 1,
    OPTIMP_NO_MODULE_INFO = 2,
    OPTIMP_NO_LOAD_START = 3,
    OPTIMP_NO_STOP_UNLOAD = 4
};

/* -------------------------------------------- weak references (linkage) */
/* Declared exactly as the packaging contract emits them; WEAK comes from
 * the .dynsym bind type, not from source syntax. The SDK declares these
 * prototypes; duplicates here stay in sync with the frozen headers. */
long sceKernelDlsym(void *handle, const char *name, void **address);
long sceKernelGetModuleInfo(void *handle, void *info);
long sceKernelLoadStartModule(const char *path, void *args, uint32_t argsize,
                              uint32_t flags, void *pOpt);
long sceKernelStopUnloadModule(void *handle, uint32_t flags, void *pOpt);
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));

/* ------------------------------------------------------- implementation */
static int optional_imports_probe(optional_import_caps *out)
{
    if (!out) return -1;
    out->dlsym           = (const void *)sceKernelDlsym;
    out->get_module_info = (const void *)sceKernelGetModuleInfo;
    out->load_start      = (const void *)sceKernelLoadStartModule;
    out->stop_unload     = (const void *)sceKernelStopUnloadModule;
    out->qsort           = (const void *)qsort;
    /* R3: qsort presence is an optimization, not a capability. */
    out->qsort_available = out->qsort != 0;
    /* R2: dynamic-module tooling is all-or-nothing; partial sets are
     * reported through the reason codes, never used half-bound. */
    out->dynamic_module_tools =
        out->dlsym && out->get_module_info && out->load_start &&
        out->stop_unload;
    return 0;
}

static int optional_imports_reason(const optional_import_caps *caps)
{
    if (!caps->dlsym)           return OPTIMP_NO_DLSYM;
    if (!caps->get_module_info) return OPTIMP_NO_MODULE_INFO;
    if (!caps->load_start)      return OPTIMP_NO_LOAD_START;
    if (!caps->stop_unload)     return OPTIMP_NO_STOP_UNLOAD;
    return OPTIMP_OK;
}

/* --------------------------- internal qsort fallback (in-module heapsort) */
static void mb__sift(unsigned char *base, size_t size, long root, long end,
                     int (*compar)(const void *, const void *))
{
    for (;;) {
        long child = 2 * root + 1;
        if (child > end) return;
        if (child + 1 <= end &&
            compar(base + child * size, base + (child + 1) * size) < 0)
            ++child;
        if (compar(base + root * size, base + child * size) >= 0) return;
        { /* swap bytes without libc */
            size_t k;
            unsigned char *a = base + root * size, *b = base + child * size;
            for (k = 0; k < size; ++k) { unsigned char t = a[k]; a[k] = b[k]; b[k] = t; }
        }
        root = child;
    }
}

static void mb__heapsort(void *base, size_t nmemb, size_t size,
                         int (*compar)(const void *, const void *))
{
    long i, end = (long)nmemb - 1;
    unsigned char *p = (unsigned char *)base;
    if (end < 1 || !size) return;
    for (i = (end - 1) / 2; i >= 0; --i) mb__sift(p, size, i, end, compar);
    for (i = end; i > 0; --i) {
        size_t k;
        for (k = 0; k < size; ++k) {
            unsigned char t = p[k]; p[k] = p[i * size + k]; p[i * size + k] = t;
        }
        mb__sift(p, size, 0, i - 1, compar);
    }
}

static optional_import_caps mb__optimp;      /* zero-filled, no libc init */
static int mb__optimp_ready;

static const optional_import_caps *optional_imports_snapshot(void)
{
    if (!mb__optimp_ready) {
        (void)optional_imports_probe(&mb__optimp); /* R1: probe once */
        mb__optimp_ready = 1;
    }
    return &mb__optimp;
}

void optional_qsort(void *base, size_t nmemb, size_t size,
                    int (*compar)(const void *, const void *))
{
    const optional_import_caps *snapshot = optional_imports_snapshot();
    if (!base || !compar || nmemb < 2 || !size) return;
    if (snapshot->qsort_available) {
        ((void (*)(void *, size_t, size_t,
                   int (*)(const void *, const void *)))snapshot->qsort)
            (base, nmemb, size, compar);
        return;
    }
    mb__heapsort(base, nmemb, size, compar);
}

#endif /* OPTIONAL_IMPORTS_H */
