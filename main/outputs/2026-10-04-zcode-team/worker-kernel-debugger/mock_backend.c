/*
 * mock_backend.c - host-development backend with a deterministic 1 MiB
 * mock memory image and canned register file.
 *
 * Everything the host tests assert on lives here, in one place:
 *
 *   MOCK_BASE+0x00000 : fake ELF header  7f 45 4c 46 02 01 01 00 ...
 *   MOCK_BASE+0x00020 : u64 0xCAFEBABEDEADBEEF (little endian)
 *   MOCK_BASE+0x00040 : "PS5 UE4SS GDB MOCK v1\0"
 *   MOCK_BASE+0x00100 : mock code: 55 48 89 e5 48 83 ec 20 (prologue)
 *   MOCK_BASE+0x01000 : scratch page for M/X write tests
 *
 * Registers: rax=0x1122334455667788 rbx=0xdeadbeefcafe0000
 *            rip=MOCK_BASE+0x104   rsp=0x0000007ffffffe00
 *            rbp=0x0000007ffffffe40 eflags=0x246  cs=0x33 ss=0x2b
 */
#include "gdbstub.h"

#include <stdio.h>
#include <string.h>

#define MOCK_BASE 0x00400000ull
#define MOCK_SIZE 0x00100000ull /* 1 MiB */

static uint8_t mock_mem[MOCK_SIZE];
static uint64_t mock_pid = 0;

#define MAX_MOCK_BPS 64
struct mock_bp {
    uint64_t addr;
    uint8_t  orig;
    int      used;
};
static struct mock_bp mock_bps[MAX_MOCK_BPS];

static void mock_init(void)
{
    static int done = 0;
    if (done)
        return;
    done = 1;
    memset(mock_mem, 0, sizeof(mock_mem));

    /* fake ELF header */
    static const uint8_t ehdr[] = {
        0x7f, 'E', 'L', 'F', 0x02, 0x01, 0x01, 0x00, /* ELF64 LSB SysV  */
        0x00, 0x00,                                  /* padding         */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x03, 0x00,                                  /* ET_DYN          */
        0x3e, 0x00,                                  /* EM_X86_64       */
    };
    memcpy(mock_mem + 0x0000, ehdr, sizeof(ehdr));

    /* u64 magic (placed clear of the ELF header, which spans 0x00-0x13) */
    const uint64_t magic = 0xCAFEBABEDEADBEEFull;
    memcpy(mock_mem + 0x0020, &magic, 8);

    /* banner */
    memcpy(mock_mem + 0x0040, "PS5 UE4SS GDB MOCK v1", 22);

    /* mock function prologue at +0x100: push rbp; mov rbp,rsp; sub 0x20 */
    static const uint8_t prologue[] = {
        0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec, 0x20,
        0x48, 0x89, 0x7d, 0xf8, 0x31, 0xc0, 0xc3,
    };
    memcpy(mock_mem + 0x0100, prologue, sizeof(prologue));
}

static long mock_read_mem(void *ctx, uint64_t addr, void *out, size_t len)
{
    (void)ctx;
    mock_init();
    if (addr < MOCK_BASE || addr - MOCK_BASE >= MOCK_SIZE ||
        len > MOCK_SIZE - (addr - MOCK_BASE))
        return -1;
    memcpy(out, mock_mem + (addr - MOCK_BASE), len);
    return (long)len;
}

static long mock_write_mem(void *ctx, uint64_t addr, const void *in,
                           size_t len)
{
    (void)ctx;
    mock_init();
    if (addr < MOCK_BASE || addr - MOCK_BASE >= MOCK_SIZE ||
        len > MOCK_SIZE - (addr - MOCK_BASE))
        return -1;
    memcpy(mock_mem + (addr - MOCK_BASE), in, len);
    return (long)len;
}

static struct gdb_regs mock_regs;
static int mock_regs_valid = 0;

