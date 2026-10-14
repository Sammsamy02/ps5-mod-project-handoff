/*
 * gdbstub.h - GDB Remote Serial Protocol server, portable core.
 *
 * PS5 UE4SS runtime port / ZCode priority #5: live debugger channel.
 *
 * The RSP core (packet framing, checksums, command dispatch, TCP listener)
 * is fully portable POSIX C and knows nothing about how memory/register
 * access is performed.  All target access goes through `struct gdb_backend`,
 * so the exact same server binary runs:
 *
 *   - on the host, backed by mock memory (development / protocol tests), or
 *   - in the PS5 payload process, backed by kernel I/O (kstuffSharp
 *     PayloadKernelIo) via the kio shim in kio_shim.h.
 *
 * Target architecture is fixed to x86-64 (PS5 CPU is AMD Zen 2; OrbisOS is
 * FreeBSD-derived), so a debugger on any host (arm64 Mac included) sees a
 * x86_64 remote target through target.xml / qHostInfo.
 */
#ifndef GDBSTUB_H
#define GDBSTUB_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Advertised RSP PacketSize (hex 0x4000 = 16384 incl. framing overhead). */
#define GDB_PACKET_SIZE   0x4000
/* Hard cap on a single m/M/X data transfer (protocol safety). */
#define GDB_MEM_XFER_MAX  (8u * 1024u * 1024u)

#define GDB_DEFAULT_PORT  2345

/* ---------------------------------------------------------------------
 * x86-64 remote register file.
 *
 * Layout matches the served target.xml and the 'g' packet byte-for-byte:
 *   regnum  0..15 : rax rbx rcx rdx rsi rdi rbp rsp r8..r15 (8 bytes)
 *   regnum 16     : rip                                     (8 bytes)
 *   regnum 17     : eflags                                  (4 bytes)
 *   regnum 18..23 : cs ss ds es fs gs                       (4 bytes each)
 *   regnum 24..31 : st0..st7                                (10 bytes each)
 *   regnum 32..39 : fctrl fstat ftag fiseg fioff foseg fooff fop (4 bytes)
 *   regnum 40..55 : xmm0..xmm15                             (16 bytes each)
 *   regnum 56     : mxcsr                                   (4 bytes)
 * Total 536 bytes -> 1072 hex chars in the 'g' packet.
 * ------------------------------------------------------------------- */
#define GDB_NUM_REGS      57
#define GDB_GPACKET_BYTES 536

struct gdb_regs {
    uint64_t gpr[16];      /* rax..r15                        */
    uint64_t rip;
    uint32_t eflags;
    uint32_t seg[6];       /* cs ss ds es fs gs               */
    uint8_t  st[8][10];    /* st0..st7 raw 80-bit             */
    uint32_t fpu[8];       /* fctrl..fop                      */
    uint8_t  xmm[16][16];  /* xmm0..xmm15                     */
    uint32_t mxcsr;
};

_Static_assert(sizeof(struct gdb_regs) == GDB_GPACKET_BYTES,
               "gdb_regs layout must match served target.xml");

/* Register descriptor used to build target.xml, the 'g' packer and the
 * 'p'/'P' single-register commands.  `roff` is the byte offset of the
 * register inside struct gdb_regs. */
struct gdb_reg_desc {
    const char *name;
    uint16_t    regnum;
    uint16_t    bytes;
    uint16_t    roff;      /* offset within struct gdb_regs  */
    const char *group;     /* NULL -> general                */
};

extern const struct gdb_reg_desc gdb_reg_table[GDB_NUM_REGS];
const char *gdb_target_xml(void);   /* built once from gdb_reg_table */
const char *gdb_threads_xml(void);

/* ---------------------------------------------------------------------
 * Backend vtable: THE swap point between host mock and PS5 kernel I/O.
 *
 * Return conventions:
 *   read_mem/write_mem : number of bytes transferred, or -1 on fault.
 *   others             : 0 success, -1 failure (stub replies Enn).
 * ------------------------------------------------------------------- */
struct gdb_backend {
    const char *name;
    void       *ctx;

    /* Select the debuggee process.  Called at startup and on
     * "vAttach;<pid>" / "monitor pid <n>". */
    int  (*attach)(void *ctx, uint64_t pid);
    void (*detach)(void *ctx);

    /* Memory access - in the PS5 build these are the PayloadKernelIo
     * read/write calls (same service the worker-kernel-saferead safe-read
     * daemon uses; see kio_shim.h). */
    long (*read_mem)(void *ctx, uint64_t addr, void *out, size_t len);
    long (*write_mem)(void *ctx, uint64_t addr, const void *in, size_t len);

    /* Thread context snapshot (kernel: stopped thread's td_frame +
     * pcb->pcb_save FPU area; see ps5_backend.c). */
    int  (*read_regs)(void *ctx, struct gdb_regs *regs);
    int  (*write_regs)(void *ctx, const struct gdb_regs *regs);
    int  (*write_reg)(void *ctx, unsigned regnum, const void *buf, unsigned len);

    /* Phase-1 software breakpoints: backend must save the original byte,
     * write 0xCC, and restore it on clear.  PS5: needs writable text
     * (kernel work item #3) until then returns -1. */
    int  (*set_bp)(void *ctx, uint64_t addr, unsigned kind);
    int  (*clear_bp)(void *ctx, uint64_t addr, unsigned kind);
};

/* Serve exactly one connection at a time, forever.  Returns only on fatal
 * error.  listen_addr: IPv4 string, NULL -> 127.0.0.1. */
int gdbstub_serve(const struct gdb_backend *be, const char *listen_addr,
                  int port);

#endif /* GDBSTUB_H */
