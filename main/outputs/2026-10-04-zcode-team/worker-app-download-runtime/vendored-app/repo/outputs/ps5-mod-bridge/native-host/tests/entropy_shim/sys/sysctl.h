#ifndef MB_TEST_ENTROPY_SYSCTL_H
#define MB_TEST_ENTROPY_SYSCTL_H

#include <stddef.h>

/* The native observer is compiled unchanged against this test-only boundary. */
#define CTL_KERN 1
#define KERN_ARND 37

int sysctl(const int *name, unsigned int name_length, void *old_value,
           size_t *old_length, const void *new_value, size_t new_length);

#endif
