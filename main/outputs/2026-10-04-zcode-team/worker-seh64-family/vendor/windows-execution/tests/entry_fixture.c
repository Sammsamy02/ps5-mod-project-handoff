/* Owned fixture for Windows dynamic attach/detach. No CRT, TLS or threads. */
#ifndef REJECT_ATTACH
#define REJECT_ATTACH 0
#endif
typedef unsigned int u32;
typedef unsigned long long u64;
struct trace { u32 attach, detach, sequence, bad_arguments; u64 image_base; };
__declspec(dllexport) volatile struct trace EntryTrace;
extern unsigned char __ImageBase;
static void *volatile linked_base = &__ImageBase; /* Exercise DIR64 relocation. */

int Entry(void *image, u32 reason, void *reserved)
{
    if (image != (void *)&__ImageBase || image != linked_base || reserved != (void *)0 || reason > 1) {
        ++EntryTrace.bad_arguments;
        return 0;
    }
    EntryTrace.image_base = (u64)image;
    if (reason == 1) {
        ++EntryTrace.attach;
        EntryTrace.sequence = EntryTrace.sequence * 10 + 1;
        return REJECT_ATTACH ? 0 : 7; /* Any nonzero BOOL means success. */
    }
    ++EntryTrace.detach;
    EntryTrace.sequence = EntryTrace.sequence * 10 + 2;
    return 0; /* Detach return must be ignored. */
}

__declspec(dllexport) u32 Status(void)
{
    return EntryTrace.attach * 100 + EntryTrace.detach * 10 + EntryTrace.bad_arguments;
}
