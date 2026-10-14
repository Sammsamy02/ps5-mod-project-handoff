/*
 * ps5_io_dir.h — directory-enumeration binding for the installer composition.
 *
 * Origin: worker-installer-composition, 2026-10-04 (owned; complements the
 * vendored ps5_io from worker-installer-ps5io, which is consumed unmodified).
 *
 * The known gap this closes: worker-7's modinst core needs three directory
 * operations ps5_io does not bind — recursive removal (rm_rf over staged
 * roots), journal-directory enumeration (recover_all), and directory-node
 * deletion (rmdir). Binding status, verified against the pinned SDK
 * (sdk-v0.43, llvm-nm -D over every target shared lib; receipts/):
 *
 *   PROVIDED (nm-verified exports of pinned target libs):
 *     opendir/readdir/closedir  -> libSceLibcInternal.so, libc_stub_weak.so,
 *                                  libScePosixForWebKit.so
 *     rmdir                     -> libkernel.so, libkernel_sys.so,
 *                                  libkernel_web.so, libkernel_stub_weak.so
 *   and, for everything non-enumerative, the vendored ps5_io sceKernel set
 *   (sceKernelOpen/Read/Write/Close/Lseek/Mkdir/Stat/Rename/Unlink/Fsync/
 *    ClockGettime) is reused unchanged.
 *
 *   MISSING (the exact absent exports — why enumeration is NOT bound over a
 *   pure sceKernel D-op set):
 *     sceKernelDopen, sceKernelDread, sceKernelDclose
 *   are defined by NO pinned target lib. sceKernelGetdirentries IS exported
 *   (libkernel*) and declared by the pinned SharpProspero
 *   Interop/Kernel/KernelFile.cs:187, but the pinned set declares no dirent
 *   struct layout for its buffer (no SceDirent anywhere in the SDK headers),
 *   so parsing it would be an unverifiable layout guess. The chosen binding
 *   is therefore the SDK's own dirent API: target/include/dirent.h (BSD
 *   layout) + the nm-verified libc opendir/readdir/closedir exports — the
 *   same provider class the composition already accepts for memcpy/strlen/
 *   snprintf, and the same route the SDK's own samples/list_files payload
 *   uses.
 *
 * Semantics (both flavors): ps5io_dir_* return 0 on success and -1 on
 * failure (errno-style diagnostics via the platform's errno only on host).
 * rm-tree is stat-free: it never follows symlinks because it only ever
 * unlinks entries found via readdir and rmdirs directories that opendir
 * proved to be directories.
 */
#ifndef PS5_IO_DIR_H
#define PS5_IO_DIR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remove path recursively; 0 if the path is gone (absent counts as gone),
 * -1 on the first irrecoverable error (a failing child does not stop the
 * walk; the walk reports failure but keeps removing what it can, like
 * worker-7's host rm_rf). */
int ps5io_dir_rm_rf(const char *path);

/* Iterate the *.json entries of dir, calling fn(title) with the file name
 * minus its .json suffix (a bounded copy, max 63 bytes; longer names are
 * skipped). Returns the number of callbacks made, or -1 if dir cannot be
 * opened. This is exactly recover_all's enumeration, rebound. */
int ps5io_dir_each_json(const char *dir, void (*fn)(const char *title));

#ifdef __cplusplus
}
#endif

#endif /* PS5_IO_DIR_H */