static void mock_regs_init(void)
{
    if (mock_regs_valid)
        return;
    mock_regs_valid = 1;
    memset(&mock_regs, 0, sizeof(mock_regs));
    mock_regs.gpr[0]  = 0x1122334455667788ull;          /* rax */
    mock_regs.gpr[1]  = 0xdeadbeefcafe0000ull;          /* rbx */
    mock_regs.gpr[2]  = 0x0000000000000045ull;          /* rcx */
    mock_regs.gpr[3]  = 0x0000000000000067ull;          /* rdx */
    mock_regs.gpr[6]  = 0x0000007ffffffe40ull;          /* rbp */
    mock_regs.gpr[7]  = 0x0000007ffffffe00ull;          /* rsp */
    mock_regs.rip     = MOCK_BASE + 0x104;              /* in prologue */
    mock_regs.eflags  = 0x246;                          /* ZF|PF|IF    */
    mock_regs.seg[0]  = 0x33;                           /* cs */
    mock_regs.seg[1]  = 0x2b;                           /* ss */
    mock_regs.mxcsr   = 0x1f80;
    memset(mock_regs.xmm[0], 0x5a, 16);                 /* xmm0='Z'*16 */
}

static int mock_read_regs(void *ctx, struct gdb_regs *regs)
{
    (void)ctx;
    mock_init();
    mock_regs_init();
    *regs = mock_regs;
    return 0;
}

static int mock_write_regs(void *ctx, const struct gdb_regs *regs)
{
    (void)ctx;
    mock_regs_init();
    mock_regs = *regs;
    return 0;
}

static int mock_write_reg(void *ctx, unsigned regnum, const void *buf,
                          unsigned len)
{
    (void)ctx;
    mock_regs_init();
    if (regnum >= GDB_NUM_REGS)
        return -1;
    const struct gdb_reg_desc *d = &gdb_reg_table[regnum];
    if (len != d->bytes)
        return -1;
    memcpy((unsigned char *)&mock_regs + d->roff, buf, len);
    return 0;
}

static int mock_set_bp(void *ctx, uint64_t addr, unsigned kind)
{
    (void)ctx;
    (void)kind;
    mock_init();
    if (addr < MOCK_BASE || addr - MOCK_BASE >= MOCK_SIZE)
        return -1;
    for (int i = 0; i < MAX_MOCK_BPS; i++) {
        if (mock_bps[i].used && mock_bps[i].addr == addr)
            return 0; /* already set */
    }
    for (int i = 0; i < MAX_MOCK_BPS; i++) {
        if (!mock_bps[i].used) {
            mock_bps[i].addr = addr;
            mock_bps[i].orig = mock_mem[addr - MOCK_BASE];
            mock_mem[addr - MOCK_BASE] = 0xCC;      /* int3 */
            mock_bps[i].used = 1;
            return 0;
        }
    }
    return -1;
}

static int mock_clear_bp(void *ctx, uint64_t addr, unsigned kind)
{
    (void)ctx;
    (void)kind;
    for (int i = 0; i < MAX_MOCK_BPS; i++) {
        if (mock_bps[i].used && mock_bps[i].addr == addr) {
            mock_mem[addr - MOCK_BASE] = mock_bps[i].orig;
            mock_bps[i].used = 0;
            return 0;
        }
    }
    return -1;
}

static int mock_attach(void *ctx, uint64_t pid)
{
    (void)ctx;
    mock_init();
    mock_pid = pid;
    fprintf(stderr, "mock: attached to pid %llu (any pid accepted)\n",
            (unsigned long long)pid);
    return 0;
}

static void mock_detach(void *ctx) { (void)ctx; mock_pid = 0; }

const struct gdb_backend *mock_backend(void)
{
    static const struct gdb_backend b = {
        .name      = "mock",
        .ctx       = NULL,
        .attach    = mock_attach,
        .detach    = mock_detach,
        .read_mem  = mock_read_mem,
        .write_mem = mock_write_mem,
        .read_regs = mock_read_regs,
        .write_regs= mock_write_regs,
        .write_reg = mock_write_reg,
        .set_bp    = mock_set_bp,
        .clear_bp  = mock_clear_bp,
    };
    return &b;
}
