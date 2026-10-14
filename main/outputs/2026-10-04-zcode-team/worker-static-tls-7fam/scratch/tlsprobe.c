/* TLS directory emission probe: manual IMAGE_TLS_DIRECTORY64 (__tls_used) with
 * $-ordered .tls subsections, no thread_local codegen, no CRT. */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef void tls_cb(void *, u32, void *);
#pragma section(".tls$A", read, write)
#pragma section(".tls$B", read, write)
#pragma section(".tls$Z", read, write)
#pragma section(".rdata$C", read)
#pragma section(".rdata$T", read)
__declspec(allocate(".tls$A")) static const char _tls_start[1] = {0};
__declspec(allocate(".tls$B")) static const char mb_tls_template[16] = {
    (char)0x54, (char)0x4c, (char)0x53, (char)0x37, (char)0xf0, (char)0x7f,
    (char)0x11, (char)0x3b, (char)0xa5, (char)0x5c, (char)0x92, (char)0x6e,
    (char)0x30, (char)0xd4, (char)0x88, (char)0x1c};
__declspec(allocate(".tls$B")) static u32 __tls_index = 0;
__declspec(allocate(".tls$Z")) static const char _tls_end[1] = {0};
void TlsProbeMark(void) {}
static void mb_tls_cb1(void *h, u32 r, void *v) {(void)h;(void)r;(void)v;}
static void mb_tls_cb2(void *h, u32 r, void *v) {(void)h;(void)r;(void)v;}
__declspec(allocate(".rdata$C")) static tls_cb *const mb_tls_callbacks[3] = {
    mb_tls_cb1, mb_tls_cb2, (tls_cb *)0};
typedef struct {u64 Start;u64 End;u64 Index;u64 Callbacks;u32 ZeroFill;u32 Chars;} tls_dir64;
__declspec(allocate(".rdata$T")) const tls_dir64 __tls_used = {
    (u64)&_tls_start, (u64)&_tls_end, (u64)&__tls_index,
    (u64)&mb_tls_callbacks[0], 0u, 0x500000u};
u32 *TlsProbeIndex(void) {return &__tls_index;}
