#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
char* SystemNative_GetLine(FILE* stream)
{
    assert(stream != NULL);

    char* lineptr = NULL;
    size_t n = 0;
    ssize_t length = getline(&lineptr, &n, stream);
    
    return length >= 0 ? lineptr : NULL;
}
