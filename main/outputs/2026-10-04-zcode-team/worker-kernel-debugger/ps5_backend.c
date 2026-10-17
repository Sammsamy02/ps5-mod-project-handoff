/*
 * ps5_backend.c - gdb_backend implementation backed by the kstuffSharp
 * kernel module (PayloadKernelIo) through the kio shim device.
 *
 * This file is host-compilable (every kernel dependency sits behind
 * kio_shim.h, and the ioctls simply fail with ENXIO/ENODEV when the
 * device is absent), so the same translation unit builds for the PS5
 * payload without edits.
 *
 * =====================================================================
 * KERNEL INTEGRATION PLAN (kstuffSharp side)
 * =====================================================================
 * The module owns four primitives, mapped 1:1 onto gdb_backend ops:
 *
 * 1. read_mem / write_mem  (KIO_READ_MEM / KIO_WRITE_MEM)
 *
 *    PayloadKernelIo already reads/writes arbitrary process memory from
 *    kernel context.  The ioctl handler wraps it:
 *
 *        struct proc *p = pfind(req->pid);          // + PROC_LOCK
 *        if (p == NULL)              -> ESRCH
 *        PHOLD(p); vmspace ref (vmspace_acquire_ref / p->p_vmspace)
 *        copy req->len bytes via the existing PayloadKernelIo mover
 *            between req->buf (caller payload, current vmspace) and
 *            p's vmspace at req->addr
 *        PRELE(p); drop vmspace ref
 *
 *    Fault policy: short transfers must report the failing sub-range
 *    (return partial count, or -EFAULT with the advanced address), so
 *    the stub can answer 'm' with a clean E01 instead of corrupt data.
 *    This is the same read path the worker-kernel-saferead safe-read
 *    daemon uses; both consumers should converge on ONE ioctl ABI.
 *
 * 2. read_regs (KIO_GET_REGS)
 *
 *    The game must be stopped for a coherent context (a running thread
 *    mutates its trapframe).  Sequence inside the module:
 *
 *        p = pfind(pid); PROC_LOCK(p)
 *        // request a stop equivalent to PT_STOP:
 *        kern_psignal(p, SIGSTOP);            // or proc_stop()/SINGLE_STOP
 *        wait until p->p_state == PRS_STOPPED (bounded msleep loop with
 *            timeout; on timeout return EBUSY so the stub answers E01
 *            instead of hanging the debugger connection)
 *        td = FIRST_THREAD_IN_PROC(p)         // tid 0; enumerate with
 *                                             // FOREACH_THREAD_IN_PROC
 *        tf = td->td_frame;                   // struct trapframe, the
 *                                             // user register save area
 *        out->gpr[i] = tf_rax/rbx/...;        // 16 GPRs
 *        out->rip    = tf_rip;
 *        out->rsp    = tf_rsp;
 *        out->eflags = tf_rflags;
 *        out->seg[0] = tf_cs; seg[1] = tf_ss; (ds/es/fs/gs not restored
 *                                             // on amd64 syscall entry;
 *                                             // read from pcb if needed)
 *        fpu = td->td_pcb->pcb_save;          // struct savefpu/xsave area
 *        memcpy st0-7, fctrl..fop, xmm0-15, mxcsr from the fxsave/xsave
 *            block inside pcb_save (sv_xmm / xstate).  The exact offsets
 *            differ between FXSR and XSAVE area formats - probe once via
 *            cpu_fxsr / fpu_kern_{enter,drop} conventions at module init.
 *
 *    Continue afterwards is implicit: the stub only stops the game while
 *    servicing g/p (and later Z0).  A "stop the world on connect" mode
 *    is a policy flag, not a requirement - phase-1 inspection can read a
 *    single stopped-and-resumed thread per query (STOP; copy; SIGCONT).
 *
 * 3. write_regs / write_reg (KIO_SET_REGS): symmetric writes into
 *    td_frame (+ pcb_save for FPU), only while stopped.  Phase-1 can
 *    leave these unimplemented (stub answers E01); patching the live
 *    game's RIP is a later, carefully reviewed step.
 *
 * 4. set_bp / clear_bp (Z0/z0)
 *
 *    Software breakpoint = write 0xCC at target text address, remember
 *    the original byte, restore on z0.  Two hard prerequisites:
 *      - kernel work item #3 (writable text): game text pages are RX;
 *        the module must map the physical page writable into kernel
 *        space (pmap_kenter_temporary-style), patch, and re-protect -
 *        or lift the text write protection globally if Orbis enforces
 *        it against kernel mappings too (SMAP/PKU on Zen 2).
 *      - trap handling: int3 in the game must route somewhere the
 *        module can observe (install a debug exception hook / patch
 *        IDT entry 1 or the Orbis equivalent), report SIGTRAP 'T05'
 *        stop replies asynchronously, and single-step over the
 *        original byte on resume.  Until that exists set_bp returns
 *        -1 and the stub answers E01 (memory watch remains fully
 *        usable meanwhile).
 *
 * THREADS: qfThreads/qsThreads currently report one synthetic thread.
 * The module exposes KIO_LIST_THREADS (extend the ABI when needed) to
 * enumerate p->p_threads; then 'H'-selected threads map onto KIO_*_REGS
 * tid != 0 and LLDB/GDB get the real thread list.
 *
 * SECURITY/SAFETY:
 *   - the stub binds 127.0.0.1 by default; console use requires an
 *     explicit -a (the payload's network is not a public debug surface)
 *   - optional ACL: refuse pid == our own pid / pid 0 / kernel pids
 *   - writes and register/execution control stay behind explicit
 *     backend capability flags so a read-only build (-DREADONLY) maps
 *     write_mem/write_regs/set_bp to unconditional failures
 *   - 'k' (kill) is translated to detach: the server can never ask the
 *     kernel module to terminate the game.
 */
