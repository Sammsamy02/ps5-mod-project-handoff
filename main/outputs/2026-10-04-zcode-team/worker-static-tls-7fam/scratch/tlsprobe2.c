/* TLS directory emission probe v2: single .tls region object, referenced from
 * code; __tls_used in a dedicated .rdataT section; directory 9 patched by the
 * build driver post-link (lld-link has no __tls_used synthesis - verified). */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;
typedef void tls_cb(void *, u32, void *);
#pragma section(".tls", read, write)
#pragma section(".rdataC", read)
#pragma section(".rdataT", read)
__declspec(allocate(".tls")) u8 mb_tls_region[24] = {
    (u8)0x54, (u8)0x4c, (u8)0x53, (u8)0x37, (u8)0xf0, (u8)0x7f,
    (u8)0x11, (u8)0x3b, (u8)0xa5, (u8)0x5c, (u8)0x92, (u8)0x6e,
    (u8)0x30, (u8)0xd4, (u8)0x88, (u8)0x1c,
    0u, 0u, 0u, 0u,
    (u8)0xe9, (u8)0x37, (u8)0xa1, (u8)0x7d};
#define mb_tls_template ((const u8 *)mb_tls_region)
#define mb_tls_index_cell (*(volatile u32 *)(mb_tls_region + 16))
#define mb_tls_region_end ((const u8 *)mb_tls_region + sizeof(mb_tls_region))
void mb_tls_cb1(void *h, u32 r, void *v) {(void)h;(void)r;(void)v;}
void mb_tls_cb2(void *h, u32 r, void *v) {(void)h;(void)r;(void)v;}
__declspec(allocate(".rdataC")) tls_cb *const mb_tls_callbacks[3] = {
    mb_tls_cb1, mb_tls_cb2, (tls_cb *)0};
typedef struct {u64 Start;u64 End;u64 Index;u64 Callbacks;u32 ZeroFill;u32 Chars;} tls_dir64;
__declspec(allocate(".rdataT")) const tls_dir64 __tls_used = {
    (u64)mb_tls_region, (u64)mb_tls_region_end, (u64)&mb_tls_index_cell,
    (u64)&mb_tls_callbacks[0], 0u, 0x500000u};
u32 TlsProbeIndex(void) {return mb_tls_index_cell + (u32)mb_tls_template[0];}
