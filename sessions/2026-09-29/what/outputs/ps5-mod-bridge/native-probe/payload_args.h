/* SPDX-License-Identifier: GPL-3.0-or-later
 * Derived from ps5-payload-dev/sdk crt/payload.h, Copyright (C) 2024 John Törnblom.
 * Modified: standalone LP64 layout and compile-time checks; no runtime API.
 * The upstream public v0.43 header is preserved in third_party/sdk-public-payload.h.
 */
#ifndef MODBRIDGE_PAYLOAD_ARGS_H
#define MODBRIDGE_PAYLOAD_ARGS_H
typedef struct payload_args {
    int (*sys_dynlib_dlsym)(int, const char *, void *);
    int *rwpipe;
    int *rwpair;
    long kpipe_addr;
    long kdata_base_addr;
    int *payloadout;
} payload_args_t;
_Static_assert(sizeof(void *) == 8 && sizeof(long) == 8 && sizeof(int) == 4, "candidate ABI requires LP64");
_Static_assert(sizeof(payload_args_t) == 48, "unexpected candidate argument layout");
_Static_assert(__builtin_offsetof(payload_args_t, payloadout) == 40, "unexpected result slot");
#endif