#include "gdbstub.h"
#include "kio_shim.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int kio_open(const char *dev)
{
    return open(dev ? dev : KIO_DEV, O_RDWR | O_CLOEXEC);
}

int kio_read_mem(int fd, uint64_t pid, uint64_t addr, void *buf, size_t len)
{
    struct kio_mem_req r;
    memset(&r, 0, sizeof(r));
    r.pid  = pid;
    r.addr = addr;
    r.len  = len;
    r.buf  = (uint64_t)(uintptr_t)buf;
    if (ioctl(fd, KIO_IOC_READ_MEM, &r) < 0)
        return -1;
    if (r.rc < 0) {
        errno = -r.rc;
        return -1;
    }
    return r.rc;
}

int kio_write_mem(int fd, uint64_t pid, uint64_t addr, const void *buf,
                  size_t len)
{
    struct kio_mem_req r;
    memset(&r, 0, sizeof(r));
    r.pid  = pid;
    r.addr = addr;
    r.len  = len;
    r.buf  = (uint64_t)(uintptr_t)buf;
    if (ioctl(fd, KIO_IOC_WRITE_MEM, &r) < 0)
        return -1;
    if (r.rc < 0) {
        errno = -r.rc;
        return -1;
    }
    return r.rc;
}

int kio_get_regs(int fd, uint64_t pid, uint64_t tid, void *regs, size_t len)
{
    struct kio_regs_req r;
    memset(&r, 0, sizeof(r));
    r.pid = pid;
    r.tid = tid;
    r.buf = (uint64_t)(uintptr_t)regs;
    if (len != sizeof(struct gdb_regs)) {
        errno = EINVAL;
        return -1;
    }
    if (ioctl(fd, KIO_IOC_GET_REGS, &r) < 0)
        return -1;
    if (r.rc < 0) {
        errno = -r.rc;
        return -1;
    }
    return 0;
}

/* ---- gdb_backend vtable ------------------------------------------- */

struct ps5_ctx {
    int      fd;
    uint64_t pid;
};

static struct ps5_ctx ps5;

static int ps5_attach(void *ctx, uint64_t pid)
{
    struct ps5_ctx *c = ctx;
    if (pid == 0) {
        errno = EINVAL;
        return -1;
    }
    c->pid = pid;
    return 0;
}

static void ps5_detach(void *ctx)
{
    struct ps5_ctx *c = ctx;
    c->pid = 0;
}

static long ps5_read_mem(void *ctx, uint64_t addr, void *out, size_t len)
{
    struct ps5_ctx *c = ctx;
    if (!c->pid || c->fd < 0)
        return -1;
    return kio_read_mem(c->fd, c->pid, addr, out, len);
}

static long ps5_write_mem(void *ctx, uint64_t addr, const void *in,
                          size_t len)
{
    struct ps5_ctx *c = ctx;
    if (!c->pid || c->fd < 0)
        return -1;
    return kio_write_mem(c->fd, c->pid, addr, in, len);
}

static int ps5_read_regs(void *ctx, struct gdb_regs *regs)
{
    struct ps5_ctx *c = ctx;
    if (!c->pid || c->fd < 0)
        return -1;
    return kio_get_regs(c->fd, c->pid, 0 /* tid 0 = first thread */, regs,
                        sizeof(*regs));
}

#ifdef GDBSTUB_PS5_READONLY
static long ps5_write_mem_ro(void *ctx, uint64_t addr, const void *in,
                             size_t len)
{
    (void)ctx; (void)addr; (void)in; (void)len;
    return -1;
}
#endif

const struct gdb_backend *ps5_backend(const char *dev)
{
    static struct gdb_backend b = {
        .name      = "ps5-kio",
        .ctx       = &ps5,
        .attach    = ps5_attach,
        .detach    = ps5_detach,
        .read_mem  = ps5_read_mem,
        .write_mem = ps5_write_mem,
        .read_regs = ps5_read_regs,
        .write_regs= NULL,        /* phase 2: KIO_IOC_SET_REGS */
        .write_reg = NULL,
        .set_bp    = NULL,        /* phase 2: needs writable text + int3 hook */
        .clear_bp  = NULL,
    };
#ifdef GDBSTUB_PS5_READONLY
    b.write_mem = ps5_write_mem_ro;
#endif
    if (ps5.fd > 0) {
        close(ps5.fd);
        ps5.fd = -1;
    }
    ps5.fd = kio_open(dev);
    if (ps5.fd < 0) {
        fprintf(stderr, "ps5-kio: cannot open %s: %s "
                "(host build: expected; on console this must succeed)\n",
                dev ? dev : KIO_DEV, strerror(errno));
    }
    return &b;
}
