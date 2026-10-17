#include <stddef.h>

#ifndef MODBRIDGE_START_RESULT
#error MODBRIDGE_START_RESULT must be selected by the variant build.
#endif

_Static_assert(sizeof(size_t) == 8, "native PS5 size_t required");

__attribute__((visibility("default")))
int module_start(size_t argument_size, const void *arguments)
{
    (void)argument_size;
    (void)arguments;
    return MODBRIDGE_START_RESULT;
}
