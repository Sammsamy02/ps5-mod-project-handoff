#ifndef MB_ARCHIVE_PRIVATE_POSIX_COMPAT_H
#define MB_ARCHIVE_PRIVATE_POSIX_COMPAT_H

#include <sys/types.h>
struct stat;

/* Five ordinary relative-filesystem calls used by archive intake/callers.
 * Success/failure follows POSIX; openat always receives an explicit mode.
 * No global SDK symbol replacement or arbitrary syscall interface. */
int mb_archive_posix_openat(int directory,const char *name,int flags,mode_t mode);
int mb_archive_posix_fstatat(int directory,const char *name,struct stat *out,int flags);
int mb_archive_posix_mkdirat(int directory,const char *name,mode_t mode);
int mb_archive_posix_linkat(int old_directory,const char *old_name,int new_directory,const char *new_name,int flags);
int mb_archive_posix_unlinkat(int directory,const char *name,int flags);

#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND)
#if defined(__PROSPERO__)
#error "host-only POSIX compatibility test backend forbidden on native target"
#endif
/* Test-only raw return/carry injection; never a production runtime provider. */
enum mb_archive_posix_call { MB_ARCHIVE_POSIX_OPENAT,MB_ARCHIVE_POSIX_FSTATAT,
 MB_ARCHIVE_POSIX_MKDIRAT,MB_ARCHIVE_POSIX_LINKAT,MB_ARCHIVE_POSIX_UNLINKAT };
struct mb_archive_posix_raw_result { long value;unsigned char carry; };
struct mb_archive_posix_raw_result mb_archive_posix_test_call(enum mb_archive_posix_call call,
 long argument1,long argument2,long argument3,long argument4,long argument5);
#endif
#endif
