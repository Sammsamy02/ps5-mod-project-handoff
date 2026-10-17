#define _POSIX_C_SOURCE 200809L
#include "ps5_entropy.h"
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/sysctl.h>

static void clear32(uint8_t output[32])
{
    volatile uint8_t *owned = output;
    for(size_t i = 0; i < 32; ++i)
        owned[i] = 0;
}

int mb_curl_ps5_entropy_read32_v1(void *context, uint8_t output[32])
{
    if(!output)
        return -1;
    if(context) {
        clear32(output);
        return -1;
    }
    int selector[2] = {CTL_KERN, KERN_ARND};
    size_t length = 32;
    errno = 0;
    int rc = sysctl(selector, 2, output, &length, NULL, 0);
    int api_errno = errno;
    size_t returned = length;
    if(rc == 0 && api_errno == 0 && returned == 32)
        return 0;
    clear32(output);
    return -1;
}
