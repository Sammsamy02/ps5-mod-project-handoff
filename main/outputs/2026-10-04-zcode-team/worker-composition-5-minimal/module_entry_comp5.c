/* worker-composition-5-minimal: minimal checkpoint module_entry.
 *
 * Replaces the frozen lineage unit lifecycle/module_entry.c for the
 * composition-5 bisection experiment. The FULL frozen 191-import surface is
 * provided by the frozen import_references.S typed rows (byte-identical to
 * the console-proven aligned-entry lineage); this unit only proves that
 * module_start is reached and that the proven kernel write path works.
 *
 * Recipe precedents honored:
 * - force_align_arg_pointer + default visibility on module_start/module_stop
 *   (aligned-entry entry-alignment evidence).
 * - sceKernelOpen(O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW, 0600) + bounded
 *   64-attempt sceKernelWrite + sceKernelClose (igd1/v2 proven writer).
 * - module_start returns 0 ALWAYS, even if the open fails (igd1 directive:
 *   an open failure must never look like a start failure).
 * - No C++, no constructors walked, no experiments: inert init/fini slots
 *   only (sentinels.c pattern, self-contained).
 *
 * Variants (compile-time, no source drift between the two runs of a variant):
 *   -DCOMP5_TAG='"COMP5"'  -DCOMP5_IMPORTS='"192 imports"'
 *       [+ -DCOMP5_REF_MODULEINFO] -> composition-5 (191 + sceKernelGetModuleInfo)
 *   -DCOMP5_TAG='"COMP5Z"' -DCOMP5_IMPORTS='"191 imports"'
 *       -> composition-5-zero control (exactly the frozen 191).
 */
#include <stddef.h>
#include <stdint.h>
#include <sys/fcntl.h>

#ifndef COMP5_TAG
#define COMP5_TAG "COMP5"
#endif
#ifndef COMP5_IMPORTS
#define COMP5_IMPORTS "unknown imports"
#endif

/* Proven kernel imports (all three are rows of the frozen 191 contract). */
extern int sceKernelOpen(const char *, int, uint16_t);
extern int64_t sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);

#ifdef COMP5_REF_MODULEINFO
/* composition-5 only: untyped reference to the single added import,
 * mirroring the composed runtime's untyped reference + typed shim row
 * coexistence (proven pattern of worker-prx-link-gaps/package_prx.py).
 * The function pointer is logged but NEVER called: the experiment tests
 * import RESOLUTION at load time, not the callee's behavior. */
extern int sceKernelGetModuleInfo(int, void *);
static int (*volatile comp5_module_info)(int, void *) = sceKernelGetModuleInfo;
#endif

#define COMP5_LOG_PATH "/data/plugins/UE4SS.log"
#define COMP5_WRITE_ATTEMPTS 64u

typedef void (*comp5_callback)(void);

static void comp5_init_slot(void) { }
static void comp5_fini_slot(void) { }

/* Inert slots only (loader owns no arrays; module_start never walks them). */
static comp5_callback const comp5_init __attribute__((used, section(".init_array.00101"))) = comp5_init_slot;
static comp5_callback const comp5_fini __attribute__((used, section(".fini_array.00101"))) = comp5_fini_slot;

static size_t comp5_append(char *buffer, size_t *used, size_t capacity,
                           const char *text)
{
    while (*text && *used < capacity)
        buffer[(*used)++] = *text++;
    return *used;
}

#ifdef COMP5_REF_MODULEINFO
static void comp5_append_hex(char *buffer, size_t *used, size_t capacity,
                             uintptr_t value)
{
    static const char digits[] = "0123456789abcdef";
    for (int shift = (int)(sizeof(uintptr_t) * 8) - 4;
         shift >= 0 && *used < capacity; shift -= 4)
        buffer[(*used)++] = digits[(value >> shift) & 15];
}
#endif

static void comp5_write_line(int fd, const char *buffer, size_t used)
{
    size_t offset = 0;
    unsigned attempts = 0;
    while (offset < used && attempts < COMP5_WRITE_ATTEMPTS) {
        ++attempts;
        int64_t written = sceKernelWrite(fd, buffer + offset, used - offset);
        if (written <= 0 || (uint64_t)written > used - offset)
            return;
        offset += (size_t)written;
    }
}

static void comp5_log(void)
{
    char buffer[192];
    size_t used = 0;

    comp5_append(buffer, &used, sizeof(buffer) - 2,
                 COMP5_TAG ": " COMP5_IMPORTS ", module_start reached");
    buffer[used++] = '\n';

    int fd = sceKernelOpen(COMP5_LOG_PATH,
        O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW, 0600);
    if (fd < 0)
        return; /* still return 0 from module_start (igd1 directive) */

    comp5_write_line(fd, buffer, used);

#ifdef COMP5_REF_MODULEINFO
    /* Clearly-scoped second line: the added import resolved to a real
     * address at load time (0x0000000000000000 would mean unresolved/null).
     * Also reports the open descriptor so "file exists but empty" vs
     * "never opened" stays distinguishable. */
    used = 0;
    comp5_append(buffer, &used, sizeof(buffer) - 2,
                 COMP5_TAG ": sceKernelGetModuleInfo_ptr=0x");
    comp5_append_hex(buffer, &used, sizeof(buffer) - 2,
                     (uintptr_t)comp5_module_info);
    comp5_append(buffer, &used, sizeof(buffer) - 2, " fd=");
    if (fd >= 0 && fd < 10)
        buffer[used++] = (char)('0' + fd);
    buffer[used++] = '\n';
    comp5_write_line(fd, buffer, used);
#endif

    (void)sceKernelClose(fd);
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_start(size_t argc, const void *args)
{
    (void)argc; (void)args;
    comp5_log();
    return 0;
}

__attribute__((visibility("default"), force_align_arg_pointer))
int module_stop(size_t argc, const void *args)
{
    (void)argc; (void)args;
    return 0;
}
