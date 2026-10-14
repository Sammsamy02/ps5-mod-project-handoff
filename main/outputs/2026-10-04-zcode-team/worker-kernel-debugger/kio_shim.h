/*
 * kio_shim.h - thin, single-purpose ABI between the GDB stub and the
 * kstuffSharp kernel module (PayloadKernelIo).
 *
 * ARCHITECTURE
 * ------------
 * The TCP/RSP server runs in the *payload process* (FreeBSD userland on
 * Orbis), NOT in the kernel: kernel-mode sockets in a kld are painful,
 * and the payload already has a socket stack and a listen loop (the
 * existing debug receiver uses the same pattern).  The kernel module
 * exposes its process-memory primitives through a character device so
 * any privileged payload can use them.
 *
 * This header defines the expected kernel-side service.  It is
 * deliberately identical in shape to the safe-read service being built
 * by worker-kernel-saferead (read_mem/write_mem by pid+address): when
 * that worker publishes its exact ioctl numbers and structs, replace the
 * definitions below with its header - the gdbstub core and ps5_backend.c
 * do not change.
 *
 *      /dev/kstuff0   (char device, privileged open)
 *
 *      KIO_READ_MEM   struct kio_mem_req   read  game memory
 *      KIO_WRITE_MEM  struct kio_mem_req   write game memory
 *      KIO_GET_REGS   struct kio_regs_req  thread context snapshot
 *      KIO_SET_REGS   struct kio_regs_req  write thread context
 *
 * Kernel-side implementation notes (kstuffSharp, FreeBSD-derived Orbis
 * kernel) are collected in ps5_backend.c.
 */
#ifndef KIO_SHIM_H
#define KIO_SHIM_H

#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define KIO_DEV "/dev/kstuff0"

/* matching struct+number assignments; adjust once the saferead service
 * publishes its final ABI (keep the field semantics identical) */
#ifndef _KIO_ABI_FIXED

struct kio_mem_req {
    uint64_t pid;     /* in: target process id                      */
    uint64_t addr;    /* in: target virtual address                 */
    uint64_t len;     /* in: number of bytes                        */
    uint64_t buf;     /* in: user pointer of len bytes              */
    int32_t  rc;      /* out: bytes transferred, or errno-style neg */
    uint32_t pad;
};

struct kio_regs_req {
    uint64_t pid;     /* in: target process id                      */
    uint64_t tid;     /* in: thread id (0 = first / main thread)     */
    uint64_t buf;     /* in: user pointer to struct gdb_regs (536B)  */
    int32_t  rc;      /* out: 0 or errno-style neg                  */
    uint32_t pad;
};

#define KIO_IOC_READ_MEM  _IOWR('K', 0x10, struct kio_mem_req)
#define KIO_IOC_WRITE_MEM _IOWR('K', 0x11, struct kio_mem_req)
#define KIO_IOC_GET_REGS  _IOWR('K', 0x12, struct kio_regs_req)
#define KIO_IOC_SET_REGS  _IOWR('K', 0x13, struct kio_regs_req)

#endif /* _KIO_ABI_FIXED */

int kio_open(const char *dev);       /* -1 on error, fd otherwise */
int kio_read_mem(int fd, uint64_t pid, uint64_t addr, void *buf,
                 size_t len);
int kio_write_mem(int fd, uint64_t pid, uint64_t addr, const void *buf,
                  size_t len);
int kio_get_regs(int fd, uint64_t pid, uint64_t tid, void *regs,
                 size_t len);

#endif /* KIO_SHIM_H */
