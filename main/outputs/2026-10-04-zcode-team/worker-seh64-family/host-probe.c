/* Standalone host probe: run the SEH64 owner's lookup/unwind against the
 * actual owned_consumer.dll image (sections mapped at RVAs like the loader
 * does, no relocation needed for the data plane). Diagnostic aid only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "mb_win_seh64.h"

static int dummy_personality(void *a, mb_win_seh64_exception_record *r, void *b, mb_win_seh64_context *c, mb_win_seh64_dispatcher_context *d) { (void)a;(void)r;(void)b;(void)c;(void)d; return 0; }
static int dummy_at_frame(void *a, uint64_t b, void *c) { (void)a;(void)b;(void)c; return 0; }
static void dummy_resume(const mb_win_seh64_context *c) { (void)c; }

int main(int argc, char **argv) {
    setvbuf(stdout, 0, _IONBF, 0); setvbuf(stderr, 0, _IONBF, 0);
    if (argc < 4) { fprintf(stderr, "usage: probe dll control_pc_rva rsp_rva\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror("open"); return 2; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *file = malloc((size_t)size);
    if (fread(file, 1, (size_t)size, f) != (size_t)size) { perror("read"); return 2; }
    fclose(f);
    uint32_t pe = *(uint32_t *)(file + 0x3c);
    uint32_t opt = pe + 24;
    uint32_t image_size = *(uint32_t *)(file + opt + 56);
    uint16_t nsec = *(uint16_t *)(file + pe + 6);
    unsigned char *image = calloc(1, image_size);
    uint32_t headers = *(uint32_t *)(file + opt + 60);
    memcpy(image, file, headers);
    for (unsigned i = 0; i < nsec; ++i) {
        unsigned char *s = file + opt + 240 + i * 40;
        uint32_t vsz = *(uint32_t *)(s + 8), rva = *(uint32_t *)(s + 12);
        uint32_t rsz = *(uint32_t *)(s + 16), ro = *(uint32_t *)(s + 20);
        if (rsz > vsz) rsz = vsz;
        memcpy(image + rva, file + ro, rsz);
    }
    uint32_t pdata_rva = *(uint32_t *)(file + opt + 112 + 3 * 8);
    uint32_t pdata_size = *(uint32_t *)(file + opt + 116 + 3 * 8);
    printf("image_size=%#x pdata=%#x+%#x\n", image_size, pdata_rva, pdata_size);

    mb_win_seh64 owner; memset(&owner, 0, sizeof(owner));
    mb_win_seh64_control control;
    control.call_personality = dummy_personality;
    control.call_at_frame = dummy_at_frame;
    control.resume = dummy_resume;
    mb_win_seh64_status st = mb_win_seh64_open(&owner, &control);
    printf("open=%s\n", mb_win_seh64_status_name(st));
    unsigned slot = 99;
    st = mb_win_seh64_register_image_table(&owner, image, image_size, pdata_rva, pdata_size, &slot);
    printf("register=%s slot=%u\n", mb_win_seh64_status_name(st), slot);

    uint64_t pc = (uint64_t)(uintptr_t)image + strtoull(argv[2], 0, 0);
    const mb_win_seh64_runtime_function *entry = 0;
    const uint8_t *base = 0;
    st = mb_win_seh64_lookup_function_entry(&owner, pc, &entry, &base);
    printf("lookup=%s entry=%p base=%p", mb_win_seh64_status_name(st), (void *)entry, (void *)base);
    if (entry) printf(" [%#x,%#x) x=%#x", entry->begin_address, entry->end_address, entry->unwind_data);
    printf("\n");
    if (st != MB_WIN_SEH64_OK) return 1;
    (void)!write(2, "A:before\n", 9);
    mb_win_seh64_context ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.rsp = (uint64_t)(uintptr_t)image + strtoull(argv[3], 0, 0);
    *(uint64_t *)(uintptr_t)(ctx.rsp) = pc; /* caller rip slot */
    mb_win_seh64_dispatcher_context dctx; memset(&dctx, 0, sizeof(dctx));
    dctx.image_base = base;
    mb_win_seh64_context_pointers ptrs; memset(&ptrs, 0, sizeof(ptrs));
    (void)!write(2, "B:call-unwind\n", 14);
    st = mb_win_seh64_virtual_unwind(0, pc, base, entry, &ctx, &dctx, &ptrs);
    (void)!write(2, "C:after-unwind\n", 15);
    printf("virtual_unwind=%s caller rip=%#llx rsp=%#llx establisher=%#llx frame_rsp=%#llx handler_rva=%u\n",
        mb_win_seh64_status_name(st), (unsigned long long)ctx.rip, (unsigned long long)ctx.rsp,
        (unsigned long long)dctx.establisher_frame, (unsigned long long)dctx.frame_rsp, dctx.handler_rva);
    return 0;
}
