#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stddef.h>
#ifndef __PROSPERO__
#error genuine Prospero target required
#endif
#if __FreeBSD__ != 9 || __SIZEOF_POINTER__ != 8 || __SIZEOF_LONG__ != 8 || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error unexpected public target ABI
#endif
_Static_assert(__builtin_types_compatible_p(size_t,unsigned long), "size_t ABI");
_Static_assert(__builtin_types_compatible_p(pid_t,int), "pid_t ABI");
_Static_assert(__builtin_classify_type((pthread_t)0) == 5, "pthread_t must be a pointer");
int (*check_sigaction)(int,const struct sigaction *restrict,struct sigaction *restrict) = sigaction;
