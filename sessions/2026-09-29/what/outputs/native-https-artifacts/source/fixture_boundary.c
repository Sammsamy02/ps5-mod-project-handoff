#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
/* Host-only boundary; this file and its symbols are excluded from native. */
static unsigned calls;
int fixture_sysctl(int *mib, unsigned count, void *out, size_t *length,
                   const void *new_value, size_t new_length) {
    if (!mib || count!=2 || mib[0]!=1 || mib[1]!=37 || !out || !length ||
        *length!=32 || new_value || new_length || errno || calls>=2) abort();
    unsigned char *b=out;
    for (unsigned i=0;i<32;++i) b[i]=(unsigned char)(0x41u+calls*17u+i);
    ++calls; *length=32; errno=0; return 0;
}
void tls_attempt_test_cleanup(const void *storage,size_t size,unsigned before,unsigned after) {
    const unsigned char *p=storage;
    if (!before || !after) abort();
    for (size_t i=0;i<size;++i) if (p[i]) abort();
}
