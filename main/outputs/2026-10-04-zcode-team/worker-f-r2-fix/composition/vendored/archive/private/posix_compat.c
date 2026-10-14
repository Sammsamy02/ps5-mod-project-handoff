#define _GNU_SOURCE 1
#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "posix_compat.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND)
#define RAW_CALL(call,number,a,b,c,d,e) mb_archive_posix_test_call(call,(long)(a),(long)(intptr_t)(b),(long)(intptr_t)(c),(long)(intptr_t)(d),(long)(e))
#elif defined(__PROSPERO__)
#if !defined(__x86_64__) || !defined(__LP64__) || !defined(__FreeBSD__)
#error "pinned PS5 ordinary syscall boundary requires FreeBSD x86_64 LP64 ABI"
#endif
#include <sys/syscall.h>
struct mb_archive_posix_raw_result { long value;unsigned char carry; };
_Static_assert(sizeof(long)==8&&sizeof(void*)==8,"pinned native register widths");

/* SDK v0.43 libc/syscalls.c uses these same ordinary filesystem syscalls,
 * but returns before preserving CF. Capture it before any flag-writing
 * instruction; never infer an error from a descriptor's numeric value.
 * RDX is both argument3 and BSD's second return register. The early-clobber
 * byte output cannot alias either read/write result register or an input. */
static struct mb_archive_posix_raw_result raw_call(long number,long a1,long a2,long a3,long a4,long a5) {
 register long argument4 __asm__("r10")=a4;
 register long argument5 __asm__("r8")=a5;
 unsigned char carry;long value=number;
 __asm__ __volatile__("syscall\n\tsetc %2"
  : "+a"(value),"+d"(a3),"=&q"(carry)
  : "D"(a1),"S"(a2),"r"(argument4),"r"(argument5)
  : "rcx","r11","cc","memory");
 return (struct mb_archive_posix_raw_result){value,carry};
}
#define RAW_CALL(call,number,a,b,c,d,e) raw_call(number,(long)(a),(long)(intptr_t)(b),(long)(intptr_t)(c),(long)(intptr_t)(d),(long)(e))
#endif

#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND) || defined(__PROSPERO__)
static int finish_result(struct mb_archive_posix_raw_result result,int descriptor) {
 if(result.carry) {
  errno=result.value>0&&result.value<=INT_MAX?(int)result.value:EIO;
  return -1;
 }
 if(result.value<0||result.value>INT_MAX) { errno=EOVERFLOW;return -1; }
 if(!descriptor&&result.value!=0) { errno=EIO;return -1; }
 return (int)result.value;
}
#endif
int mb_archive_posix_openat(int directory,const char *name,int flags,mode_t mode) {
#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND) || defined(__PROSPERO__)
 return finish_result(RAW_CALL(MB_ARCHIVE_POSIX_OPENAT,SYS_openat,directory,name,flags,mode,0),1);
#else
 return openat(directory,name,flags,mode);
#endif
}
int mb_archive_posix_fstatat(int directory,const char *name,struct stat *out,int flags) {
#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND) || defined(__PROSPERO__)
 return finish_result(RAW_CALL(MB_ARCHIVE_POSIX_FSTATAT,SYS_fstatat,directory,name,out,flags,0),0);
#else
 return fstatat(directory,name,out,flags);
#endif
}
int mb_archive_posix_mkdirat(int directory,const char *name,mode_t mode) {
#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND) || defined(__PROSPERO__)
 return finish_result(RAW_CALL(MB_ARCHIVE_POSIX_MKDIRAT,SYS_mkdirat,directory,name,mode,0,0),0);
#else
 return mkdirat(directory,name,mode);
#endif
}
int mb_archive_posix_linkat(int old_directory,const char *old_name,int new_directory,const char *new_name,int flags) {
#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND) || defined(__PROSPERO__)
 return finish_result(RAW_CALL(MB_ARCHIVE_POSIX_LINKAT,SYS_linkat,old_directory,old_name,new_directory,new_name,flags),0);
#else
 return linkat(old_directory,old_name,new_directory,new_name,flags);
#endif
}
int mb_archive_posix_unlinkat(int directory,const char *name,int flags) {
#if defined(MB_ARCHIVE_POSIX_TEST_BACKEND) || defined(__PROSPERO__)
 return finish_result(RAW_CALL(MB_ARCHIVE_POSIX_UNLINKAT,SYS_unlinkat,directory,name,flags,0,0),0);
#else
 return unlinkat(directory,name,flags);
#endif
}
